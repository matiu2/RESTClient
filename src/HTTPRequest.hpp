#pragma once
#include "HTTPBody.hpp"

namespace RESTClient {
  
struct HTTPRequest {
  Headers headers;
  HTTPBody body;
};

} /* RESTClient */
