#pragma once

#include <ostream>

#include <boost/spirit/home/x3.hpp>
#include <boost/range.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

namespace RESTClient {

namespace ast {

struct URLParts {
  // TODO: change this to boost::iterator_range<const char> or something like that
  using string = std::string;
  string protocol;
  string hostname;
  string username;
  string password;
  string port;
  string path;
  std::map<string, string> queryParameters;
};

using boost::fusion::operator<<;

}
}

BOOST_FUSION_ADAPT_STRUCT(RESTClient::ast::URLParts,
                          (std::string, protocol)(std::string, hostname));

namespace RESTClient {

namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

using x3::char_;
using x3::lit;
using x3::alnum;
using x3::hex;
using x3::string;

x3::rule<class url, ast::URLParts> const url = "url";

auto const protocol = string("https") | string("http");
auto const normal_char = ~char_("?/%");
auto const quoted_char = (lit('%') >> hex >> hex);
auto const hostname = +(normal_char | quoted_char);
auto const url_def = protocol >> lit("://") >> hostname; //  >> -(path) >> -(query);

BOOST_SPIRIT_DEFINE(url);

class URL {
private:
  std::string _url;
  ast::URLParts _parts;

  void parse() {
    x3::phrase_parse(_url.begin(), _url.end(), url_def, x3::space, _parts);
  }
public:
  URL(const std::string& url) : _url(url) {
    parse();
  }
  URL(std::string&& url) : _url(std::move(url)) {
    parse();
  }
  const ast::URLParts &parts() const { return _parts; };
  const std::string &str() const { return _url; }
};

std::ostream &operator<<(std::ostream &o, const URL &url) {
  o << url.str();
  return o;
}

} /* RESTClient */
