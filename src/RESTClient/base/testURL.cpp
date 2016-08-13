#include <RESTClient/base/url.cpp> // We need to import the .cpp so to access it's internals
#include <RESTClient/base/logger.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/optional/optional.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/any.hpp>

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

namespace x3 = boost::spirit::x3;

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
      x3::phrase_parse(begin, end, x3::lexeme[hostport], x3::space, out);
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

template <typename T> using bo = boost::optional<T>;

void testLogin(const std::string &input, const std::string &hostname,
               boost::optional<std::string> username,
               boost::optional<std::string> password,
               boost::optional<unsigned short> portToTest) {
  LOG_INFO("Test login: " << input);
  auto begin = input.cbegin();
  auto end = input.cend();
  using bos = bo<std::string>;
  using bon = bo<unsigned short>;
  using bt = bo<std::pair<std::string, std::string>>;
  bool worked = x3::phrase_parse(
      begin, end, x3::lexeme[-userpass >> hostname >> -port >> host_terminator],
      x3::space);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(input.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "input: " << input
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
  /*
   * Currently we're not parsing the output into any structure. Just making sure that it can parse.
  auto &userpass = std::get<0>(out);
  if (userpass) {
    assert(username);
    if (username) {
      EQ(std::get<0>(userpass.get()), username.get());
    } else {
      EQ(std::get<0>(userpass.get()), "");
    }
    auto &out_pass = std::get<1>(userpass.get()).get();
    EQ(out_pass, password);
  }
  EQ(std::get<1>(out), hostname);
  EQ(std::get<2>(out), port);
  */
}

void testHostInfo(const std::string &input, const std::string &protocol,
                  const std::string &hostname, const std::string &username,
                  const std::string &password, unsigned short port) {
  LOG_INFO("Test hostinfo: " << input);
  auto begin = input.cbegin();
  auto end = input.cend();
  RESTClient::HostInfo out;
  bool worked =
      x3::phrase_parse(begin, end, lexeme[hostinfo], x3::space, out);
  assert(worked);
  if (begin != end) {
    std::string compare_to;
    std::copy(input.cbegin(), begin, std::back_inserter(compare_to));
    std::stringstream msg;
    msg << "Failed to do a full parse: " << std::endl << "input: " << input
        << std::endl << "copyd: " << compare_to << std::endl;
    throw runtime_error(msg.str());
  }
  EQ(out.protocol, protocol);
  EQ(out.hostname, hostname);
  EQ(out.username, username);
  EQ(out.password, password);
  EQ(out.getPort(), port);
}

void testUserPass(const std::string &label, const std::string &username,
                  const std::string &password = "") {
  LOG_INFO("Test username and password: " << label);
  auto begin = label.cbegin();
  auto end = label.cend();
  std::pair<std::string, std::string> out;
  auto myRule = x3::rule<class UserPassClass, decltype(out)>() =
      x3::lexeme[userpass];
  bool worked =
      x3::phrase_parse(begin, end, myRule, x3::space, out);
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
  if (password.empty())
    assert(out.second.empty());
  else
    EQ(out.second, password);
}

int main(int , char**)
{
  testHostName("somewhere");
  testHostName("s");
  testHostName("s-aw");
  testHostName("s-w");

  testHostName("com");

  testHostName("some.host.com");
  testHostName("somewhere");
  testHostName("other-host.com");

  testHostPort("other-host.com", "other-host.com");
  testHostPort("somewhere:8080", "somewhere", 8080);
  testHostPort("somewhere.com:8000", "somewhere.com", 8000);
  testHostPort("123.100.200.300:65000", "123.100.200.300", 65000);

  testUserPass("mister@", "mister");
  testUserPass(";&=?mister@", ";&=?mister");
  testUserPass("mister@", "mister", {});
  testUserPass("Doctor:who@", "Doctor", "who");

  testLogin("other-host.com", "other-host.com", {}, {}, {});
  testLogin("somewhere:8080", "somewhere", {}, {}, 8080);
  testLogin("somewhere.com", "somewhere.com", {}, {}, {});
  using bs = boost::optional<std::string>;
  testLogin("mister:awesome@somewhere.com:8000", "somewhere.com", bs("mister"),
            bs("awesome"), 8000);
  testLogin("mister@somewhere.com:2362", "somewhere.com", bs("mister"), {},
            2362);
  testLogin("123.100.200.300:65000", "123.100.200.300", {}, {}, 65000);

  testHostInfo("http://other-host.com", "http", "other-host.com", "", "", 80);
  testHostInfo("https://somewhere:8080", "https", "somewhere", "", "", 8080);
  testHostInfo("https://somewhere.com", "https", "somewhere.com", "", "", 443);
  testHostInfo("http://mister:awesome@somewhere.com:8000", "http", "somewhere.com", "mister", "awesome", 8000);
  testHostInfo("http://mister@somewhere.com:2362", "http", "somewhere.com", "mister", "", 2362);
  testHostInfo("https://123.100.200.300:65000", "https", "123.100.200.300", "", "", 65000);

  //testLogin("mister@somewhere.co.uk", "mister", "", "somewhere.co.uk");

  testQueryWord("abc");
  testQueryPair("abc=123");

  std::string path1("/some/where");
  LOG_INFO("Testing Path: " << path1);
  test(path1, path);

  path1 = "/some/where/";
  LOG_INFO("Testing Path: " << path1);
  test(path1, path);

  std::string query("?a=1&b=2");
  LOG_INFO("Testing query string: " << query);
  QueryParameters params;
  x3::phrase_parse(query.begin(), query.end(), lexeme[query_string], x3::space,
                   params);
  EQ(params.size(), 2);
  EQ(params.at("a"), "1");
  EQ(params.at("b"), "2");

  std::string test1_s("http://somewhere.com");
  LOG_INFO("Test 1: " << test1_s);
  URL test1(test1_s);
  EQ(test1.protocol(), "http");
  EQ(test1.hostname(), "somewhere.com");
  LOG_INFO("URL Test 1 - PASSED");

  std::string test2_s("https://somewhere.com/some/path/");
  LOG_INFO("URL Test 2: " << test2_s);
  URL test2(test2_s);
  using namespace std;
  cout << "Protocol: " << test2.protocol() << endl
       << "Hostname: " << test2.hostname() << endl;
  EQ(test2.protocol(), "https");
  EQ(test2.hostname(), "somewhere.com");
  EQ(test2.path(), "/some/path/");
  LOG_INFO("Test 2 - PASSED");

  std::string megaTest1_s("https://doctor:who@somewhere.com:9000/some/path?flying=true&time=travel");
  LOG_INFO("Mega Test 1: " << megaTest1_s);
  URL megaTest1(megaTest1_s);
  EQ(megaTest1.protocol(), "https");
  EQ(megaTest1.username(), "doctor");
  EQ(megaTest1.password(), "who");
  EQ(megaTest1.hostname(), "somewhere.com");
  EQ(megaTest1.port(), 9000);
  EQ(megaTest1.path(), "/some/path");
  EQ(megaTest1.queryParameters().size(), 2);
  EQ(megaTest1.queryParameters().at("flying"), "true");
  EQ(megaTest1.queryParameters().at("time"), "travel");

  std::string megaTest2_s("https://doctor@somewhere.com:9000/some/path?");
  LOG_INFO("Mega Test 2: " << megaTest2_s);
  URL megaTest2(megaTest2_s);
  EQ(megaTest2.protocol(), "https");
  EQ(megaTest2.username(), "doctor");
  EQ(megaTest2.password(), "");
  EQ(megaTest2.hostname(), "somewhere.com");
  EQ(megaTest2.port(), 9000);
  EQ(megaTest2.path(), "/some/path");
  EQ(megaTest2.queryParameters().size(), 0);

  LOG_INFO("Mega Test 2 - PASSED");
  return 0;
}
