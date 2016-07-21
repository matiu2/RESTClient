#include <RESTClient/base/url.hpp>
#include <RESTClient/base/logger.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
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

  using Pair = std::pair<std::string, std::string>;
  x3::rule<class pair_rule, Pair> const pair_rule = "pair";
  const auto pair_rule_def = +(~char_("=")) >> lit("=") >> +(~char_("="));
  BOOST_SPIRIT_DEFINE(pair_rule);

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

  std::string pair("a=1");
  Pair params;
  x3::phrase_parse(pair.begin(), pair.end(), pair_rule_def, x3::space, params);
  LOG_INFO("Out: " << params.first << " - " << params.second);

  //std::string query("?a=1&b=2");
  //ast::QueryParameters params;
  //x3::phrase_parse(query.begin(), query.end(), query_def, x3::space, params);


  return 0;
}
