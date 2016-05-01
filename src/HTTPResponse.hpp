#pragma once
#include "HTTPBody.hpp"

namespace RESTClient {

using Headers = std::map<std::string, std::string>;

/// Response from an HTTP request
class HTTPResponse {
public:
  int http_code;
  Headers headers;
  HTTPBody body;
};

} /* RESTClient */
