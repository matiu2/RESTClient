/// Tests chunked upload and zipped upload against the Rackspace cloud files API
/// https://developer.rackspace.com/docs/cloud-files/v1/developer-guide/#create-or-update-object


#include <RESTClient/base/logger.hpp>
#include <RESTClient/base/url.hpp>
#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/http/Services.hpp>
#include <RESTClient/jobManagement/JobRunner.hpp>

// From jsonpp11 external project in cmake
#include <parse_to_json_class.hpp>

#include <iostream>
#include <sstream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/coroutine2/all.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/regex.hpp>

// Generates A-Z 0-9 over and over
class AlphabetoSource {
public:
  using char_type = char;
  using category = boost::iostreams::source_tag;
private:
  using coro_t = boost::coroutines2::coroutine<char>;
  static void _generator(coro_t::push_type &sink) {
    while (true) {
      for (char c = 'A'; c != 'Z' + 1; ++c)
        sink(c);
      sink(' ');
      for (char c = '0'; c != '9' + 1; ++c)
        sink(c);
      sink(' ');
    }
  }
  coro_t::pull_type generator;
  std::streamsize limit;
  std::streamsize transmitted;
public:
  AlphabetoSource(size_t limit)
      : generator(AlphabetoSource::_generator), limit(limit), transmitted(0) {}
  std::streamsize read(char *s, std::streamsize n) {
    size_t toSend = std::min(transmitted, n);
    for (std::streamsize i = 0; i != toSend; ++i) {
      *s++ = generator.get();
      generator();
    }
    transmitted += toSend;
    return toSend;
  }
};

int main(int argc, char *argv[]) {

  using namespace std::placeholders;

  std::vector<RESTClient::QueuedJob> tests;

  // First we need to authenticate against the RS API, before we can get the URL for all the tests
  RESTClient::JobRunner jobs;

  using json::JSON;
  using json::JMap;

  JSON info;
  RESTClient::Headers headers{{"Content-type", "application/json"}};

  auto afterLogin = [&]() {
    // Now get the sydney cloud files URL
    const std::string &token = info["access"]["token"]["id"];
    std::cout << "Token: " << token << std::endl;

    RESTClient::URL syd_cf_url;

    const json::JList &catalog = info["access"]["serviceCatalog"];
    for (const JMap &service : catalog)
      if (service.at("name") == "cloudFiles") {
        const json::JList &endpoints = service.at("endpoints");
        for (const JMap &point : endpoints)
          if (point.at("region") == "SYD")
            syd_cf_url = point.at("publicURL");
      }

    std::cout << "Syd URL: " << syd_cf_url << std::endl;
    headers["X-Auth-Token"] = token;
    
    // Extract the hostname from the URL
    boost::iterator_range<char> divider = boost::find(syd_cf_url, "://");
    std::string::iterator hostStart = divider.end();
    const std::string protocolTerminator = "/?";
    const std::string hostnameTerminators = "/?";
    const std::string pathTerminators = "/?";
    std::string::iterator pathStart = std::find_first_of(
        hostStart, syd_cf_url.end(), hostnameTerminators.begin(),
        hostnameTerminators.end());
    std::string hostname, path;
    std::copy(hostStart, pathStart, std::back_inserter(hostname));
    std::copy(pathStart, 

    boost::iterator_range<char> pathStart(boost::find(divider.end(), boost::find(
    auto part = boost::make_split_iterator(syd_cf_url, "://");
    ++part;
    boost::itertor_range minusProto = *part;
    part = boost::make_split_iterator(minusProto, '/');
    std::string hostname = 
    boost::iterator_range afterProto = syd_cf_url


    auto &q = jobs.queue(syd_cf_url);

    q.emplace(RESTClient::QueuedJob{"Ensure container", syd_cf_url,
                                    [&token](const std::string &name,
                                             const std::string &hostname,
                                             RESTClient::HTTP &server) {
      // List containers
      auto response = server.get("/");
      std::istream &b = response.body;
      boost::iostreams::copy(b, std::cout);
      return true;
    }});

    jobs.startProcessing();

  };


  jobs.queue("https://identity.api.rackspacecloud.com").emplace(RESTClient::QueuedJob{
      "Login", "https://identity.api.rackspacecloud.com/v2.0/tokens",
      [&info, &headers, &afterLogin](const std::string &name, const std::string &hostname,
              RESTClient::HTTP &conn) {
        JSON j(JMap{{"auth", JMap{{"RAX-KSKEY:apiKeyCredentials",
                                   JMap{{"username", RS_USERNAME},
                                        {"apiKey", RS_APIKEY}}}}}});
        RESTClient::HTTPRequest request{
            "POST", "/v2.0/tokens", headers};
        std::ostream& putter(request.body);
        putter << j;
        RESTClient::HTTPResponse response = conn.action(request);
        if (!((response.code >= 200) && (response.code < 300)))
          LOG_ERROR("Couldn't log in to RS: code("
                    << response.code << ") message (" << response.body << ")");
        std::string data(response.body);
        info = json::readValue(data.begin(), data.end());
        afterLogin();
        return true;
      }});

  jobs.startProcessing();

  RESTClient::Services::instance().io_service.run();


  jobs.startProcessing();

  RESTClient::Services::instance().io_service.run();

/*
  q.emplace({"Chunked Transmit", syd_cf_url, [&token](const std::string& name, const std::string& hostname, RESTClient::HTTP& server){
    boost::iostream::filtering_stream<char> s;
    s.push(AlphabetoSource(1024 * 20));
    HTTP::Request r("POST"), 
  });
  */

  return 0;
}
