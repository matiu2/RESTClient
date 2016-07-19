#include <RESTClient/base/url.hpp>
#include <RESTClient/base/logger.hpp>

#include <string>
#include <cassert>

using namespace std;
using namespace RESTClient;

#define EQ(a, b)                                                               \
  if (a != b) {                                                                \
    LOG_ERROR("Expected a == b, but it doesn't. a: "                           \
              << a << " - b: " << b << " - Line: " << __LINE__ << " - File: "  \
              << __FILE__ << " - Function: " << __FUNCTION__ << std::endl);    \
  }

int main(int , char**)
{
  URL test1("http://somewhere.com");
  LOG_INFO("Test 1: " << test1);
  EQ(test1.parts().protocol, "http");
  EQ(test1.parts().hostname, "somewhere.com");
  LOG_INFO("Test 1 - PASSED");

  URL test2("https://somewhere.com/some/path/");
  LOG_INFO("Test 2: " << test2);
  using namespace std;
  cout << "Protocol: " << test2.parts().protocol << endl
       << "Hostname: " << test2.parts().hostname << endl;
  EQ(test2.parts().protocol, "https");
  EQ(test2.parts().hostname, "somewhere.com");
  EQ(test2.parts().path, "/some/path/");
  LOG_INFO("Test 2 - PASSED");
  return 0;
}
