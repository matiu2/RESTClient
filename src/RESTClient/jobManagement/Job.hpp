#pragma once

#include <RESTClient/http/MonitoredConnection.hpp>

#include <cassert>

namespace RESTClient {

using JobFunction =
    std::function<bool(const std::string &name, const std::string &hostname,
                       MonitoredConnection &server)>;

struct QueuedJob {
  std::string name;
  const std::string &hostname;
  JobFunction work;
  bool operator()(MonitoredConnection &server) const {
    assert(server.inUse());
    return work(name, hostname, server);
  }
};

} /* RESTClient */
