#include "HTTP.hpp"
#include "JobRunner.hpp"
#include "logger.hpp"

#include <iostream>
#include <sstream>

#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

bool testGet(asio::io_service& io_service, tcp::resolver& resolver, bool is_ssl, bool toFile, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  RESTClient::HTTPResponse response;
  if (toFile) {
    std::stringstream fn;
    fn << "tmp-get";
    if (is_ssl)
      fn << "-ssl";
    fn << ".json";
    response = server.getToFile("/get", fn.str());
  } else
    response = server.get("/get");
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
  return true;
}

bool testPut(asio::io_service &io_service, tcp::resolver &resolver, bool is_ssl,
             bool fromFile, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  std::string source("This is some data");
  if (fromFile) {
    std::stringstream fn;
    fn << "test-put";
    if (is_ssl)
      fn << "-ssl";
    fn << ".txt";
    std::fstream f(fn.str());
    f << source;
    f.seekg(0);
    server.putStream("/put", f);
  } else {
    server.put("/put", source);
  }
  return true;
}

bool testPost(asio::io_service &io_service, tcp::resolver &resolver, bool is_ssl,
             bool fromFile, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  std::string source("This is some data");
  if (fromFile) {
    std::stringstream fn;
    fn << "test-post";
    if (is_ssl)
      fn << "-ssl";
    fn << ".txt";
    std::fstream f(fn.str());
    f << source;
    f.seekg(0);
    server.putStream("/post", f);
  } else {
    server.post("/post", source);
  }
  return true;
}

bool testChunkedGet(asio::io_service &io_service, tcp::resolver &resolver,
                    bool is_ssl, bool toFile, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  const int size = 1024;
  const int chunk_size = 80;
  std::stringstream path;
  path << "/range/" << size << "?duration=1&chunk_size=" << chunk_size;
  RESTClient::HTTPResponse response;
  if (toFile) {
    std::stringstream fn;
    fn << "tmp-chunked";
    if (is_ssl)
      fn << "-ssl";
    fn << ".json";
    response = server.getToFile(path.str(), fn.str());
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
  return true;
}

bool testDelete(asio::io_service &io_service, tcp::resolver &resolver,
                    bool is_ssl, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  RESTClient::HTTPResponse response = server.del("/delete");
  std::string shouldContain("httpbin.org/delete");
  std::string body = response.body;
  assert(boost::algorithm::contains(body, shouldContain));
  return true;
}

bool testGZIPGet(asio::io_service& io_service, tcp::resolver& resolver, bool is_ssl, bool toFile, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  RESTClient::HTTPResponse response;
  if (toFile) {
    std::stringstream fn;
    fn << "tmp-get";
    if (is_ssl)
      fn << "-ssl";
    fn << ".json";
    response = server.getToFile("/gzip", fn.str());
  } else
    response = server.get("/gzip");
  // Check it
  std::string shouldContain(R"("gzipped": true)");
  response.body.flush();
  std::string body = response.body;
  if (!boost::algorithm::contains(body, shouldContain)) {
    using namespace std;
    cerr << "Expected repsonse to contain '" << shouldContain << "'" << endl
         << "This is what we got: '" << body << "'" << endl << flush;
    return false;
  }
  return true;
}


int main(int argc, char *argv[]) {
  asio::io_service io_service;
  tcp::resolver resolver(io_service);

  using namespace std::placeholders;

  std::vector<
      std::pair<std::string, std::function<bool(asio::yield_context)>>> tests{
      // HTTP get
      {"GET Content-Length - no ssl - no file",
       std::bind(testGet, std::ref(io_service), std::ref(resolver), false,
                 false, _1)},
      {"GET Content-Length - no ssl - file",
       std::bind(testGet, std::ref(io_service), std::ref(resolver), false, true,
                 _1)},
      {"GET Content-Length - ssl - no file",
       std::bind(testGet, std::ref(io_service), std::ref(resolver), true, false,
                 _1)},
      {"GET Content-Length - ssl - file",
       std::bind(testGet, std::ref(io_service), std::ref(resolver), true, true,
                 _1)},
      // HTTP chunked get
      {"GET CHUNKED - no ssl - no file",
       std::bind(testChunkedGet, std::ref(io_service), std::ref(resolver),
                 false, false, _1)},
      {"GET CHUNKED - no ssl - file",
       std::bind(testChunkedGet, std::ref(io_service), std::ref(resolver),
                 false, true, _1)},
      {"GET CHUNKED - ssl - no file",
       std::bind(testChunkedGet, std::ref(io_service), std::ref(resolver), true,
                 false, _1)},
      {"GET CHUNKED - ssl - file",
       std::bind(testChunkedGet, std::ref(io_service), std::ref(resolver), true,
                 true, _1)},
      // Delete
      {"DELETE - no ssl", std::bind(testDelete, std::ref(io_service),
                                    std::ref(resolver), false, _1)},
      {"DELETE - ssl", std::bind(testDelete, std::ref(io_service),
                                 std::ref(resolver), false, _1)},
      // Put
      {"PUT - no ssl - no file",
       std::bind(testPut, std::ref(io_service), std::ref(resolver), false,
                 false, _1)},
      {"PUT - no ssl - file", std::bind(testPut, std::ref(io_service),
                                        std::ref(resolver), false, true, _1)},
      {"PUT - ssl - no file", std::bind(testPut, std::ref(io_service),
                                        std::ref(resolver), true, false, _1)},
      {"PUT - ssl - file", std::bind(testPut, std::ref(io_service),
                                     std::ref(resolver), true, true, _1)},
      // Post
      {"POST - no ssl - no file",
       std::bind(testPut, std::ref(io_service), std::ref(resolver), false,
                 false, _1)},
      {"POST - no ssl - file", std::bind(testPut, std::ref(io_service),
                                         std::ref(resolver), false, true, _1)},
      {"POST - ssl - no file", std::bind(testPut, std::ref(io_service),
                                         std::ref(resolver), true, false, _1)},
      {"POST - ssl - file", std::bind(testPut, std::ref(io_service),
                                      std::ref(resolver), true, true, _1)},
      // HTTP get gzipped content
      {"GET gzip -Length - no ssl - no file",
       std::bind(testGZIPGet, std::ref(io_service), std::ref(resolver), false,
                 false, _1)},
      {"GET gzip -Length - no ssl - file",
       std::bind(testGZIPGet, std::ref(io_service), std::ref(resolver), false,
                 true, _1)},
      {"GET gzip -Length - ssl - no file",
       std::bind(testGZIPGet, std::ref(io_service), std::ref(resolver), true,
                 false, _1)},
      {"GET gzip -Length - ssl - file",
       std::bind(testGZIPGet, std::ref(io_service), std::ref(resolver), true,
                 true, _1)},

  };

  RESTClient::JobRunner jobs(io_service, 4);

  // Parse args for regexes
  std::vector<boost::regex> regexs;
  for (int i = 1; i < argc; ++i)
    regexs.push_back(boost::regex(argv[i]));

  if (regexs.size() != 0)
    for (auto &pair : tests) {
      for (auto &regex : regexs)
        if (boost::regex_search(pair.first, regex)) {
          jobs.addJob(pair.first, pair.second);
          break;
        }
    }
  else {
    for (auto &pair : tests)
      jobs.addJob(pair.first, pair.second);
  }

  io_service.run();
  return 0;
}
