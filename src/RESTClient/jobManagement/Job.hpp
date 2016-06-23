#pragma once

#include <RESTClient/http/HTTP.hpp>

namespace RESTClient {

using JobFunction =
    std::function<bool(const std::string &name, const std::string &hostname,
                       asio::yield_context yield, RESTClient::HTTP &server)>;

struct QueuedJob {
  std::string name;
  const std::string &hostname;
  JobFunction work;
  bool operator()(asio::yield_context yield, RESTClient::HTTP &server) const {
    return work(name, hostname, yield, server);
  }
};

} /* RESTClient */
