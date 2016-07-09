/// Tests chunked upload and zipped upload against the Rackspace cloud files API
/// https://developer.rackspace.com/docs/cloud-files/v1/developer-guide/#create-or-update-object


#include <RESTClient/base/logger.hpp>
#include <RESTClient/http/HTTP.hpp>
#include <RESTClient/http/Services.hpp>
#include <RESTClient/jobManagement/JobRunner.hpp>

// From jsonpp11 external project in cmake
#include <parse_to_json_class.hpp>

#include <iostream>
#include <sstream>

#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>


int main(int argc, char *argv[]) {

  using namespace std::placeholders;

  std::vector<RESTClient::QueuedJob> tests;

  // First we need to authenticate against the RS API, before we can get the URL for all the tests
  RESTClient::JobRunner jobs;

  using json::JSON;
  using json::JMap;

  JSON info;

  jobs.queue("https://identity.api.rackspacecloud.com").emplace(RESTClient::QueuedJob{
      "Login", "https://identity.api.rackspacecloud.com/v2.0/tokens",
      [&info](const std::string &name, const std::string &hostname,
              RESTClient::HTTP &conn) {
        JSON j(JMap{{"auth", JMap{{"RAX-KSKEY:apiKeyCredentials",
                                   JMap{{"username", RS_USERNAME},
                                        {"apiKey", RS_APIKEY}}}}}});
        std::stringstream request;
        request << j;
        RESTClient::HTTPResponse response =
            conn.post("/v2.0/tokens", request.str());
        if (!((response.code >= 200) && (response.code < 300)))
          LOG_ERROR("Couldn't log in to RS: code("
                    << response.code << ") message (" << response.body << ")");
        std::string data(response.body);
        info = json::readValue(data.begin(), data.end());
        return true;
      }});

  jobs.startProcessing();

  RESTClient::Services::instance().io_service.run();

  std::cout << info << std::endl;
  return 0;
}
