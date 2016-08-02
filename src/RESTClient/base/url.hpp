#pragma once

#include <ostream>
#include <map>

#include <boost/optional/optional_io.hpp>

namespace RESTClient {

using QueryParameters = std::map<std::string, std::string>;

struct URLParts {
  // TODO: change this to boost::iterator_range<const char> or something like
  // that
  using string = std::string;
  string protocol;
  string hostname;
  string username;
  string password;
  boost::optional<unsigned int> _port;
  unsigned int port() const {
    if (_port)
      return _port.value();
    else
      return (protocol == "https") ? 443 : 80;
  }
  string path;
  QueryParameters queryParameters;
};


class URL {
private:
  std::string _url;
  URLParts _parts;

  void parse();
public:
  URL(const std::string& url) : _url(url) {
    parse();
  }
  URL(std::string&& url) : _url(std::move(url)) {
    parse();
  }
  const URLParts &parts() const { return _parts; };
  const std::string &str() const { return _url; }
  URL &operator=(const std::string &in) {
    _url = in;
    parse();
    return *this;
  }
};

std::ostream &operator<<(std::ostream &o, const URL &url) {
  o << url.str();
  return o;
}

} /* RESTClient */
