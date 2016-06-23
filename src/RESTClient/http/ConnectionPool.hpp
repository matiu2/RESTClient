#pragma once
#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/base/logger.hpp>

#include <vector>
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
  MonitoredConnection &_connection;

public:
  ConnectionUseSentry(MonitoredConnection &_connection)
      : _connection(_connection) {
    LOG_TRACE("ConnectionUseSentry constructor");
    assert(_connection.inUse == false);
    _connection.inUse = true;
  }
  ConnectionUseSentry(const ConnectionUseSentry &other) = delete;
  ConnectionUseSentry(ConnectionUseSentry &&other)
      : _connection(other._connection) {
    LOG_TRACE("ConnectionUseSentry move constructor");
    other.iOwnIt = false;
  }
  ~ConnectionUseSentry() {
    LOG_TRACE("ConnectionUseSentry detstructor. iOwnIt = " << iOwnIt);
    if (iOwnIt)
      _connection.inUse = false;
  }
  RESTClient::HTTP &connection() {
    LOG_TRACE("ConnectionUseSentry::connection()");
    return _connection.connection;
  }
};

class ConnectionPool {
private:
  // Connection pool, true means it's in use, false means it's available
  std::vector<MonitoredConnection> connections;
  const std::string hostname;
public: 
  /**
  * @param hostname must include the http:// or https://
  */
  ConnectionPool(std::string hostname) : hostname(hostname) {
    LOG_TRACE("ConnectionPool constructor: " << hostname);
  }

  ConnectionUseSentry getSentry(asio::yield_context yield) {
    LOG_TRACE("ConnectionPool::getSentry: " << hostname);
    RESTClient::HTTP* result = nullptr;
    for (MonitoredConnection& result : connections)
      if (!result.inUse)
        return ConnectionUseSentry(connections.back());
    // If we get here, we couldn't find a connection, we'll just create one
    LOG_TRACE("ConnectionPool::getSentry .. creating connection: " << hostname);
    MonitoredConnection mon{false, {hostname, yield}};
    connections.emplace_back(std::move(mon));
    return ConnectionUseSentry(connections.back());
  }
};

} /* RESTClient */
