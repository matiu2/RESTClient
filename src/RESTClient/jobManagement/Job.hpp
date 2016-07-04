#pragma once

#include <RESTClient/http/HTTP.hpp>

#include <cassert>

namespace RESTClient {

using JobFunction = std::function<
    bool(const std::string &name, const std::string &hostname, HTTP &server)>;

struct QueuedJob {
  std::string name;
  const std::string &hostname;
  JobFunction work;
  bool operator()(HTTP &server) const {
    return work(name, hostname, server);
  }
};

} /* RESTClient */
