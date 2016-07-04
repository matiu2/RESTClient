#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include <fstream>

#include <RESTClient/http/Services.hpp>
#include <RESTClient/http/HTTPResponse.hpp>
#include <RESTClient/http/HTTPRequest.hpp>

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
using boost::iostreams::filtering_istream;
using boost::iostreams::filtering_ostream;

// Handle an HTTP connection
class HTTP {
private:
  std::string hostname;
  Services& services;
  asio::yield_context yield;
  bool is_ssl;
  ssl::context ssl_context;
  // Needs to be a unique_ptr, because ssl::stream has no copy and no move
  // powerz, but we want to be able to move HTTP instances
  ssl::stream<tcp::socket> sslStream;
  tcp::socket socket;
  filtering_ostream output;
  size_t incomingByteCounter = 0;
  void ensureConnection();
  void readHTTPReply(HTTPResponse &result);
  HTTPResponse PUT_OR_POST(std::string verb, std::string path,
                           std::string data);
  HTTPResponse PUT_OR_POST_STREAM(std::string verb,
                                  std::string path, std::istream &data);
  void makeOutput();

public:
  HTTP(const std::string &hostname, asio::yield_context yield);
  HTTP(const HTTP&) = delete;
  ~HTTP();

  HTTP &operator=(HTTP &&other);
  /// Modifies 'request' to have our default headers (won't change headers that
  /// are already in place)
  void addDefaultHeaders(HTTPRequest &request);
  /// Perform an HTTP action (this is a catch all)
  /// request headers may be modified to add the defaults
  /// By default will read the response to a string, but if you specify
  /// 'filePath' it'll save it to a file
  HTTPResponse action(HTTPRequest& request, std::string filePath="");
  // Get a resource from the server. Path is the part after the URL.
  // eg. get("/person/1"); would get http://httpbin.org/person/1
  HTTPResponse get(std::string path);
  HTTPResponse getToFile(std::string serverPath, const std::string& filePath);
  HTTPResponse del(std::string path);
  HTTPResponse put(std::string path, std::string data);
  HTTPResponse putStream(std::string path, std::istream& data);
  HTTPResponse post(std::string path, std::string data);
  HTTPResponse postStream(std::string path, std::istream& data);
  HTTPResponse patch(std::string path, std::string data);
  bool is_open() const; // Return true if the connection is open
  /// WARNING: This is the only blocking function, and must be called before
  /// shutting down. It'll wait for the SSL shutdown procedure
  void close();
};

} /* HTTP */
