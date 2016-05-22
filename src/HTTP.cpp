#include "HTTP.hpp"

#include <sstream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/search.hpp>
#include <boost/range/istream_range.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/system/error_code.hpp>
 

#include <boost/asio/ssl/rfc2818_verification.hpp>

#ifdef HTTP_ON_STD_OUT
#include <iostream>
#endif

namespace RESTClient {

namespace io = boost::iostreams;

// This is a boost iostreams source
template <typename Connection>
class NetReader : public io::source {
private:
  Connection &connection;
  asio::yield_context &yield;

public:
  NetReader(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield) {}
  std::streamsize read(char *s, std::streamsize n) {
    return asio::async_read(connection, asio::buffer(s, n),
                            asio::transfer_at_least(1), yield);
  }
};

template <typename Connection> class NetBlockReader : public io::source {
private:
  Connection &connection;
  asio::yield_context &yield;
  size_t &bytes_to_read;
  std::string starter;

public:
  NetBlockReader(Connection &connection, asio::yield_context &yield,
                 size_t &bytes_to_read, std::string &&starter)
      : connection(connection), yield(yield), bytes_to_read(bytes_to_read),
        starter(std::move(starter)) {}
  std::streamsize read(char *s, std::streamsize n) {

    if (bytes_to_read == 0)
      return 0;

    // Copy the starter bit if  there is any
    if (starter.length()) {
      boost::range::copy(starter, s);
      size_t ln = starter.length();
      starter.clear();
      bytes_to_read -= ln;
      return ln;
    }

    // Read the rest from the net
    size_t bytes_read =
        asio::async_read(connection, asio::buffer(s, n),
                         asio::transfer_exactly(bytes_to_read), yield);
    assert(bytes_read == bytes_to_read);
    bytes_to_read = 0;
    return bytes_read;
  }
};

template <typename Connection>
NetReader<Connection> makeNetReader(Connection &connection,
                                    asio::yield_context &yield) {
  return NetReader<Connection>(connection, yield);
}

template <typename Connection>
NetBlockReader<Connection>
makeNetBlockReader(Connection &connection, asio::yield_context &yield,
                   size_t &bytes_to_read, std::string &&leftOver) {
  return NetBlockReader<Connection>(connection, yield, bytes_to_read,
                                    std::move(leftOver));
}

#ifdef HTTP_ON_STD_OUT
class CopyIncomingToCout {
public:
    typedef char              char_type;
    typedef io::input_filter_tag  category;

    bool first = true; // Is this the first char in a line ?

    template<typename Source>
    int get(Source& src) {
      char_type c = io::get(src);
      if (first)
        std::cout << "> ";
      std::cout.put(c);
      first = (c == '\n');
      return c;
    }
};

class CopyOutgoingToCout {
public:
    typedef char              char_type;
    typedef io::output_filter_tag  category;

    bool first = true; // Is this the first char in a line ?

    template <typename Sink> bool put(Sink &dest, int c) {
      if (first)
        std::cout << "< ";
      std::cout.put(c);
      first = (c == '\n');
      io::put(dest, c);
      return true;
    }
};
#endif

// This is a boost iostreams sync
template <typename Connection>
class OutputToNet : public io::sink {
private:
  Connection &connection;
  asio::yield_context &yield;
public:
  OutputToNet(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield) {}
  std::streamsize write(const char* s, std::streamsize n) {
    return asio::async_write(connection, asio::buffer(s, n),
                             asio::transfer_at_least(n), yield);
  }
};

template <typename Connection>
OutputToNet<Connection> make_output_to_net(Connection &connection,
                                           asio::yield_context &yield) {
  return OutputToNet<Connection>(connection, yield);
}


void HTTP::addDefaultHeaders(HTTPRequest& request) {
  // Host
  std::string* value = &request.headers["Host"];
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

void chunkedTransmit(filtering_ostream &transmitter, std::istream &data,
                     asio::yield_context yield) {
  // https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
  const size_t bufferSize = 1024;
  char buffer[bufferSize];
  while (data.good()) {
    size_t bytes = data.readsome(buffer, bufferSize);
    if (bytes == 0)
      break;
    // Write the chunksize
    transmitter << std::ios_base::hex << bytes << "\r\n";
    // Write the data
    namespace s = io;
    s::copy(s::restrict(data, 0, bytes), transmitter);
    // Write a new line
    transmitter << "\r\n";
  }
}

void transmitBody(filtering_ostream &transmitter, HTTPRequest &request,
                  asio::yield_context yield) {
  // If we know the body size
  std::istream& body = request.body;
  if (request.body.size() >= 0)
    io::copy(body, transmitter);
  else
    chunkedTransmit(transmitter, body, yield);
}

HTTPResponse HTTP::action(HTTPRequest& request, std::string filePath) {
  ensureConnection();
  addDefaultHeaders(request);
  output << request.verb << " " << request.path << " HTTP/1.1" << "\r\n";

  for (const auto &header : request.headers)
    output << header.first << ": " << header.second << "\r\n";
  output << "\r\n";
  output.flush();

  HTTPResponse result;
  if (!filePath.empty())
    result.body.initWithFile(filePath);
  transmitBody(output, request, yield);
  readHTTPReply(input, result);

  return result;
}

/// Reads the first line of the HTTP reply.
/// Returns the HTTP response/error code, and 'true' if it has 'OK' at the end
/// of the reply.
/// throws std::runtime_error if there aren't three words in the reply.
std::pair<int, bool> readFirstLine(const std::string& line) {
  // First line should be of the format: HTTP/1.1 200 OK
  std::vector<boost::iterator_range<std::string::const_iterator>> words;
  boost::split(words, line, boost::is_any_of(" \t"));
  if (words.size() != 3)
    throw std::runtime_error(
        std::string("Expected 'HTTP/1.1 200 OK' but got '") + line + "'");
  auto word = words.begin();
  if (*word != std::string("HTTP/1.1"))
    throw std::runtime_error(
        std::string("Expected 'HTTP/1.1' in response but got: ") + line);
  // Read the return code and 'OK'
  ++word;
  int code = boost::lexical_cast<int>(*word);
  ++word;
  bool ok = *word == std::string("OK");
  return {code, ok};
}

std::string HTTPError::lookupCode(int code) {
  // TODO: Maybe translate the http error code into a useful message
  // Maybe copy how it's done in curlpp11 where you pass an error code callback
  // handler
  std::stringstream result;
  result << "Code: (" << code << "). We don't translate HTTP error codes yet. Look it up on "
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

HTTP::~HTTP() {
  close();
}

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
  makeInput();
  makeOutput();
}

HTTPResponse HTTP::get(std::string path) {
  HTTPRequest request("GET", path);
  return action(request);
}

HTTPResponse HTTP::getToFile(std::string serverPath, const std::string &filePath) {
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
  output << verb << " " << path << " HTTP/1.1" << "\r\n";
  output << "Host: " << hostName << "\r\n";
  output << "Accept: */*" << "\r\n";
  output << "Accept-Encoding: gzip, deflate" << "\r\n";
  output << "TE: trailers" << "\r\n";
  if (data.size() > 0)
    output << "Content-Length: " << data.size() << "\r\n";
  output << "\r\n";
  HTTPResponse result;
  ensureConnection();
  output << data;
  readHTTPReply(input, result);
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
  request << verb << " " << path << " HTTP/1.1" << "\r\n";
  request << "Host: " << hostName << "\r\n";
  request << "Accept: */*" << "\r\n";
  request << "Accept-Encoding: gzip, deflate" << "\r\n";
  request << "TE: trailers" << "\r\n";
  // Find the stream size
  data.seekg(0, std::istream::end);
  long size = data.tellg();
  data.seekg(0);
  if (size != -1)
    request << "Content-Length: " << size << "\r\n";
  request << "\r\n";
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> ";
  for (auto& part : buf.data())
    std::cout.write(boost::asio::buffer_cast<const char *>(part),
                    boost::asio::buffer_size(part));
  #endif
  HTTPResponse result;
  ensureConnection();
  io::copy(buf, output);
  io::copy(data, output);
  readHTTPReply(input, result);
  return result;
}

struct CharBufSource : public io::source {
    
  std::vector<char> &data;
  decltype(data.begin()) pos;

  CharBufSource(std::vector<char> &data) : data(data), pos(data.begin()) {}

  std::streamsize read(char *s, std::streamsize n) {
    size_t toRead = std::min(n, data.end() - pos);
    std::copy(pos, pos + toRead, s);
    return toRead;
  }

};

void HTTP::readHTTPReply(filtering_istream &input, HTTPResponse &result) {
  size_t contentLength = 0;
  // Copy the data into a line
  bool keepAlive = true; // All http 1.1 connections are keepalive unless
                         // they contain a 'Connection: close' header
  bool chunked = false;  // Chunked encoding (instead of contentLength)
  bool gzipped = false;  // incoming content is gzip encoded

  std::string line;
  auto getLine = [&input, &line ]() -> std::string & {
    std::getline(input, line);
    line.pop_back(); // Strip off the '\r' on the end
    return line;
  };
  bool ok;
  std::tie(result.http_code, ok) = readFirstLine(getLine());

  // Reads a header into key and value. Trims spaces off both sides
  std::string key;
  std::string value;
  auto readHeader = [&]() {
    // Get the next line
    getLine();
    // If the line is empty, there are no more headers, go read the body
    if (line == "")
      return false;
    // Read the headers into key+value pairs
    auto colon = std::find(line.begin(), line.end(), ':');
    if (colon == line.end())
      throw std::runtime_error(std::string("Unable to read header (no colon): ") + line);
    key.reserve(std::distance(line.begin(), colon));
    value.reserve(std::distance(colon, line.end()));
    std::copy(line.begin(), colon, std::back_inserter(key));
    std::copy(colon + 1, line.end(), std::back_inserter(value));
    boost::trim(key);
    boost::trim(value);
    return true;
  };

  // Reads the headers into the result
  auto readHeaders = [&]() {
    while (true) {
      if (!readHeader())
        break;
      // Check for headers we care about for the transfer
      if (key == "Content-Length")
        contentLength = std::stoi(value);
      else if ((key == "Connection") && (value == "close"))
        keepAlive = false;
      else if ((key == "Transfer-Encoding") && (value == "chunked"))
        chunked = true;
      else if ((key == "Content-Encoding") && (value == "gzip"))
        gzipped = true;
      // Store the headers
      result.headers.emplace(std::move(key), std::move(value));
    }
  };

  readHeaders();

  // Steal any left over buffered input
  assert(input.rdbuf());
  std::string leftOver;
  auto buf = input.rdbuf();
  size_t toRead = buf->in_avail();
  leftOver.reserve(toRead);
  for (size_t i = 0; i<toRead; ++i)
    leftOver.push_back(buf->sbumpc());
  assert(buf->in_avail() == 0);

  // Make a body reader
  io::filtering_istream bodyReader;
#ifdef HTTP_ON_STD_OUT
  bodyReader.push(CopyIncomingToCout());
#endif
  if (gzipped)
    bodyReader.push(io::gzip_decompressor());
  // Because there's no 'swap' in boost iostream stream buf :( .. we copy the
  // left over buffer into the body
  if (is_ssl)
    bodyReader.push(makeNetBlockReader(sslStream, yield, contentLength,
                                       std::move(leftOver)));
  else
    bodyReader.push(
        makeNetBlockReader(socket, yield, contentLength, std::move(leftOver)));

  std::ostream& body = result.body;
  // If we have a contentLength, read that many bytes
  if (chunked) {
    // Read the chunk size;
    getLine();
    contentLength = stoul(line, 0, 16);
    while (contentLength != 0) {
      // TODO: maybe read 'extended chunk' data one day
      io::copy(bodyReader, body);
      std::string emptyLine;
      std::getline(bodyReader, emptyLine);
      if (emptyLine != "\r")
        throw std::runtime_error("chunked encoding read error. Expected empty line.");
      getLine(); // Read the next chunk header
      contentLength = std::stoul(line, 0, 16);
    }
    // Read extra headers if there are any
    readHeaders();
  } else  {
    // Copy the initial buffer contents to the body
    io::copy(bodyReader, body);
  }
  // Close connection if that's what the server wants
  if (!keepAlive)
    close();
  // If the result was bad
  if (!ok)
    throw HTTPError(result.http_code, result.body);
}

void HTTP::makeInput() {
  input.reset();
#ifdef HTTP_ON_STD_OUT
  input.push(CopyIncomingToCout());
#endif
  if (is_ssl)
    input.push(makeNetReader(sslStream, yield));
  else
    input.push(makeNetReader(socket, yield));
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

HTTPResponse HTTP::putStream(std::string path, std::istream& data) {
  return PUT_OR_POST_STREAM("PUT", path, data);
}

HTTPResponse HTTP::postStream(std::string path, std::istream& data) {
  return PUT_OR_POST_STREAM("POST", path, data);
}

//HTTPResponse HTTP::patch(const std::string path, std::string data);

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

