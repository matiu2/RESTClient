#include "JobRunner.hpp"

#include <RESTClient/http/Services.hpp>

namespace RESTClient {

/// Spawns a single worker for a queue (not a thread, but a co-routine)
void queueWorker(const std::string &hostname, std::queue<QueuedJob> &jobs) {
  LOG_TRACE("queueWorker: " << hostname << " - " << jobs.size());
  asio::spawn(Services::instance().io_service, [&](asio::yield_context yield) {
    LOG_TRACE("queueWorker - job starting: " << hostname << " - " << jobs.size());
    if (jobs.size() == 0)
      return;
    HTTP conn(hostname, yield);
    while (jobs.size() > 0) {
      QueuedJob job = std::move(jobs.front());
      jobs.pop();
      try {
        LOG_DEBUG("Starting Job: " << hostname << " - " << job.name);
        job(conn);
        LOG_INFO("Job Completed: " << hostname << " - " << job.name);
      } catch (std::exception &e) {
        LOG_ERROR("Job threw exception: " << job.name << "': " << e.what());
      } catch (...) {
        LOG_ERROR("Unknown exception caught while running job '" << job.name);
      }
    }
    conn.close();
  });
}

JobRunner::JobQueue &JobRunner::queue(const std::string &hostname) {
  LOG_TRACE("JobRunner::queue: " << hostname);
  return queues[hostname];
}

void JobRunner::startProcessing(size_t connectionsPerHost) {
  LOG_TRACE("jobRunner::startProcessing: " << queues.size() << " hostnames found");
  // For each hostname and job queue
  for (auto &both : queues) {
    LOG_TRACE("Processing queue. Hostname: " << both.first << " - queue size: "
                                             << both.second.size())
    for (size_t i = 0; i < connectionsPerHost; ++i) {
      queueWorker(both.first, both.second);
    }
  }
}

} /* RESTClient */
