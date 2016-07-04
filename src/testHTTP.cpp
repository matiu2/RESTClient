#include <RESTClient/base/logger.hpp>
#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/http/Services.hpp>
#include <RESTClient/jobManagement/JobRunner.hpp>

#include <iostream>
#include <sstream>

#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'
namespace algo = boost::algorithm;

bool testGet(const std::string &name, const std::string &hostname,
             RESTClient::HTTP &server, bool toFile) {
  LOG_TRACE("testGet: " << name << " starting....")
  RESTClient::HTTPResponse response;
  if (toFile) {
    response = server.getToFile("/get", name);
    LOG_DEBUG("testGet - filename: " << name);
  } else {
    response = server.get("/get");
    LOG_DEBUG("testGet - no file: ");
  }
  // Check it
  std::string shouldContain("httpbin.org/get");
  response.body.flush();
  std::string body = response.body;
  if (!boost::algorithm::contains(body, shouldContain)) {
    using namespace std;
    cerr << "Expected repsonse to contain '" << shouldContain << "'" << endl
         << "This is what we got: '" << body << "'" << endl << flush;
    return false;
  }
  LOG_INFO(name << " PASSED");
  return true;
}

bool testPut(const std::string &name, const std::string &hostname,
             RESTClient::HTTP &server, bool fromFile) {
  LOG_TRACE(name << " starting...");
  std::string source("This is some data");
  if (fromFile) {
    std::fstream f(name);
    f << source;
    f.seekg(0);
    server.putStream("/put", f);
  } else {
    server.put("/put", source);
  }
  LOG_INFO(name << " PASSED");
  return true;
}

bool testPost(const std::string &name, const std::string &hostname,
              RESTClient::HTTP &server, bool fromFile) {
  LOG_TRACE(name << " starting....")
  std::string source("This is some data");
  if (fromFile) {
    std::fstream f(name);
    f << source;
    f.seekg(0);
    server.putStream("/post", f);
  } else {
    server.post("/post", source);
  }
  LOG_INFO(name << " PASSED");
  return true;
}

bool testChunkedGet(const std::string &name, const std::string &hostname,
                    RESTClient::HTTP &server, bool toFile) {
  LOG_TRACE(name << " starting....")
  const int size = 1024;
  const int chunk_size = 80;
  std::stringstream path;
  path << "/range/" << size << "?duration=1&chunk_size=" << chunk_size;
  RESTClient::HTTPResponse response;
  if (toFile) {
    response = server.getToFile(path.str(), name);
  } else
    response = server.get(path.str());
  // Make up the expected string
  const std::string block = "abcdefghijklmnopqrstuvwxyz";
  std::stringstream expected;
  int i = 0;
  size_t block_size = block.size();
  while (i < size) {
    int toWrite = size - i;
    if (toWrite >= block.size()) {
      expected << block;
      i += block_size;
    } else {
      std::copy_n(block.begin(), toWrite,
                  std::ostream_iterator<char>(expected));
      i += toWrite;
    }
  }

  std::string body = response.body;

  if (expected.str() != body) {
    std::stringstream msg;
    using std::endl;
    msg << "Received body not the same as the expected body. "
        << "Expected Length: " << size << endl
        << "Actual Length: " << body.size() << endl << endl
        << "=== Expected data begin === " << endl << expected.str() << endl
        << "=== Expected data end ===" << endl
        << "=== Actual data begin === " << endl << body << endl
        << "=== Actual data end ===" << endl;
    throw std::runtime_error(msg.str());
  }
  LOG_INFO(name << " PASSED");
  return true;
}

bool testDelete(const std::string &name, const std::string &hostname,
                RESTClient::HTTP &server, bool fromFile) {
  LOG_TRACE(name << " starting....")
  RESTClient::HTTPResponse response = server.del("/delete");
  std::string shouldContain("httpbin.org/delete");
  std::string body = response.body;
  assert(boost::algorithm::contains(body, shouldContain));
  LOG_INFO(name << " PASSED");
  return true;
}

bool testGZIPGet(const std::string &name, const std::string &hostname,
                 RESTClient::HTTP &server, bool toFile) {
  LOG_TRACE(name << " starting....")
  RESTClient::HTTPResponse response;
  if (toFile) {
    response = server.getToFile("/gzip", name);
  } else
    response = server.get("/gzip");
  // Check it
  std::string shouldContain(R"("gzipped": true)");
  response.body.flush();
  std::string body = response.body;
  if (!boost::algorithm::contains(body, shouldContain)) {
    LOG_ERROR(name << " FAILED: "
                   << "Expected repsonse to contain '" << shouldContain << "'"
                   << std::endl << "This is what we got: '" << body << "'");
    return false;
  }
  LOG_INFO(name << " PASSED");
  return true;
}

int main(int argc, char *argv[]) {

  using namespace std::placeholders;

  std::vector<RESTClient::QueuedJob> tests(
      {// HTTP get
       {"GET Content-Length - no ssl - no file", "http://httpbin.org",
        std::bind(testGet, _1, _2, _3, false)},
       {"GET Content-Length - no ssl - file", "http://httpbin.org",
        std::bind(testGet, _1, _2, _3, true)},
       {"GET Content-Length - ssl - no file", "https://httpbin.org",
        std::bind(testGet, _1, _2, _3, false)},
       {"GET Content-Length - ssl - file", "https://httpbin.org",
        std::bind(testGet, _1, _2, _3, true)},
       // HTTP chunked get
       {"GET CHUNKED - no ssl - no file", "http://httpbin.org",
        std::bind(testChunkedGet, _1, _2, _3, false)},
       {"GET CHUNKED - no ssl - file", "http://httpbin.org",
        std::bind(testChunkedGet, _1, _2, _3, true)},
       {"GET CHUNKED - ssl - no file", "https://httpbin.org",
        std::bind(testChunkedGet, _1, _2, _3, false)},
       {"GET CHUNKED - ssl - file", "https://httpbin.org",
        std::bind(testChunkedGet, _1, _2, _3, true)},
       // Delete
       {"DELETE - no ssl", "http://httpbin.org",
        std::bind(testDelete, _1, _2, _3, false)},
       {"DELETE - ssl", "https://httpbin.org",
        std::bind(testDelete, _1, _2, _3, false)},
       // Put
       {"PUT - no ssl - no file", "http://httpbin.org",
        std::bind(testPut, _1, _2, _3, false)},
       {"PUT - no ssl - file", "http://httpbin.org",
        std::bind(testPut, _1, _2, _3, true)},
       {"PUT - ssl - no file", "https://httpbin.org",
        std::bind(testPut, _1, _2, _3, false)},
       {"PUT - ssl - file", "https://httpbin.org",
        std::bind(testPut, _1, _2, _3, true)},
       // Post
       {"POST - no ssl - no file", "http://httpbin.org",
        std::bind(testPut, _1, _2, _3, false)},
       {"POST - no ssl - file", "http://httpbin.org",
        std::bind(testPut, _1, _2, _3, true)},
       {"POST - ssl - no file", "https://httpbin.org",
        std::bind(testPut, _1, _2, _3, false)},
       {"POST - ssl - file", "https://httpbin.org",
        std::bind(testPut, _1, _2, _3, true)},
       // HTTP get gzipped content
       {"GET gzip -Length - no ssl - no file", "http://httpbin.org",
        std::bind(testGZIPGet, _1, _2, _3, false)},
       {"GET gzip -Length - no ssl - file", "http://httpbin.org",
        std::bind(testGZIPGet, _1, _2, _3, true)},
       {"GET gzip -Length - ssl - no file", "https://httpbin.org",
        std::bind(testGZIPGet, _1, _2, _3, false)},
       {"GET gzip -Length - ssl - file", "https://httpbin.org",
        std::bind(testGZIPGet, _1, _2, _3, true)}});

  RESTClient::JobRunner jobs;

  // Parse args for regexes
  std::vector<boost::regex> regexs;
  for (int i = 1; i < argc; ++i)
    regexs.push_back(boost::regex(argv[i]));

  if (regexs.size() != 0)
    for (auto &job : tests) {
      for (auto &regex : regexs)
        if (boost::regex_search(job.name, regex)) {
          jobs.queue(job.hostname).emplace(std::move(job));
          break;
        }
    }
  else {
    for (auto &job : tests)
      jobs.queue(job.hostname).emplace(std::move(job));
  }

  jobs.startProcessing();

  RESTClient::Services::instance().io_service.run();
  return 0;
}
