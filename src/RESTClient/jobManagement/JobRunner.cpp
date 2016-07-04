#include "JobRunner.hpp"

#include <RESTClient/http/Services.hpp>

namespace RESTClient {

int queueWorkerId = 0;

/// Spawns a single worker for a queue (not a thread, but a co-routine)
void queueWorker(const std::string &hostname, std::queue<QueuedJob> &jobs) {
  int myId = queueWorkerId++;
  LOG_TRACE("queueWorker: (" << myId << ") " << hostname << " - " << jobs.size());
  asio::spawn(Services::instance().io_service, [&, myId](asio::yield_context yield) {
    LOG_TRACE("queueWorker: (" << myId << ") - job starting: " << hostname
                               << " - " << jobs.size());
    if (jobs.size() == 0)
      return;
    HTTP conn(hostname, yield);
    while (jobs.size() > 0) {
      QueuedJob job = std::move(jobs.front());
      jobs.pop();
      try {
        LOG_DEBUG("queueWorker: (" << myId << ") - Starting Job: " << hostname
                                   << " - " << job.name);
        job(conn);
        LOG_INFO("queueWorker: (" << myId << ") - Job Completed: " << hostname
                                  << " - " << job.name);
      } catch (std::exception &e) {
        LOG_ERROR("queueWorker: (" << myId << ") - Job threw exception: "
                                   << job.name << "': " << e.what());
      } catch (...) {
        LOG_ERROR("queueWorker: ("
                  << myId << ") - Unknown exception caught while running job '"
                  << job.name);
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
