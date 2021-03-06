#include "HTTP.hpp"

#include <sstream>

#include "HTTP_OutputToNet.hpp"
#include "HTTP_ReadReply.hpp"

#include "HTTP_CopyToCout.hpp"

#include <RESTClient/base/logger.hpp>

#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/algorithm/search.hpp>
#include <boost/range/istream_range.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/system/error_code.hpp>

#include <boost/asio/ssl/rfc2818_verification.hpp>

namespace RESTClient {

namespace io = boost::iostreams;

/// Convenience function to create an OutputToNet sink
template <typename Connection>
OutputToNet<Connection> make_output_to_net(Connection &connection,
                                           asio::yield_context &yield) {
  LOG_TRACE("make_output_to_net");
  return OutputToNet<Connection>(connection, yield);
}

/// Adds the default HTTP headers to a request
void HTTP::addDefaultHeaders(HTTPRequest &request) {
  LOG_TRACE("addDefaultHeaders");
  // Host
  std::string *value = &request.headers["Host"];
  if (value->empty())
    *value = hostInfo.hostname;
  // Accept */*
  value = &request.headers["Accept"];
  if (value->empty())
    *value = "*/*";
  // Accept-Encoding: gzip, deflate
  value = &request.headers["Accept-Encoding"];
  if (value->empty())
    *value = "gzip, deflate";
  // TE: trailers
  value = &request.headers["TE"];
  if (value->empty())
    *value = "trailers";
  // Content-Length
  long size = request.body.size();
  if (size >= 0) {
    value = &request.headers["Content-Length"];
    if (value->empty())
      *value = std::to_string(size);
  }
}

/// Transmits a body with 'chunked' transfer-encoding (untested because
/// httpbin.org doesn't support it, but cloud files API does, we'll have to
/// write some tests that connect to cloud files api later)
void chunkedTransmit(filtering_ostream &transmitter, std::istream &data) {
  // https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
  const size_t bufferSize = 4096;
  char buffer[bufferSize];
  while (data.good()) {
    data.read(buffer, bufferSize);
    size_t bytes = data.gcount();
    if (bytes == 0)
      break;
    // Write the chunksize
    transmitter << std::ios_base::hex << bytes << "\r\n";
    // Write the data
    io::copy(io::restrict(data, 0, bytes), transmitter);
    // Write a new line
    transmitter << "\r\n";
  }
}

/// Transmits a body. If the request.size (body size) is positive (not -1),
/// it'll send it in one go. A negative number indicates that we don't know the
/// size, so it should be sent in chunked transfer-encoding
void transmitBody(filtering_ostream &transmitter, HTTPRequest &request,
                  asio::yield_context &yield) {
  // If we know the body size
  std::istream &body = request.body;
  if (request.body.size() >= 0)
    io::copy(body, transmitter);
  else
    chunkedTransmit(transmitter, body);
}

HTTP::HTTP(const HostInfo &hostInfo, asio::yield_context yield)
    : hostInfo(hostInfo), services(Services::instance()), yield(yield),
      ssl_context(services.io_service, ssl::context::sslv23),
      sslStream(services.io_service, ssl_context), socket(services.io_service) {
  LOG_TRACE("HTTP constructor: " << hostInfo);
  // Set up
  ssl_context.set_default_verify_paths();
  sslStream.set_verify_mode(ssl::verify_peer);
  sslStream.set_verify_callback(ssl::rfc2818_verification(hostInfo.hostname));
}

HTTP::~HTTP() {
  const char *ending(" should have been closed before destruction");
  if (sslStream.lowest_layer().is_open()) {
    LOG_FATAL("HTTP SSL Connection to " << hostInfo.hostname << ending);
  }
  if (socket.is_open()) {
    LOG_FATAL("HTTP socket Connection to " << hostInfo.hostname << ending);
  }
}

/// Handles an HTTP action (verb) GET/POST/ etc..
HTTPResponse HTTP::action(HTTPRequest &request, std::string filePath) {
  ensureConnection();
  addDefaultHeaders(request);
  output << request.verb << " " << request.path << " HTTP/1.1"
         << "\r\n";

  for (const auto &header : request.headers)
    output << header.first << ": " << header.second << "\r\n";
  output << "\r\n";

  HTTPResponse result;
  if (!filePath.empty())
    result.body.initWithFile(filePath);
  transmitBody(output, request, yield);

  readHTTPReply(result);

  return result;
}

void HTTP::readHTTPReply(HTTPResponse &result) {
  if (hostInfo.is_ssl())
    RESTClient::readHTTPReply(result, yield, sslStream,
                              std::bind(&HTTP::close, this));
  else
    RESTClient::readHTTPReply(result, yield, socket,
                              std::bind(&HTTP::close, this));
}

std::string HTTPError::lookupCode(int code) {
  // TODO: Maybe translate the http error code into a useful message
  // Maybe copy how it's done in curlpp11 where you pass an error code callback
  // handler
  std::stringstream result;
  result << "Code: (" << code
         << "). We don't translate HTTP error codes yet. Look it up on "
            "https://en.wikipedia.org/wiki/List_of_HTTP_status_codes";
  return std::move(result.str());
}

void HTTP::ensureConnection() {
  // Resolve if needed
  tcp::resolver::iterator endpoints;
  if (endpoints == decltype(endpoints)()) {
    endpoints = services.resolver.async_resolve(
        {hostInfo.hostname, hostInfo.protocol}, yield);
  }
  // Connect if needed
  if (hostInfo.is_ssl()) {
    if (!sslStream.lowest_layer().is_open()) {
      asio::async_connect(sslStream.lowest_layer(), endpoints, yield);
      // Perform SSL handshake and verify the remote host's
      // certificate.
      sslStream.async_handshake(ssl::stream<tcp::socket>::client, yield);
    }
  } else {
    if (!socket.is_open())
      asio::async_connect(socket, endpoints, yield);
  }
  makeOutput();
}

HTTPResponse HTTP::get(std::string path, Headers headers) {
  HTTPRequest request("GET", path, headers);
  return action(request);
}

HTTPResponse HTTP::getToFile(std::string serverPath,
                             const std::string &filePath) {
  HTTPRequest request("GET", serverPath);
  return action(request, filePath);
}

HTTPResponse HTTP::del(std::string path) {
  HTTPRequest request("DELETE", path);
  return action(request);
}

HTTPResponse HTTP::PUT_OR_POST(std::string verb, std::string path,
                               std::string data) {
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  // TODO: use 'action' instead
  HTTPRequest request(verb, path, {}, data);
  return action(request);
}

HTTPResponse HTTP::put(const std::string path, std::string data) {
  LOG_TRACE("PUT " << path << " - " << data);
  HTTPRequest request("PUT", path, {}, data);
  return action(request);
}

HTTPResponse HTTP::post(const std::string path, std::string data) {
  LOG_TRACE("POST " << path << " - " << data);
  return PUT_OR_POST("POST", path, data);
}

HTTPResponse HTTP::PUT_OR_POST_STREAM(std::string verb, std::string path,
                                      std::istream &data) {
  // TODO: use 'action' instead
  boost::asio::streambuf buf;
  std::ostream request(&buf);
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << verb << " " << path << " HTTP/1.1"
          << "\r\n";
  request << "Host: " << hostInfo.hostname << "\r\n";
  request << "Accept: */*"
          << "\r\n";
  request << "Accept-Encoding: gzip, deflate"
          << "\r\n";
  request << "TE: trailers"
          << "\r\n";
  // Find the stream size
  data.seekg(0, std::istream::end);
  long size = data.tellg();
  data.seekg(0);
  if (size != -1)
    request << "Content-Length: " << size << "\r\n";
  request << "\r\n";
#ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> ";
  for (auto &part : buf.data())
    std::cout.write(boost::asio::buffer_cast<const char *>(part),
                    boost::asio::buffer_size(part));
#endif
  HTTPResponse result;
  ensureConnection();
  io::copy(buf, output);
  io::copy(data, output);
  readHTTPReply(result);
  return result;
}

void HTTP::makeOutput() {
  output.reset();
#ifdef HTTP_ON_STD_OUT
  output.push(CopyOutgoingToCout());
#endif
  if (hostInfo.is_ssl())
    output.push(make_output_to_net(sslStream, yield));
  else
    output.push(make_output_to_net(socket, yield));
}

HTTPResponse HTTP::putStream(std::string path, std::istream &data) {
  return PUT_OR_POST_STREAM("PUT", path, data);
}

HTTPResponse HTTP::postStream(std::string path, std::istream &data) {
  return PUT_OR_POST_STREAM("POST", path, data);
}

// HTTPResponse HTTP::patch(const std::string path, std::string data);

bool HTTP::is_open() const {
  if (hostInfo.is_ssl())
    return sslStream.lowest_layer().is_open();
  else
    return socket.is_open();
}

void HTTP::close() {
  if (sslStream.lowest_layer().is_open()) {
    boost::system::error_code ec;
    sslStream.async_shutdown(yield[ec]);
    sslStream.lowest_layer().close();
    LOG_DEBUG("SSH Shutdown 1: " << ec.category().name() << " - " << ec.value()
                                 << " - " << ec.category().message(ec.value()));
    using asio::error::misc_errors;
    using asio::error::basic_errors;
    const auto &misc_cat = asio::error::get_misc_category();
    const auto &ssl_cat = asio::error::get_ssl_category();
    // This error means the remote party has initiated has already closed the
    // underlying transport (TCP FIN) without shutting down the SSL.
    // It may be a truncate attack attempt, but nothing we can do about it
    // except close the connection.
    if (ec.category() == ssl_cat &&
        ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ)) {
      LOG_DEBUG("SSL Shutdown - remote party just dropped TCP FIN instead of "
                "closing SSL protocol. Possible truncate attack - closing "
                "connection.")
      return;
    }
    // We are the first one to run ssl_shutdown, and remote party responded in
    // kind, just continue
    if (ec.category() == misc_cat && ec.value() == misc_errors::eof) {
      return;
    }
    // The remote party sent ssl_shutdown, then just dropped the connection
    if (ec.category() == misc_cat &&
        ec.value() == basic_errors::operation_aborted) {
      return;
    }
    // Everything went as planned
    if (ec.category() == boost::system::system_category() &&
        ec.value() == boost::system::errc::success) {
      return;
    }
    // Something scary happened, log an error (throw an exception too)
    LOG_ERROR("Unabled to shutdown SSL connection: "
              << ec.category().name() << " (" << ec.value() << ") "
              << ec.category().message(ec.value()));
  } else if (socket.is_open()) {
    socket.close();
  }
}

} /* RESTClient */

