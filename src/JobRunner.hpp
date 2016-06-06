/// Keeps X amount of jobs running in a single thread using the boost asio job runner

#include <queue>
#include <boost/asio/io_service.hpp>

namespace RESTClient {

using namespace boost;

/// Basically a reference counting sentinel
/// Increases count on construction, decreases on destruction
struct CountSentinel {
  size_t& count;
  std::function<void()> onDone;
  CountSentinel(size_t &count, std::function<void()> onDone)
      : count(count), onDone(onDone) {}
  ~CountSentinel() {
    --count;
    // onDone will start the next job in the queue if there is one
    // it hopefully shouldn't throw any exceptions
    if (onDone)
      onDone();
  }
};

struct JobRunner {
  using Job = std::function<bool(asio::yield_context)>;
  asio::io_service& io_service;
  const size_t maxConcurrentJobs;
  size_t jobsRunningNow = 0;

  std::queue<std::pair<std::string, Job>> jobs;
  JobRunner(asio::io_service &io_service, size_t maxConcurrentJobs = 8)
      : io_service(io_service), maxConcurrentJobs(maxConcurrentJobs) {}
  void addJob(std::string name, Job job) {
    jobs.emplace(std::move(typename decltype(jobs)::value_type(
        {std::move(name), std::move(job)})));
    if (jobsRunningNow < maxConcurrentJobs)
      startJob();
  }
  void startJob() {
    std::string name;
    Job job;
    std::tie(name, job) = std::move(jobs.front());
    jobs.pop();
    ++jobsRunningNow;
    asio::spawn(io_service,
                [ job = std::move(job), name = std::move(name), this ](
                    asio::yield_context yield) {
      CountSentinel s(this->jobsRunningNow,
                      std::bind(&JobRunner::checkStartJob, this));
      try {
        job(yield);
      } catch (std::exception &e) {
        std::stringstream msg;
        msg << "Exception caught while running job '" << name
            << "': " << e.what();
        throw std::runtime_error(msg.str());
      } catch (...) {
        std::stringstream msg;
        msg << "Unknown exception caught while running job '" << name;
        throw std::runtime_error(msg.str());
      }
    });
  }
  /// Starts a job if one is avalable and we are below maxConcurrentJobs
  void checkStartJob() {
    if ((jobsRunningNow < maxConcurrentJobs) && (jobs.size() > 0)) {
      startJob();
    }
  }
};
  
} /* RESTClient */
