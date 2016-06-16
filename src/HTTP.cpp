#include "HTTP.hpp"

#include <sstream>

#include "HTTP_OutputToNet.hpp"
#include "HTTP_ReadReply.hpp"

#include "HTTP_CopyToCout.hpp"

#include "logger.hpp"

#include <boost/asio/connect.hpp>
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
  return OutputToNet<Connection>(connection, yield);
}

/// Adds the default HTTP headers to a request
void HTTP::addDefaultHeaders(HTTPRequest &request) {
  LOG_TRACE("addDefaultHeaders");
  // Host
  std::string *value = &request.headers["Host"];
  if (value->empty())
    *value = hostName;
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
void chunkedTransmit(filtering_ostream &transmitter, std::istream &data,
                     asio::yield_context yield) {
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

/// Transmits a body. If the body size is positive (not -1), it'll send it in
/// one go. A negative number indicates that we don't know the size, so it
/// should be sent in chunked transfer-encoding
void transmitBody(filtering_ostream &transmitter, HTTPRequest &request,
                  asio::yield_context yield) {
  // If we know the body size
  std::istream &body = request.body;
  if (request.body.size() >= 0)
    io::copy(body, transmitter);
  else
    chunkedTransmit(transmitter, body, yield);
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
  output.flush();

  HTTPResponse result;
  if (!filePath.empty())
    result.body.initWithFile(filePath);
  transmitBody(output, request, yield);

  readHTTPReply(result);

  return result;
}


void HTTP::readHTTPReply(HTTPResponse &result) {
  if (is_ssl)
    RESTClient::readHTTPReply(result, sslStream, yield, std::bind(&HTTP::close, this));
  else
    RESTClient::readHTTPReply(result, socket, yield, std::bind(&HTTP::close, this));
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

HTTP::HTTP(std::string hostName, asio::io_service &io_service,
           tcp::resolver &resolver, asio::yield_context yield, bool is_ssl)
    : hostName(hostName), io_service(io_service), resolver(resolver),
      yield(yield), is_ssl(is_ssl), ssl_context(ssl::context::sslv23),
      sslStream(io_service, ssl_context), socket(io_service) {
  ssl_context.set_default_verify_paths();
  sslStream.set_verify_mode(ssl::verify_peer);
  sslStream.set_verify_callback(ssl::rfc2818_verification(hostName));
}

HTTP::~HTTP() { close(); }

void HTTP::ensureConnection() {
  // Resolve if needed
  if (endpoints == decltype(endpoints)())
    endpoints =
        resolver.async_resolve({hostName, is_ssl ? "https" : "http"}, yield);
  // Connect if needed
  if (is_ssl) {
    if (!sslStream.lowest_layer().is_open()) {
      asio::async_connect(sslStream.lowest_layer(), endpoints, yield);
      // Perform SSL handshake and verify the remote host's
      // certificate.
      sslStream.async_handshake(decltype(sslStream)::client, yield);
    }
  } else {
    if (!socket.is_open())
      asio::async_connect(socket, endpoints, yield);
  }
  makeOutput();
}

HTTPResponse HTTP::get(std::string path) {
  HTTPRequest request("GET", path);
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
  output << verb << " " << path << " HTTP/1.1"
         << "\r\n";
  output << "Host: " << hostName << "\r\n";
  output << "Accept: */*"
         << "\r\n";
  output << "Accept-Encoding: gzip, deflate"
         << "\r\n";
  output << "TE: trailers"
         << "\r\n";
  if (data.size() > 0)
    output << "Content-Length: " << data.size() << "\r\n";
  output << "\r\n";
  HTTPResponse result;
  ensureConnection();
  output << data;
  readHTTPReply(result);
  return result;
}

HTTPResponse HTTP::put(const std::string path, std::string data) {
  HTTPRequest request("PUT", path, {}, data);
  return action(request);
}

HTTPResponse HTTP::post(const std::string path, std::string data) {
  return PUT_OR_POST("POST", path, data);
}

HTTPResponse HTTP::PUT_OR_POST_STREAM(std::string verb, std::string path,
                                      std::istream &data) {
  boost::asio::streambuf buf;
  std::ostream request(&buf);
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << verb << " " << path << " HTTP/1.1"
          << "\r\n";
  request << "Host: " << hostName << "\r\n";
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
  if (is_ssl)
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
  if (is_ssl)
    return sslStream.lowest_layer().is_open();
  else
    return socket.is_open();
}

void HTTP::close() {
  if (sslStream.lowest_layer().is_open()) {
    boost::system::error_code ec;
    sslStream.async_shutdown(yield[ec]);
    sslStream.lowest_layer().close();
    // Short read is not a real error. Everything else is.
    if (ec.category() == asio::error::get_ssl_category() &&
        ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ))
      return;
    if (ec.category() == boost::system::system_category() &&
        ec.value() == boost::system::errc::success)
      return;
    throw boost::system::system_error(ec);
  } else if (socket.is_open()) {
    socket.close();
  }
}

} /* RESTClient */

