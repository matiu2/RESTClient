#pragma once
#include "HTTPBody.hpp"
#include "HTTPHeaders.hpp"

namespace RESTClient {

struct HTTPRequest {
  std::string verb;
  std::string path;
  Headers headers;
  HTTPBody body;
  HTTPRequest(std::string verb, std::string path, Headers headers = {},
              HTTPBody body = {})
      : verb(std::move(verb)), path(std::move(path)),
        headers(std::move(headers)), body(std::move(body)) {}
};

} /* RESTClient */
