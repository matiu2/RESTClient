#include "JobRunner.hpp"

#include <RESTClient/http/Services.hpp>

namespace RESTClient {

#if MIN_LOG_LEVEL <= FATAL
int queueWorkerId = 0;
#endif

/// Spawns a single worker for a queue (not a thread, but a co-routine)
void queueWorker(const std::string &hostname, std::queue<QueuedJob> &jobs) {
#if MIN_LOG_LEVEL <= FATAL
  int myId = queueWorkerId++;
#endif
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
#if MIN_LOG_LEVEL <= FATAL
        LOG_ERROR("queueWorker: (" << myId << ") - Job threw exception: "
                                   << job.name << "': " << e.what());
#else
        LOG_ERROR("queueWorker: - Job threw exception: " << job.name
                                                         << "': " << e.what());
#endif
      } catch (...) {
#if MIN_LOG_LEVEL <= FATAL
        LOG_ERROR("queueWorker: ("
                  << myId << ") - Unknown exception caught while running job '"
                  << job.name);
#else
        LOG_ERROR("queueWorker: - Unknown exception caught while running job '"
                  << job.name);
#endif
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
