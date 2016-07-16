#pragma once

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
using x3::string;

x3::rule<class url, ast::URLParts> const url = "url";

auto const protocol = string("http") | string("https");
auto const hostname = +(alnum) % char_(".");
auto const url_def = protocol >> lit("://") >> hostname;

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
  const ast::URLParts& parts() { return _parts; };

};


} /* RESTClient */
