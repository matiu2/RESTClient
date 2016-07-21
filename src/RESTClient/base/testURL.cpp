#include <RESTClient/base/url.hpp>
#include <RESTClient/base/logger.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/include/io.hpp>

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


  using x3::char_;
  using x3::lit;
  using x3::alnum;
  using x3::hex;
  using x3::string;
  using x3::attr;

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

  std::string query("?a=1&b=2");
  ast::QueryParameters params;
  x3::phrase_parse(query.begin(), query.end(), query_def, x3::space, params);
  EQ(params.size(), 2);
  EQ(params.at("a"), "1");
  EQ(params.at("b"), "2");


  return 0;
}
