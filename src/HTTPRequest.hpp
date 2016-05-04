#pragma once
#include "HTTPBody.hpp"
#include "HTTPHeaders.hpp"

namespace RESTClient {

struct HTTPRequest {
  std::string verb;
  std::string path;
  std::string hostName;
  Headers headers;
  HTTPBody body;
};

} /* RESTClient */
