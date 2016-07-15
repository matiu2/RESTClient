#pragma once

#include <boost/spirit/home/x3.hpp>

namespace RESTClient {

namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

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

class URL {
private:
  std::string _url;
  URLParts _parts;
  void parse() {

    using x3::char_;

    auto const uri = +char_;

    x3::phrase_parse(_url.begin(), _url.end(), uri, x3::space, _parts.hostname);
  }
public:
  URL(const std::string& url) : _url(url) {
    parse();
  }
  URL(std::string&& url) : _url(std::move(url)) {
    parse();
  }
  const URLParts& parts() { return _parts; };
};


} /* RESTClient */
