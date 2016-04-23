#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>

namespace RESTClient {

/// Response from an HTTP request
struct HTTPResponse {
  int http_code;
  std::map<std::string, std::string> headers;
  std::string body;
};

/// Thrown when HTTP returns an error code
class HTTPError : public std::runtime_error {
private:
  static std::string lookupCode(int code);
public:
  HTTPError(int code, std::string msg = "")
      : std::runtime_error(lookupCode(code) + (msg.empty() ? "" : (" - " + msg))),
        code(code) {}
  int code;
};

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

// Handle an HTTP connection
class HTTP {
private:
  std::string hostName;
  asio::io_service &io_service;
  tcp::resolver &resolver;
  asio::yield_context yield;
  tcp::socket socket;
  bool ssl;
  tcp::resolver::iterator endpoints;
public:
  HTTP(std::string hostName, asio::io_service &io_service,
       tcp::resolver &resolver, asio::yield_context yield, bool ssl = true);
  ~HTTP();
  // Get a resource from the server. Path is the part after the URL.
  // eg. get("/person/1"); would get http://httpbin.org/person/1
  HTTPResponse get(const std::string path);
  HTTPResponse getToFile(std::string serverPath, const std::string& filePath);
  HTTPResponse del(const std::string path);
  HTTPResponse put(const std::string path, std::string data);
  HTTPResponse post(const std::string path, std::string data);
  HTTPResponse postFromFile(const std::string path, const std::string& filePath);
  HTTPResponse patch(const std::string path, std::string data);
};

  
} /* HTTP */
