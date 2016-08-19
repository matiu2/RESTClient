#include "JobRunner.hpp"

#include <RESTClient/http/Services.hpp>

namespace RESTClient {

int queueWorkerId = 0;

/// Spawns a single worker for a queue (not a thread, but a co-routine)
/// Returns immediately but when
/// RESTClient::Services::instance().io_service.run() is run, the spawned jobs
/// will run
void queueWorker(const HostInfo &host_info, std::queue<QueuedJob> &jobs) {
  int myId = queueWorkerId++;
  std::string conn_info = host_info;
  LOG_TRACE("queueWorker spawning: (" << myId << ") " << conn_info << " - "
                                      << jobs.size());
  asio::spawn(Services::instance().io_service,
              [ conn_info = std::move(conn_info), myId, &jobs, &host_info ](
                  asio::yield_context yield) {
    LOG_TRACE("queueWorker running: (" << myId << ") " << conn_info << " - "
                                       << jobs.size());
    if (jobs.size() == 0) {
      LOG_TRACE("queueWorker NO JOBS - exiting: (" << myId << ") " << conn_info
                                                   << " - " << jobs.size());
      return;
    }
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
  return queues[hostInfo];
}

void JobRunner::run(size_t connectionsPerHost) {
  while (queues.size() > 0) {
    LOG_TRACE("jobRunner::run - spawning workers: " << queues.size());
    // For each hostname and job queue
    for (auto &both : queues) {
      // Spawn workers
      for (size_t i = 0; i < std::min(connectionsPerHost, both.second.size());
           ++i) {
        // Spawn a worker
        queueWorker(both.first, both.second);
      }
    }
    // Run everything that needs running
    LOG_TRACE("jobRunner::run - Running queued jobs");
    auto& io = RESTClient::Services::instance().io_service;
    io.reset();
    io.run();
    LOG_TRACE("jobRunner::run - removing empty queues: " << queues.size());
    // Delete empty queues
    auto it = queues.begin();
    while (it != queues.end()) {
      if (it->second.size() == 0) {
        LOG_TRACE("JobRunner::run - Removing queue " << it->first);
        it = queues.erase(it);
      } else { 
        LOG_TRACE("JobRunner::run - NOT Removing queue " << it->first);
        ++it;
      }
    }
    LOG_TRACE("jobRunner::run - empty queues removed: " << queues.size());
  }
}

} /* RESTClient */
