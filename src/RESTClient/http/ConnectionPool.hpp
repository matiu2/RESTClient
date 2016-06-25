#pragma once
#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/base/logger.hpp>

#include <list>
#include <boost/algorithm/string/predicate.hpp>

namespace RESTClient {

/// A connection, with an inUse tag (so we can reuse the connection)
struct MonitoredConnection {
  bool inUse = false;
  RESTClient::HTTP connection;
};

/// A sentry that 'un-uses' the connection after we've done with it, so that
/// someone else can pick it up later
class ConnectionUseSentry {
private:
  bool iOwnIt = true;
  std::function<bool()> shouldClose;
  MonitoredConnection &_connection;

public:
  /// onDone will be called in the constructor
  ConnectionUseSentry(MonitoredConnection &_connection,
                      std::function<bool()> shouldClose)
      : shouldClose(shouldClose), _connection(_connection) {
    LOG_TRACE("ConnectionUseSentry constructor");
    assert(_connection.inUse == false);
    _connection.inUse = true;
  }
  ConnectionUseSentry(const ConnectionUseSentry &other) = delete;
  ConnectionUseSentry(ConnectionUseSentry &&other)
      : iOwnIt(true), _connection(other._connection) {
    LOG_TRACE("ConnectionUseSentry move constructor");
    other.iOwnIt = false;
  }
  ~ConnectionUseSentry() {
    LOG_TRACE("ConnectionUseSentry detstructor. iOwnIt = " << iOwnIt);
    if (iOwnIt)
      _connection.inUse = false;
    if (shouldClose())
      _connection.connection.close();
  }
  RESTClient::HTTP &connection() {
    LOG_TRACE("ConnectionUseSentry::connection()");
    return _connection.connection;
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
  ConnectionPool(std::string hostname) : hostname(hostname) {
    LOG_TRACE("ConnectionPool constructor: " << hostname);
  }

  ~ConnectionPool() {
    // The connections must be closed inside of a coroutine because there's some
    // time where it waits for the ssl shutdown
    if (!connections.empty()) {
      LOG_FATAL("Connections should be closed before the pool is shutdown. "
                "Call cleanup() first. We have "
                << connections.size() << " existing connections still");
    }
  }

  /// neededConnections is the foreseeable amount of connections we'll need in the future
  /// If it's below the amount in the pool, we'll close this connection after use
  ConnectionUseSentry getSentry(asio::yield_context yield, std::function<size_t()> getNeededConnections) {
    LOG_TRACE("ConnectionPool::getSentry: " << hostname);
    RESTClient::HTTP* result = nullptr;
    // Remove closed connections from the pool
    boost::remove_erase_if(connections, [](MonitoredConnection& connection) {
      connection.connection.is_open();
    };
    // Return if the sentry should close the connection when it's done
    auto shouldClose = [this, getNeededConnections]() {
      return getNeededConnections() < connections.size();
    };
    for (auto &result : connections) {
      if (!result->inUse)
        return ConnectionUseSentry(*result, shouldClose);
    }
    // If we get here, we couldn't find a connection, we'll just create one
    LOG_TRACE("ConnectionPool::getSentry .. creating connection: " << hostname);
    connections.emplace_back(new MonitoredConnection{false, {hostname, yield}});
    return ConnectionUseSentry(*connections.back(), shouldClose);
  }

  /// Close all connections
  void cleanup(asio::yield_context yield) {
    try {
      for (auto &connection : connections)
        connection->connection.close(yield);
      connections.clear();
    } catch (std::exception &e) {
      LOG_ERROR("Exception while closing connection pool: " << e.what());
    } catch (...) {
      LOG_ERROR("Unknown exception while closing connection pool");
    }
  }
};

} /* RESTClient */
