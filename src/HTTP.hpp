#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <fstream>

#include "HTTPResponse.hpp"
#include "HTTPRequest.hpp"

namespace RESTClient {

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
namespace ssl = boost::asio::ssl;

class HTTP;

template <typename T>
HTTPResponse readHTTPReply(HTTP& http, T& connection);

// Handle an HTTP connection
class HTTP {
private:
  template <typename T>
  friend void readHTTPReply(HTTP& http, T& connection, HTTPResponse& result);
  std::string hostName;
  asio::io_service &io_service;
  tcp::resolver &resolver;
  asio::yield_context yield;
  bool is_ssl;
  ssl::context ssl_context;
  ssl::stream<tcp::socket> sslStream;
  tcp::socket socket;
  tcp::resolver::iterator endpoints;
  void ensureConnection();
public:
  HTTP(std::string hostName, asio::io_service &io_service,
       tcp::resolver &resolver, asio::yield_context yield, bool is_ssl=true);
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
  bool is_open() const; // Return true if the connection is open
  void close(); // Close the connection
};

} /* HTTP */
