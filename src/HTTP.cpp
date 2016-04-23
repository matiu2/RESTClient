#include "HTTP.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/istream_range.hpp>

#include <sstream>
#ifdef HTTP_ON_STD_OUT
#include <iostream>
#endif

namespace RESTClient {

const char* endl("\r\n");

struct TCPReader {
  tcp::socket &socket;
  asio::yield_context &yield;
  asio::streambuf buf;
  std::istream data;
  TCPReader(tcp::socket &socket, asio::yield_context &yield)
      : socket(socket), yield(yield), buf(), data(&buf) {}
  void line(std::string& result) {
    asio::async_read_until(socket, buf, "\n", yield);
    getline(data, result);
    #ifdef HTTP_ON_STD_OUT
    std::cout << endl << " < " << result << endl << std::flush;
    #endif
  }
  template <typename T> void word(T &result) {
    asio::async_read_until(socket, buf, " ", yield);
    data >> result;
    #ifdef HTTP_ON_STD_OUT
    std::cout << endl << " < " << result << ' ' << std::flush;
    #endif
  }
  int readAvailableBody(std::string& body) {
    // Fills our result.body with what ever's in the buffer
    auto start = std::istreambuf_iterator<char>(&buf);
    decltype(start) end;
    int bytesRead = buf.in_avail();
    body.reserve(body.size() + buf.in_avail());
    std::copy(start, end, std::back_inserter(body));
    return bytesRead;
    #ifdef HTTP_ON_STD_OUT
    std::cout << endl << " < " << body << std::flush;
    #endif
  };
  int waitForMoreBody() { return asio::async_read(socket, buf, yield); }
};

HTTPResponse readHTTPReply(tcp::socket &socket, asio::yield_context &yield) {
  TCPReader reader(socket, yield);
  // Get the response
  HTTPResponse result;
  unsigned int contentLength = 0;
  // Copy the data into a line
  bool keepAlive = true; // All http 1.1 connections are keepalive unless
                         // they contain a 'Connection: close' header

  std::string input;
  // First line should be of the format: HTTP/1.1 200 OK
  reader.word(input); // Reads one word into input, should be HTTP/1.1
  if (input != "HTTP/1.1")
    throw std::runtime_error(
        std::string("Expected 'HTTP/1.1' in response but got: ") + input);
  // Read the return code
  reader.word(result.http_code);
  // Read OK
  reader.word(input);
  if (input != "OK") {
    reader.readAvailableBody(input);
    throw HTTPError(result.http_code, input);
  }
  // Second line should be empty
  reader.line(input);
  if (input != "\r")
    throw std::runtime_error("Expected second HTTP response line to be empty");

  // Reads a header into key and value. Trims spaces off both sides
  std::string key;
  std::string value;
  auto readHeader = [&]() {
    // Get the next line
    reader.line(input);
    // If the line is empty, there are no more headers, go read the body
    if (input != "\r")
      return false;
    // Check headers we care about
    auto colon = std::find(input.begin(), input.end(), ':');
    if (colon == input.end())
      throw std::runtime_error(std::string("Unable to read header (no colon): ") + input);
    key.reserve(std::distance(input.begin(), colon));
    value.reserve(std::distance(colon, input.end()));
    std::copy(input.begin(), colon, std::back_inserter(key));
    std::copy(colon, input.end(), std::back_inserter(value));
    boost::trim(key);
    boost::trim(value);
    return true;
  };

  // Now get the rest of the headers
  while (true) {
    if (!readHeader())
      break;
    // TODO: support chunked encoding
    if (key == "Content-Length")
      contentLength = std::stoi(value);
    else if (key == "Connection")
      if (value == "close")
        keepAlive = false;
    // TODO: check for gzip in response headers
    // Store the headers
    result.headers.emplace(std::move(key), std::move(value));
  }

  // Now read the body

  // If we have a contentLength, read that many bytes
  if (contentLength != 0) {
    // Copy the initial buffer contents to the body
    int bytes = 0; // We're counting body bytes now
    bytes += reader.readAvailableBody(result.body);
    while (bytes < contentLength) {
      // Fill the buffer
      bytes += reader.waitForMoreBody();
      // Copy that into the body
      reader.readAvailableBody(result.body);
    }
  } else if (!keepAlive) {
    // No content length, but not connection keepAlive, just read whatever we
    // can
    // TODO: This will be 'chunked reading'
    reader.readAvailableBody(result.body);
    while (socket.is_open()) {
      // Read whatever's available
      reader.waitForMoreBody();
      // Copy that into the body
      reader.readAvailableBody(result.body);
    }
  }
  return result;
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
           tcp::resolver &resolver, asio::yield_context yield, bool ssl)
    : hostName(hostName), io_service(io_service), resolver(resolver),
      yield(yield), socket(io_service), ssl(ssl) {}

HTTP::~HTTP() { socket.close(); }

HTTPResponse HTTP::get(const std::string path) {
  // Resolve if needed
  if (endpoints == decltype(endpoints)())
    endpoints =
        resolver.async_resolve({hostName, ssl ? "https" : "http"}, yield);
  // Connect if needed
  if (!socket.is_open())
    asio::async_connect(socket, endpoints, yield);
  std::stringstream request;
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << "GET " << path << " HTTP/1.1" << endl;
  request << "Host: " << hostName << endl;
  request << "Accept: */*" << endl;
  //request << "Accept-Encoding: gzip, deflate" << endl;
  request << endl;
  using namespace std;
  #ifdef HTTP_ON_STD_OUT
  cout << endl << "> " << request.str();
  #endif
  asio::async_write(socket, asio::buffer(request.str()), yield);
  return readHTTPReply(socket, yield);
}

//HTTPResponse HTTP::getToFile(std::string serverPath, const std::string &filePath);
//HTTPResponse HTTP::del(const std::string path);
//HTTPResponse HTTP::put(const std::string path, std::string data);
//HTTPResponse HTTP::post(const std::string path, std::string data);
//HTTPResponse HTTP::postFromFile(const std::string path, const std::string &filePath);
//HTTPResponse HTTP::patch(const std::string path, std::string data);

} /* RESTClient */

