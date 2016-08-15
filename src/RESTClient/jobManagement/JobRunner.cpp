#include "JobRunner.hpp"

#include <RESTClient/http/Services.hpp>

namespace RESTClient {

int queueWorkerId = 0;

/// Spawns a single worker for a queue (not a thread, but a co-routine)
void queueWorker(const HostInfo &host_info, std::queue<QueuedJob> &jobs) {
  int myId = queueWorkerId++;
  std::string conn_info = host_info;
  LOG_TRACE("queueWorker: (" << myId << ") " << conn_info << " - "
                             << jobs.size());
  asio::spawn(Services::instance().io_service,
              [ conn_info = std::move(conn_info), myId, &jobs, &host_info ](
                  asio::yield_context yield) {
    LOG_TRACE("queueWorker: (" << myId << ") - job starting: " << conn_info
                               << " - " << jobs.size());
    if (jobs.size() == 0)
      return;
    // Extract the login info
    HTTP conn(host_info, yield);
    while (jobs.size() > 0) {
      QueuedJob job = std::move(jobs.front());
      jobs.pop();
      try {
        LOG_DEBUG("queueWorker: (" << myId << ") - Starting Job: " << conn_info
                                   << " - " << job.name);
        job(conn);
        LOG_DEBUG("queueWorker: (" << myId << ") - Job Completed: " << conn_info
                                   << " - " << job.name);
      } catch (std::exception &e) {
        LOG_WARN("queueWorker: (" << myId << ") - Job (" << job.name
                                  << ") - host (" << conn_info
                                  << ") threw exception: "
                                  << "': " << e.what());
      } catch (...) {
        LOG_WARN("queueWorker: ("
                 << myId << ") - Unknown exception caught while running job '"
                 << job.name);
      }
    }
    conn.close();
  });
}

JobRunner::JobQueue &JobRunner::queue(const HostInfo &hostInfo) {
  LOG_TRACE("JobRunner::queue: " << hostInfo);
  return queues[hostInfo];
}

void JobRunner::startProcessing(size_t connectionsPerHost) {
  LOG_TRACE("jobRunner::startProcessing: " << queues.size()
                                           << " hostnames found");
  // Delete empty queues
  for (auto it = queues.begin(); it != queues.end();)
    if (it->second.size() == 0)
      queues.erase(it);
    else
      ++it;
  // For each hostname and job queue
  for (auto &both : queues) {
    LOG_TRACE("Processing queue. Hostname: " << both.first << " - queue size: "
                                             << both.second.size())
    for (size_t i = 0; i < std::min(connectionsPerHost, both.second.size());
         ++i) {
      queueWorker(both.first, both.second);
    }
  }
}

} /* RESTClient */
