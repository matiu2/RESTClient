#pragma once
/// Distributes jobs based on their host names to Job Runners

#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/jobManagement/Job.hpp>
#include <RESTClient/jobManagement/JobRunner.hpp>

#include <boost/asio/io_service.hpp>

namespace RESTClient {

class JobDistributor {
private:
  // How many jobs to run per host name
  const size_t maxConcurrentJobsPerHost;
  // map of hostname to jobRunner
  std::map<std::string, JobRunner> runners;
public:
  JobDistributor(size_t maxConcurrentJobsPerHost = 4);
  void addJob(std::string name, std::string hostname, JobFunction job);
};
  
  
} /* RESTClient */
