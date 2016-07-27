#include <RESTClient/base/url.hpp>
#include <RESTClient/base/logger.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/optional/optional.hpp>
#include <boost/spirit/home/x3.hpp>

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

template <typename T>
void test(const std::string &label, const T &parser) {
  auto begin = label.cbegin();
  auto end = label.cend();
  std::string out;
  bool worked =
      x3::phrase_parse(begin, end, x3::lexeme[parser], x3::space, out);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(label.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "Label: " << label
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
  EQ(out, label);
}

void testDomainLabel(const std::string& label) {
  LOG_INFO("Testing domainlabel with " << label);
  test(label, domainlabel);
  // Test a bad label
  std::string bad = label + ".com";
  LOG_INFO("Testing domainlabel with " << bad);
  std::string out;
  auto begin = bad.cbegin();
  auto end = bad.cend();
  bool worked =
      x3::phrase_parse(begin, end, x3::lexeme[domainlabel], x3::space, out);
  assert(worked);
  assert(begin != end);
}

void testTopLabel(const std::string& label) {
  LOG_INFO("Test top label: " << label);
  test(label, toplabel);
}

void testHostName(const std::string& label) {
  LOG_INFO("Test Hostname: " << label);
  test(label, hostname);
}

void testQueryWord(const std::string& label) {
  LOG_INFO("Test Query word: " << label);
  test(label, query_word);
}

void testQueryPair(const std::string& label) {
  LOG_INFO("Test Query pair: " << label);
  std::pair<std::string, std::string> query_pair_out;
  x3::phrase_parse(label.begin(), label.end(), query_pair, x3::space, query_pair_out);
  std::pair<std::string, std::string> out;
  auto begin = label.cbegin();
  auto end = label.cend();
  bool worked =
      x3::phrase_parse(begin, end, x3::lexeme[query_pair], x3::space, out);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(label.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "Label: " << label
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
}

void testHostPort(const std::string &label, const std::string &firstPart,
                  boost::optional<unsigned short> portToTest = {}) {
  LOG_INFO("Test hostport: " << label);
  auto begin = label.cbegin();
  auto end = label.cend();
  std::pair<std::string, boost::optional<unsigned short>> out;
  bool worked =
      x3::phrase_parse(begin, end, x3::lexeme[hostport_def], x3::space, out);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(label.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "Label: " << label
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
  EQ(out.first, firstPart);
  EQ(out.second, portToTest);
}

void testUser(const std::string label) {
  LOG_INFO("Test User: " << label);
  test(label, user_string);
}

void testUserPass(const std::string &label, const std::string &username,
                  const boost::optional<std::string> &password) {
  LOG_INFO("Test username and password: " << label);
  auto begin = label.cbegin();
  auto end = label.cend();
  userpass_type out;
  bool worked =
      x3::phrase_parse(begin, end, x3::lexeme[userpass_def], x3::space, out);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(label.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "label: " << label
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
  EQ(out.first, username);
  EQ(out.second, password);
}

/*
void testUserPassHost(const std::string &url, const std::string& username, const std::string& password, const std::string& hostname,
                  boost::optional<unsigned short> portToTest = {}) {
  LOG_INFO("Test username and password: " << url);
  auto begin = url.cbegin();
  auto end = url.cend();
  std::tuple<std::string, std::string,
             std::pair<std::string, boost::optional<unsigned short>>> out;
  bool worked =
      x3::phrase_parse(begin, end, x3::lexeme[login], x3::space, out);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(url.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "url: " << url
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
  EQ(std::get<0>(out), username);
  EQ(std::get<1>(out), password);
  EQ(std::get<2>(out).first, hostname);
  EQ(std::get<2>(out).second, portToTest);
}
  */

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
  testHostName("somewhere");
  testHostName("other-host.com");

  testHostPort("other-host.com", "other-host.com");
  testHostPort("somewhere:8080", "somewhere", 8080);
  testHostPort("somewhere.com:8000", "somewhere.com", 8000);

  testUser("mister");
  testUser(";?&=mister");
  //testUserPass("mister@somewhere.co.uk", "mister", "", "somewhere.co.uk");
  testUserPass("mister", "mister", {});

  testQueryWord("abc");
  testQueryPair("abc=123");

  std::string query("?a=1&b=2");
  ast::QueryParameters params;
  x3::phrase_parse(query.begin(), query.end(), query_def, x3::space, params);
  EQ(params.size(), 2);
  EQ(params.at("a"), "1");
  EQ(params.at("b"), "2");

  return 0;
}
