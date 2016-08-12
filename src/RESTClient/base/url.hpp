#pragma once

#include <ostream>
#include <map>

#include <boost/optional/optional_io.hpp>

namespace RESTClient {

using QueryParameters = std::map<std::string, std::string>;

struct HostInfo {
  using string = std::string;
  string protocol;
  string hostname;
  string username;
  string password;
  HostInfo() = default;
  HostInfo(const std::string& url);
  boost::optional<unsigned int> port;
  unsigned int getPort() const {
    if (port)
      return port.value();
    else
      return is_ssl();
  }
  bool is_ssl() const { return (protocol == "https") ? 443 : 80; }
  operator std::string() const {
    std::string result = protocol + "://";
    if ((!username.empty()) || (!password.empty()))
      result += username + ":" + password + "@";
    result += hostname;
    if (port) {
      if (((protocol == "http") && (port.get() != 80)) ||
          ((protocol == "https") && (port.get() != 443)))
        result += ":" + std::to_string(port.get());
    }
    return result;
  }
  bool operator<(const HostInfo &other) const {
    if (hostname < other.hostname)
      return true;
    if (hostname > other.hostname)
      return false;
    if (username < other.username)
      return true;
    if (password < other.password)
      return true;
    if (port > other.port)
      return true;
    return false;
  }
};

std::ostream &operator<<(std::ostream &o, const HostInfo &host);

struct URLParts {
  using string = std::string;
  HostInfo hostInfo;
  string path;
  QueryParameters queryParameters;
};

class URL {
private:
  std::string _url;
  URLParts parts;
  void parse();

public:
  URL() {}
  URL(const std::string &url) : _url(url) { parse(); }
  URL(std::string &&url) : _url(std::move(url)) { parse(); }
  const std::string &str() const { return _url; }
  URL &operator=(const std::string &in) {
    _url = in;
    parse();
    return *this;
  }
  using string = typename URLParts::string;
  const HostInfo &getHostInfo() const { return parts.hostInfo; }
  string protocol() const { return parts.hostInfo.protocol; }
  string hostname() const { return parts.hostInfo.hostname; }
  string username() const { return parts.hostInfo.username; }
  string password() const { return parts.hostInfo.password; }
  unsigned int port() const { return parts.hostInfo.getPort(); }
  string path() const { return parts.path; }
  const QueryParameters& queryParameters() const { return parts.queryParameters; }
};

std::ostream &operator<<(std::ostream &o, const URL &url);

} /* RESTClient */
