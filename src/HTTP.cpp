#include "HTTP.hpp"

#include <sstream>

#include <boost/algorithm/cxx11/copy_n.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
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

// This is the end line used in the HTTP protocol
const char* endl("\r\n");

template <typename Connection> struct TCPReader {
private:
  Connection &connection;
  asio::yield_context &yield;
  asio::streambuf raw_buf;
  boost::iostreams::filtering_streambuf<boost::iostreams::input> filtered_buf;
  std::streambuf *buf;

public:
  TCPReader(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield), raw_buf(), buf(&raw_buf) {}
  void line(std::string &result) {
    assert(buf);
    asio::async_read_until(connection, raw_buf, "\n", yield);
    size_t available = buf->in_avail();
    size_t count = 0;
    result.resize(0);
    result.reserve(available);
    while (count < available) {
      char c = buf->sbumpc();
      ++count;
      result.push_back(c);
      if (c == '\n')
        break;
    }
    boost::trim(result);
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < " << result << std::endl << std::flush;
#endif
  }
  size_t readAvailableBody(std::ostream &body) {
    // Fills our result.body with what ever's in the buffer
    assert(buf);
    size_t available = buf->in_avail();
    for (int i = 0; i < available; ++i) {
      char c = buf->sbumpc();
#ifdef HTTP_ON_STD_OUT
      std::cout.put(c);
      std::cout.flush();
#endif
      body.put(c);
    }
    return available;
  };
  size_t readAvailableBody(std::string &body) {
    // Fills our result.body with what ever's in the buffer
    assert(buf);
    size_t available = buf->in_avail();
    body.reserve(body.size() + available);
    for (size_t i = 0; i != available; ++i)
      body.push_back(buf->sbumpc());
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < " << body << std::flush;
#endif
    return available;
  };
  void readNBytes(std::ostream &destination, size_t toRead) {
    assert(buf);
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < ";
#endif
    size_t available = buf->in_avail();
    if (available >= toRead) {
      // If we want less than what we've already downleaded,
      // copy it out of the incoming buffer to the destination
      for (size_t i = 0; i != toRead; ++i) {
        char c = buf->sbumpc();
#ifdef HTTP_ON_STD_OUT
        std::cout << c << std::flush;
#endif
        destination.put(c);
      }
      return;
    } else {
      // If we need more than what we've already downloaded,
      // copy the whole buffer contents
      toRead -= readAvailableBody(destination);
      // Now get the rest into the buffer and read that
      if (toRead > 0) {
        waitForMoreBody(toRead);
        assert(buf->in_avail() >= toRead);
        for (size_t i = 0; i != toRead; ++i) {
          char c = buf->sbumpc();
#ifdef HTTP_ON_STD_OUT
          std::cout << c << std::flush;
#endif
          destination.put(c);
        }
      }
    }
  }
  void readNBytes(std::string &body, size_t toRead) {
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < ";
#endif
    body.reserve(body.size() + toRead);
    size_t available = buf->in_avail();
    if (available >= toRead) {
      for (size_t i = 0; i != toRead; ++i) {
        body.push_back(buf->sbumpc());
#ifdef HTTP_ON_STD_OUT
        std::cout << body.back() << std::flush;
#endif
      }
      return;
    } else {
      size_t startLen = body.size();
      readAvailableBody(body);
      toRead -= body.size() - startLen;
      // Now get the rest into the buffer and read that
      if (toRead > 0) {
        waitForMoreBody(toRead);
        assert(buf->in_avail() >= toRead);
        for (size_t i = 0; i != toRead; ++i) {
          body.push_back(buf->sbumpc());
#ifdef HTTP_ON_STD_OUT
          std::cout << body.back() << std::flush;
#endif
        }
      }
    }
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < " << body << std::flush;
#endif
  };
  void waitForMoreBody(size_t bytes) {
    auto bufs = raw_buf.prepare(bytes);
    size_t bytesRead = asio::async_read(connection, bufs,
                                        asio::transfer_exactly(bytes), yield);
    assert(bytesRead == bytes);
    raw_buf.commit(bytesRead);
    assert(raw_buf.in_avail() >= bytes);
  }
  /// Return at least 1 byte, but whatever we get
  size_t waitForAnyBody() {
    return asio::async_read(connection, raw_buf, asio::transfer_at_least(1),
                            yield);
  }
  void startGzipDecoding() {
    filtered_buf.push(boost::iostreams::gzip_decompressor());
    filtered_buf.push(*buf);
    buf = &filtered_buf;
  }
};

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

template <typename T>
void chunkedTransmit(T &transmitter, std::istream &data,
                     asio::yield_context yield) {
  // https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
  const size_t bufferSize = 1024;
  char buffer[bufferSize];
  while (data.good()) {
    size_t bytes = data.readsome(buffer, bufferSize);
    if (bytes == 0)
      break;
    // Write the chunksize
    std::stringstream chunkSize;
    chunkSize << std::ios_base::hex << bytes << endl;
    asio::async_write(transmitter, asio::buffer(chunkSize.str()), yield);
    // Write the data
    asio::async_write(transmitter, asio::buffer(buffer, bytes), yield);
    // Write a new line
    asio::async_write(transmitter, asio::buffer(endl, 2), yield);
  }
}

template <typename T>
void transmit(T &transmitter, std::istream &data, asio::yield_context yield) {
  const size_t bufferSize = 1024;
  char buffer[bufferSize];
  while (data.good()) {
    size_t bytes = data.readsome(buffer, bufferSize);
    if (bytes == 0)
      break;
    asio::async_write(transmitter, asio::buffer(buffer, bytes), yield);
  }
}

template <typename T>
void transmitBody(T &transmitter, HTTPRequest &request,
                  asio::yield_context yield) {
  std::istream &data = request.body;
  if (request.body.size() >= 0)
    transmit(transmitter, data, yield);
  else
    chunkedTransmit(transmitter, data, yield);
}

HTTPResponse HTTP::action(HTTPRequest& request, std::string filePath) {
  boost::asio::streambuf buf;
  std::ostream stream(&buf);
  addDefaultHeaders(request);
  stream << request.verb << " " << request.path << " HTTP/1.1" << endl;

  for (const auto &header : request.headers)
    stream << header.first << ": " << header.second << endl;
  stream << endl;

  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> ";
  for (auto& part : buf.data())
    std::cout.write(boost::asio::buffer_cast<const char *>(part),
                    boost::asio::buffer_size(part));
  #endif

  HTTPResponse result;
  if (!filePath.empty())
    result.body.initWithFile(filePath);
  ensureConnection();

  if (is_ssl) {
    asio::async_write(sslStream, buf, yield);
    transmitBody(sslStream, request, yield);
    readHTTPReply(*this, sslStream, result);
  } else {
    asio::async_write(socket, buf, yield);
    transmitBody(socket, request, yield);
    readHTTPReply(*this, socket, result);
  }
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

template <typename T>
void readHTTPReply(HTTP& http, T& connection, HTTPResponse& result) {
  TCPReader<T> reader(connection, http.yield);
  unsigned int contentLength = 0;
  // Copy the data into a line
  bool keepAlive = true; // All http 1.1 connections are keepalive unless
                         // they contain a 'Connection: close' header
  bool chunked = false;  // Chunked encoding (instead of contentLength)
  bool gzipped = false;  // incoming content is gzip encoded

  std::string input;
  reader.line(input);
  bool ok;
  std::tie(result.http_code, ok) = readFirstLine(input);
  if (!ok) {
    reader.readAvailableBody(input);
    throw HTTPError(result.http_code, input);
  }

  // Reads a header into key and value. Trims spaces off both sides
  std::string key;
  std::string value;
  auto readHeader = [&]() {
    // Get the next line
    reader.line(input);
    // If the line is empty, there are no more headers, go read the body
    if (input == "")
      return false;
    // Read the headers into key+value pairs
    auto colon = std::find(input.begin(), input.end(), ':');
    if (colon == input.end())
      throw std::runtime_error(std::string("Unable to read header (no colon): ") + input);
    key.reserve(std::distance(input.begin(), colon));
    value.reserve(std::distance(colon, input.end()));
    std::copy(input.begin(), colon, std::back_inserter(key));
    std::copy(colon + 1, input.end(), std::back_inserter(value));
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

  if (gzipped)
    reader.startGzipDecoding();

  std::string buffer;

  auto readData = [&reader, &buffer, &result](size_t n_bytes) {
    reader.readNBytes(result.body, n_bytes);
  };

  // If we have a contentLength, read that many bytes
  if (chunked) {
    // Read the chunk size;
    reader.line(input);
    unsigned long chunkSize = stoul(input, 0, 16);
    while (chunkSize != 0) {
      // TODO: maybe read 'extended chunk' data one day
      readData(chunkSize);
      std::string emptyLine;
      reader.readNBytes(emptyLine, 2); // Read the empty line at the end of the chunk
      if (emptyLine != endl)
        throw std::runtime_error("chunked encoding read error. Expected empty line.");
      reader.line(input); // Read the next chunk header
      chunkSize = stoul(input, 0, 16);
    }
    // Read extra headers if there are any
    readHeaders();
  } else  {
    // Copy the initial buffer contents to the body
    readData(contentLength);
  }
  // Close connection if that's what the server wants
  if (!keepAlive) {
    reader.readAvailableBody(result.body);
    http.close();
  }
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
  boost::asio::streambuf buf;
  std::ostream request(&buf);
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << verb << " " << path << " HTTP/1.1" << endl;
  request << "Host: " << hostName << endl;
  request << "Accept: */*" << endl;
  request << "Accept-Encoding: gzip, deflate" << endl;
  request << "TE: trailers" << endl;
  request << "Content-Length: " << data.size() << endl;
  request << endl;
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> ";
  for (auto& part : buf.data())
    std::cout.write(boost::asio::buffer_cast<const char *>(part),
                    boost::asio::buffer_size(part));
  #endif
  HTTPResponse result;
  ensureConnection();
  if (is_ssl) {
    asio::async_write(sslStream, buf, yield);
    asio::async_write(sslStream, asio::buffer(data), yield);
    readHTTPReply(*this, sslStream, result);
  } else {
    asio::async_write(socket, buf, yield);
    asio::async_write(socket, asio::buffer(data), yield);
    readHTTPReply(*this, socket, result);
  }
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
  request << verb << " " << path << " HTTP/1.1" << endl;
  request << "Host: " << hostName << endl;
  request << "Accept: */*" << endl;
  request << "Accept-Encoding: gzip, deflate" << endl;
  request << "TE: trailers" << endl;
  // Find the stream size
  data.seekg(0, std::istream::end);
  long size = data.tellg();
  data.seekg(0);
  if (size != -1)
    request << "Content-Length: " << size << endl;
  request << endl;
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> ";
  for (auto& part : buf.data())
    std::cout.write(boost::asio::buffer_cast<const char *>(part),
                    boost::asio::buffer_size(part));
  #endif
  HTTPResponse result;
  ensureConnection();
  if (is_ssl) {
    asio::async_write(sslStream, buf, yield);
    transmit(sslStream, data, yield);
    readHTTPReply(*this, sslStream, result);
  } else {
    asio::async_write(socket, buf, yield);
    transmit(socket, data, yield);
    readHTTPReply(*this, socket, result);
  }
  return result;
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

