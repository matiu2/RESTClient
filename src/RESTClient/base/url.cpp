#include "url.hpp"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>

#include "url_parser.hpp"

BOOST_FUSION_ADAPT_STRUCT(RESTClient::HostInfo, (std::string, protocol),
                          (std::string, username), (std::string, password),
                          (std::string, hostname),
                          (boost::optional<unsigned int>, port));

namespace RESTClient {

std::ostream &operator<<(std::ostream &o, const HostInfo &host) {
  o << (std::string)host;
  return o;
}

std::ostream &operator<<(std::ostream &o, const URL &url) {
  o << url.str();
  return o;
}

namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

HostInfo::HostInfo(const std::string &url) {
  auto it = url.begin();
  bool ok = x3::phrase_parse(it, url.end(), hostinfo, x3::space, *this);
  assert(ok);
}

void URL::parse() {
  auto it = _url.begin();
  bool ok =
      x3::phrase_parse(it, _url.end(), hostinfo, x3::space, parts.hostInfo);
  assert(ok);
  if (it != _url.end()) {
    std::string path_out;
    ok = x3::phrase_parse(it, _url.end(), RESTClient::path, x3::space,
                          parts.path);
    assert(ok);
    if (it != _url.end()) {
      ok = x3::phrase_parse(it, _url.end(), RESTClient::query_string, x3::space,
                            parts.queryParameters);
      assert(ok);
    }
  }
  assert(it == _url.end());
}
}

