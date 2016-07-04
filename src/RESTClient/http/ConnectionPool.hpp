#pragma once

#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/base/logger.hpp>
#include <RESTClient/http/MonitoredConnection.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <list>
#include <boost/algorithm/string/predicate.hpp>

namespace RESTClient {

/// A sentry that 'un-uses' the connection after we've done with it, so that
/// someone else can pick it up later
class ConnectionUseSentry {
private:
  bool iOwnIt = true;
  MonitoredConnection &monitoredConnection;

public:
  /// onDone will be called in the constructor
  ConnectionUseSentry(MonitoredConnection &monitoredConnection,
                      asio::yield_context &yield)
      : monitoredConnection(monitoredConnection) {
    LOG_TRACE("ConnectionUseSentry constructor");
    assert(monitoredConnection.inUse() == false);
    monitoredConnection.use(yield);
  }
  ConnectionUseSentry(const ConnectionUseSentry &other) = delete;
  ConnectionUseSentry(ConnectionUseSentry &&other)
      : iOwnIt(true), monitoredConnection(other.monitoredConnection) {
    LOG_TRACE("ConnectionUseSentry move constructor");
    other.iOwnIt = false;
  }
  ~ConnectionUseSentry() {
    LOG_TRACE("ConnectionUseSentry detstructor. iOwnIt = " << iOwnIt);
    if (iOwnIt)
      monitoredConnection.release();
  }
  MonitoredConnection &connection() {
    LOG_TRACE("ConnectionUseSentry::connection()");
    return monitoredConnection;
  }
};

class ConnectionPool {
private:
  // Connection pool, true means it's in use, false means it's available
  std::list<MonitoredConnection> connections;
  const std::string hostname;
public: 
  /**
  * @param hostname must include the http:// or https://
  */
  ConnectionPool(std::string hostname) : connections(), hostname(hostname) {
    LOG_TRACE("ConnectionPool constructor: " << hostname);
  }

  ~ConnectionPool() {
    // The connections must be closed inside of a coroutine because there's some
    // time where it waits for the ssl shutdown
    boost::remove_erase_if(connections, [](auto& conn) { return !conn.is_open(); });
    if (!connections.empty()) {
      LOG_FATAL("Connections should be closed before the pool is shutdown. "
                "Call cleanup() first. We have "
                << connections.size() << " existing connections still");
    }
  }

  /// neededConnections is the foreseeable amount of connections we'll need in
  /// the future
  /// If it's below the amount in the pool, we'll close this connection after
  /// use
  ConnectionUseSentry getSentry(asio::yield_context yield) {
    LOG_TRACE("ConnectionPool::getSentry: " << hostname);
    RESTClient::HTTP *result = nullptr;
    // Remove closed connections from the pool
    cleanup();
#ifndef NDEBUG
    for (auto& conn : connections)
      assert(conn.is_open());
#endif
    // Return if the sentry should close the connection when it's done
    for (auto &result : connections) {
      if (!result.inUse()) {
        LOG_TRACE("ConnectionPool::getSentry .. reusing connection: " << hostname);
        return ConnectionUseSentry(result, yield);
      }
    }
    // If we get here, we couldn't find a connection, we'll just create one
    LOG_TRACE("ConnectionPool::getSentry .. creating connection: "
              << hostname << " - " << connections.size());
    connections.emplace_back(MonitoredConnection(hostname));
    LOG_TRACE("ConnectionPool::getSentry .. created connection: "
              << hostname << " - " << connections.size());
    return ConnectionUseSentry(connections.back(), yield);
  }
  void cleanup() {
    // Remove closed connections from the pool
    LOG_TRACE("ConnectionPool::cleanup .. start: " << connections.size());
    boost::remove_erase_if(connections, [](auto& conn) { return !conn.is_open(); });
    LOG_TRACE("ConnectionPool::cleanup .. end: " << connections.size());
  }
};

} /* RESTClient */
