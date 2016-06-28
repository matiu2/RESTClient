/// Keeps X amount of jobs running in a single thread using the boost asio job runner
#pragma once

#include <queue>

#include <RESTClient/base/logger.hpp>
#include <RESTClient/jobManagement/Job.hpp>
#include <RESTClient/http/ConnectionPool.hpp>
#include <RESTClient/http/Services.hpp>

namespace RESTClient {

using namespace boost;

/// Runs all the jobs for a certain hostname
struct JobRunner {
  Services* services = Services::instance();
  std::string hostname;
  ConnectionPool connections;
  const size_t maxConcurrentJobs;
  size_t jobsRunningNow = 0;

  std::queue<QueuedJob> jobs;
  JobRunner(std::string hostname, size_t maxConcurrentJobs = 8);
  void addJob(std::string name, JobFunction job);
  void startJob();
  /// Starts a job if one is avalable and we are below maxConcurrentJobs
  void checkStartJob();
};

} /* RESTClient */
