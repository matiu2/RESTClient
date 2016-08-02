#include "url.hpp"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>

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

x3::rule<class url, URLParts> const url = "url";

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
auto const mid_string = *(mid >> &(mid));
auto const domainlabel = alnum >> -(mid_string >> alnum);
auto const toplabel = alpha >> -(mid_string >> alnum);
// NOTE: Top label should dissallow num in the first char
auto const urlpath = *xchar;
auto const hostnumber = digits >> char_('.') >> digits >> char_('.') >>
                        digits >> char_('.') >> digits;
auto const hostname = *(domainlabel >> char_('.')) >> toplabel;
auto const host = hostnumber | hostname;
auto const port = ushort_;
auto const hostport = host >> -(':' >> port);
auto const user_string = +(uchar - (lit(':') | '@') | char_(';') | char_('?') |
                           char_('&') | char_('='));
auto const userpass =
    user_string >> ((':' >> user_string) | string(""));
auto const login = -(userpass >> '@') >> hostport;

// Misc
auto const protocol = string("https") | string("http");
auto const path = char_('/') >> +(~char_('?'));

// Query part
auto const query_word = +(~char_("?&="));
auto const query_pair = query_word >> '=' >> query_word;
auto const query_def = lit('?') >> -(query_pair % '&');
auto const url_def =
    lexeme[protocol >> "://" >> login >> (path | attr("")) >> -query_def];

BOOST_SPIRIT_DEFINE(url);

void URL::parse() {
  x3::phrase_parse(_url.begin(), _url.end(), url_def, x3::space, _parts);
}
}

BOOST_FUSION_ADAPT_STRUCT(
    RESTClient::URLParts,
    (std::string, protocol)(std::string, username)(std::string, password)(
        std::string, hostname)(boost::optional<unsigned int>, _port)(
        std::string, path)(RESTClient::QueryParameters, queryParameters));
