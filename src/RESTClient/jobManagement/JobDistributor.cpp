#include "JobDistributor.hpp"

#include <RESTClient/base/logger.hpp>

#include <cassert>

namespace RESTClient {

JobDistributor::JobDistributor(size_t maxConcurrentJobsPerHost)
    : maxConcurrentJobsPerHost(maxConcurrentJobsPerHost) {
  LOG_TRACE("JobDistributor constructor: " << maxConcurrentJobsPerHost);
}

void JobDistributor::addJob(std::string name, std::string hostname,
                            JobFunction job) {
  LOG_TRACE("JobDistributor::addJob: name='" << name << "' hostname='"
                                             << hostname << "'");
  auto found = runners.find(hostname);
  if (found == runners.end()) {
    runners.emplace(std::make_pair(
        hostname, JobRunner(hostname, maxConcurrentJobsPerHost)));
    found = runners.find(hostname);
    assert(found != runners.end());
  }
  found->second.addJob(name, job);
}

} /* RESTClient */
