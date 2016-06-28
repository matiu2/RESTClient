#include "JobRunner.hpp"

namespace RESTClient {

/// Basically a reference counting sentinel
/// Increases count on construction, decreases on destruction
struct CountSentinel {
  size_t& count;
  std::function<void()> onDone;
  CountSentinel(size_t &count, std::function<void()> onDone)
      : count(count), onDone(onDone) {
    LOG_TRACE("CountSentinel: " << count);
  }
  ~CountSentinel() {
    LOG_TRACE("~CountSentinel: " << count);
    --count;
    // onDone will start the next job in the queue if there is one
    // it hopefully shouldn't throw any exceptions
    if (onDone)
      onDone();
  }
};

JobRunner::JobRunner(std::string hostname, size_t maxConcurrentJobs)
    : hostname(hostname), connections(hostname),
      maxConcurrentJobs(maxConcurrentJobs) {
  LOG_TRACE("JobRunner constructor: " << hostname << " - "
                                      << maxConcurrentJobs);
}

void JobRunner::addJob(std::string name, JobFunction job) {
  LOG_TRACE("JobRunner::addJob: - name: " << name << " - hostname: " << hostname
                                          << " - running: " << jobsRunningNow
                                          << " - max: " << maxConcurrentJobs);
  jobs.emplace(QueuedJob{name, hostname, job});
  checkStartJob();
}

void JobRunner::startJob() {
  QueuedJob job = std::move(jobs.front());
  LOG_TRACE("jobRunner::startJob: " << job.name
                                    << " - jobsRunningNow: " << jobsRunningNow);
  jobs.pop();
  ++jobsRunningNow;
  asio::spawn(services->io_service,
              [ job = std::move(job), this ](
                  asio::yield_context yield) {
    try {
      CountSentinel s(this->jobsRunningNow,
                      std::bind(&JobRunner::checkStartJob, this));
      LOG_DEBUG("Starting Job: " << job.name);
      ConnectionUseSentry sentry(connections.getSentry(yield));
      job(sentry.connection());
      LOG_INFO("Job Completed: " << job.name);
      // If we don't have any more jobs, we won't be re-using this connection
      if (jobs.size() == 0) {
        sentry.connection().close();
        connections.cleanup();
      }
    } catch (std::exception &e) {
      LOG_ERROR("Job threw exception: " << job.name << "': " << e.what());
    } catch (...) {
      LOG_ERROR("Unknown exception caught while running job '" << job.name);
    }
  });
}

void JobRunner::checkStartJob() {
  LOG_TRACE("JobRunner::checkStartJob");
  if ((jobsRunningNow < maxConcurrentJobs) && (jobs.size() > 0)) {
    startJob();
  }
}

} /* RESTClient */
