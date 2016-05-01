#include "HTTP.hpp"

#include <sstream>

#include <boost/asio/completion_condition.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/search.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/istream_range.hpp>
#include <boost/algorithm/cxx11/copy_n.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/istream_range.hpp>

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
  asio::streambuf buf;
public:
  TCPReader(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield), buf() {
  }
  void line(std::string &result) {
    asio::async_read_until(connection, buf, "\n", yield);
    size_t available = buf.in_avail();
    size_t count = 0;
    result.resize(0);
    result.reserve(available);
    while (count < available) {
      char c = buf.sbumpc();
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
  void readAvailableBody(std::string &body) {
    // Fills our result.body with what ever's in the buffer
    size_t available = buf.in_avail();
    body.reserve(body.size() + available);
    for (size_t i = 0; i != available; ++i)
      body.push_back(buf.sbumpc());
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < " << body << std::flush;
#endif
  };
  void readNBytes(std::string &body, size_t toRead) {
#ifdef HTTP_ON_STD_OUT
    std::cout << std::endl << " < ";
#endif
    body.reserve(body.size() + toRead);
    size_t available = buf.in_avail();
    if (available >= toRead) {
      for (size_t i=0; i!=toRead; ++i) {
        body.push_back(buf.sbumpc());
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
        assert(buf.in_avail() >= toRead);
        for (size_t i=0; i!=toRead; ++i) {
          body.push_back(buf.sbumpc());
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
    auto bufs = buf.prepare(bytes);
    size_t bytesRead =
        asio::async_read(connection, bufs, asio::transfer_exactly(bytes), yield);
    assert(bytesRead == bytes);
    buf.commit(bytesRead);
    assert(buf.in_avail() >= bytes);
  }
};

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
      // TODO: check for gzip in response headers
      // Store the headers
      result.headers.emplace(std::move(key), std::move(value));
    }
  };

  readHeaders();

  std::string buffer;

  auto readData = [&reader, &buffer, &result](size_t n_bytes) {
    std::string *dest = result.body.asString();
    if (dest) {
      reader.readNBytes(*dest, n_bytes);
    } else {
      buffer.resize(0);
      reader.readNBytes(buffer, n_bytes);
      result.body.consumeData(buffer);
    }
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
    std::string* destination = result.body.asString();
    if (destination)
      reader.readAvailableBody(*destination);
    else {
      buffer.resize(0);
      reader.readAvailableBody(buffer);
      result.body.consumeData(buffer);
    }
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

HTTPResponse HTTP::get(const std::string path) {
  std::stringstream request;
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << "GET " << path << " HTTP/1.1" << endl;
  request << "Host: " << hostName << endl;
  request << "Accept: */*" << endl;
  request << "Accept-Encoding: gzip, deflate" << endl;
  request << "TE: trailers" << endl;
  request << endl;
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> " << request.str();
  #endif
  HTTPResponse result;
  ensureConnection();
  if (is_ssl) {
    asio::async_write(sslStream, asio::buffer(request.str()), yield);
    readHTTPReply(*this, sslStream, result);
  } else {
    asio::async_write(socket, asio::buffer(request.str()), yield);
    readHTTPReply(*this, socket, result);
  }
  return result;
}

HTTPResponse HTTP::getToFile(std::string serverPath, const std::string &filePath) {
  std::stringstream request;
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << "GET " << serverPath << " HTTP/1.1" << endl;
  request << "Host: " << hostName << endl;
  request << "Accept: */*" << endl;
  request << "Accept-Encoding: gzip, deflate" << endl;
  request << "TE: trailers" << endl;
  request << endl;
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> " << request.str();
  #endif
  HTTPResponse result;
  result.body.initWithFile(filePath);
  ensureConnection();
  if (is_ssl) {
    asio::async_write(sslStream, asio::buffer(request.str()), yield);
    readHTTPReply(*this, sslStream, result);
  } else {
    asio::async_write(socket, asio::buffer(request.str()), yield);
    readHTTPReply(*this, socket, result);
  }
  return result;
}

HTTPResponse HTTP::del(const std::string path) {
  std::stringstream request;
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << "DELETE " << path << " HTTP/1.1" << endl;
  request << "Host: " << hostName << endl;
  request << "Accept: */*" << endl;
  request << "Accept-Encoding: gzip, deflate" << endl;
  request << "TE: trailers" << endl;
  request << endl;
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> " << request.str();
  #endif
  HTTPResponse result;
  ensureConnection();
  if (is_ssl) {
    asio::async_write(sslStream, asio::buffer(request.str()), yield);
    readHTTPReply(*this, sslStream, result);
  } else {
    asio::async_write(socket, asio::buffer(request.str()), yield);
    readHTTPReply(*this, socket, result);
  }
  return result;
}

//HTTPResponse HTTP::put(const std::string path, std::string data);
//HTTPResponse HTTP::post(const std::string path, std::string data);
//HTTPResponse HTTP::postFromFile(const std::string path, const std::string &filePath);
//HTTPResponse HTTP::patch(const std::string path, std::string data);

bool HTTP::is_open() const {
  if (is_ssl)
    return sslStream.lowest_layer().is_open();
  else
    return socket.is_open();
}

void HTTP::close() {
  if (socket.is_open()) {
    socket.close();
  }
  if (sslStream.lowest_layer().is_open()) {
    boost::system::error_code ec;
    sslStream.async_shutdown(yield[ec]);
    sslStream.lowest_layer().close();
    // Short read is not a real error. Everything else is.
    if (ec.category() != asio::error::get_ssl_category() ||
        ec.value() != ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ))
      throw boost::system::system_error(ec);
  }
}

} /* RESTClient */

