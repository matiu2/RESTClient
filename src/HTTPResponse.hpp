#pragma once
#include "HTTPBody.hpp"
#include "HTTPHeaders.hpp"

namespace RESTClient {

/// Response from an HTTP request
class HTTPResponse {
public:
  int http_code;
  Headers headers;
  HTTPBody body;
};

} /* RESTClient */
