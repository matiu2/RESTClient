#pragma once

#include <RESTClient/base/url.hpp>
#include <RESTClient/http/HTTP.hpp>

#include <cassert>

namespace RESTClient {

using JobFunction = std::function<
    bool(const std::string &name, const HostInfo &hostname, HTTP &server)>;

struct QueuedJob {
  std::string name;
  const HostInfo& hostInfo;
  JobFunction work;
  bool operator()(HTTP &server) const {
    return work(name, hostInfo, server);
  }
};

} /* RESTClient */
