#pragma once

#include <ostream>
#include <map>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/range.hpp>
#include <boost/spirit/home/x3.hpp>

namespace RESTClient {

namespace ast {

using QueryParameters = std::map<std::string, std::string>;

struct URLParts {
  // TODO: change this to boost::iterator_range<const char> or something like
  // that
  using string = std::string;
  string protocol;
  string hostname;
  string username;
  string password;
  string port;
  string path;
  QueryParameters queryParameters;
};

using boost::fusion::operator<<;

}
}

BOOST_FUSION_ADAPT_STRUCT(RESTClient::ast::URLParts, (std::string, protocol),
                          (std::string, hostname), (std::string, path)
                          //(RESTClient::ast::QueryParameters, queryParameters)
                          );

namespace RESTClient {

namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

using x3::char_;
using x3::lit;
using x3::alpha;
using x3::alnum;
using x3::digit;
using x3::hex;
using x3::string;
using x3::ushort_;
using x3::attr;
using x3::lexeme;
using x3::eoi;
using x3::space;

x3::rule<class url, ast::URLParts> const url = "url";
x3::rule<class query, ast::QueryParameters> const query = "query";

// from RFC1738
// characters

auto const safe = char_("$-_.+");
auto const extra = char_("!*'(),");
auto const national = char_("{}|\\^~[]`");
auto const punctuation = char_("<>#%\"");
auto const reserved = char_(";/?:@&=");
auto const escape = "%" > hex > hex;

auto const unreserved = alnum | safe | extra;
auto const uchar = unreserved | escape;
auto const xchar = unreserved | reserved | escape;
auto digits = +digit;

// hostname part
auto const mid = alnum | char_('-');
auto const mid_string = *(mid >> !(eoi | space | '.' | '/' | '?'));
auto const domainlabel = alnum >> -(mid_string >> alnum);
auto const toplabel = alpha >> -(mid_string >> alnum);
// NOTE: Top label should dissallow num in the first char
auto const user = *(uchar | char_(";?&="));
auto const password = *(uchar | char_(";?&="));
auto const urlpath = *xchar;
auto const hostnumber = digits >> char_('.') >> digits >> char_('.') >>
                        digits >> char_('.') >> digits;
auto const hostname = x3::rule<class hostname, std::string>() =
    +(domainlabel >> char_('.')) >> toplabel;
auto const host = hostname | hostnumber;
auto const port = ushort_;
auto const hostport = host >> -(':' >> port);
auto const login = -(user >> -(':' >> password)) >> hostport;

    // Older bits
    auto const protocol = string("https") | string("http");
auto const normal_char = ~char_("?/%");
auto const quoted_char = (lit('%') >> hex >> hex);
auto const path = char_('/') >> +(~char_('?'));
// Query part
auto const query_word = x3::rule<class query_word, std::string>() =
    +(~char_("?&="));
auto const query_pair =
    x3::rule<class query_pair, std::pair<std::string, std::string>>() =
        query_word >> '=' >> query_word;
auto const query_def =
    lit('?') >> *(query_pair) % '&';
auto const url_def = protocol >> "://" >> hostname >> (path | attr(""));
                     //(query_def | attr(std::map<std::string, std::string>()));

BOOST_SPIRIT_DEFINE(query, url);

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
