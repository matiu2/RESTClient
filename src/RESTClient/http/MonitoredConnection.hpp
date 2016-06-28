#pragma once

#include <RESTClient/http/HTTP.hpp>

namespace RESTClient {
/// A connection, with an inUse tag (so we can reuse the connection)
/// It holds the yield context too and passes that to the HTTP instance that it
/// wraps
struct MonitoredConnection {
  asio::yield_context *yield = nullptr;
  // This is a shared_ptr because std::list demands that we be able to copy it
  std::shared_ptr<HTTP> connection;
  MonitoredConnection(std::string hostname)
      : connection(new HTTP(std::move(hostname))) {}
  void use(asio::yield_context &context) { yield = &context; }
  void release() { yield = nullptr; }
  bool inUse() const { return yield != nullptr; }
  // Mirror of normal HTTP connection methods starrts here ====
  HTTPResponse get(std::string path) {
    assert(yield);
    return connection->get(path, *yield);
  }
  HTTPResponse getToFile(std::string serverPath, const std::string &filePath) {
    assert(yield);
    return connection->getToFile(serverPath, *yield, filePath);
  }
  HTTPResponse del(std::string path) {
    assert(yield);
    return connection->del(std::move(path), *yield);
  }
  HTTPResponse put(std::string path, std::string data) {
    assert(yield);
    return connection->put(path, *yield, data);
  }
  HTTPResponse putStream(std::string path, std::istream &data) {
    assert(yield);
    return connection->putStream(path, *yield, data);
  }
  HTTPResponse post(std::string path, std::string data) {
    assert(yield);
    return connection->post(path, *yield, data);
  }
  HTTPResponse postStream(std::string path, std::istream &data) {
    assert(yield);
    return connection->postStream(path, *yield, data);
  }
  HTTPResponse patch(std::string path, std::string data) {
    assert(yield);
    return connection->patch(path, *yield, data);
  }
  /// Return true if the connection is open
  bool is_open() const { return connection->is_open(); }
  void close() {
    assert(yield);
    connection->close(*yield);
  }
  // Mirror of normal HTTP connection methods ends here ====
};

} /* RESTClient */
