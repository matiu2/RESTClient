/// Keeps X amount of jobs running in a single thread using the boost asio job runner
#pragma once

#include <map>
#include <queue>
#include <string>

#include <RESTClient/base/logger.hpp>
#include <RESTClient/base/url.hpp>
#include <RESTClient/jobManagement/Job.hpp>
#include <RESTClient/http/Services.hpp>

namespace RESTClient {

using namespace boost;

/// Runs all the jobs for a certain hostname
class JobRunner {
public:
  using JobQueue = std::queue<QueuedJob>;
private:
  Services& services = Services::instance();
  // Map of hostname to job queue
  std::map<HostInfo, JobQueue> queues;
public:
  JobQueue &queue(const HostInfo &hostInfo);
  void startProcessing(size_t connectionsPerHost = 4);
};

} /* RESTClient */
