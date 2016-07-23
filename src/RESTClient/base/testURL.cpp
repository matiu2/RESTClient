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

void testDomainLabel(const std::string& label) {
  LOG_INFO("Testing domainlabel with " << label);
  std::string out;
  auto begin = label.cbegin();
  auto end = label.cend();
  bool worked = x3::phrase_parse(begin, end, domainlabel, x3::space, out);
  std::string matched;
  std::copy(label.cbegin(), begin, std::back_inserter(matched));
  LOG_DEBUG("Matched: " << matched);
  assert(worked);
  assert(begin == end);
  EQ(out, label);
  // Test a bad label
  std::string bad = label + ".com";
  LOG_INFO("Testing domainlabel with " << bad);
  out.clear();
  begin = bad.cbegin();
  end = bad.cend();
  worked = x3::phrase_parse(begin, end, x3::lexeme[domainlabel], x3::space, out);
  assert(worked);
  assert(begin != end);
  EQ(out, label);
}

void testTopLabel(const std::string& label) {
  LOG_INFO("Test top label: " << label);
  auto begin = label.cbegin();
  auto end = label.cend();
  std::string out;
  bool worked = x3::phrase_parse(begin, end, x3::lexeme[toplabel], x3::space, out);
  assert(worked);
  assert(begin == end);
}

void testHostName(const std::string& label) {
  LOG_INFO("Test Hostname: " << label);
  auto begin = label.cbegin();
  auto end = label.cend();
  std::string out;
  bool worked = x3::phrase_parse(begin, end, x3::lexeme[hostname], x3::space, out);
  assert(worked);
  assert(begin == end);
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

  testDomainLabel("somewhere");
  testDomainLabel("s");
  testDomainLabel("s-w");

  testTopLabel("com");

  testHostName("some.host.com");
  /*
  std::string query("?a=1&b=2");
  ast::QueryParameters params;
  x3::phrase_parse(query.begin(), query.end(), query_def, x3::eps, params);
  EQ(params.size(), 2);
  EQ(params.at("a"), "1");
  EQ(params.at("b"), "2");
  */


  return 0;
}
