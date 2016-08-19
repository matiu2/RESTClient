/// Spirit parsers for URL

#include <string>
#include <boost/spirit/home/x3.hpp>

#include "url.hpp"

namespace RESTClient {

using namespace boost::spirit::x3;

// Tags
class quoted_char_tag;
class path_tag;
class user_or_pass_tag;
class hostname_tag;

// Rules
rule<quoted_char_tag, char> const quoted_char = "quoted_char";
rule<path_tag, std::string> const path = "path";
rule<user_or_pass_tag, std::string> const user_or_pass = "user_or_pass";
rule<hostname_tag, std::string> const hostname = "hostname";

// Parsers
auto const protocol = string("https") | string("http");
auto const port = lit(':') >> ushort_;
auto const host_terminator = lit('?') | '/' | eoi;
auto const host_terminator_all = host_terminator | &port;
auto const hostname_def = +~char_("?/:");
auto const quoted_char_def = '%' >> hex;
auto const path_def = char_('/') >> +(~char_("?%") | quoted_char) >> &(eoi | '?');

// userpass
auto const user_or_pass_def = +~char_(":@") >> !host_terminator_all;
auto const userpass = (user_or_pass >> -(':' >> user_or_pass) >> '@');

// query_string
auto const query_word = +(~char_("&=%") | quoted_char);
auto const query_pair =
    rule<class query_pair_tag, std::pair<std::string, std::string>>() =
        query_word >> '=' >> query_word;
auto const query_string =
    rule<class query_string_tag, std::map<std::string, std::string>>() =
        lit('?') >> -(query_pair % '&');

auto const hostport = hostname >> -port >> &host_terminator;
auto const hostinfo = protocol >> "://" >> -userpass >> hostport;
auto const pathquery = -path >> -query_string;
auto const url = hostinfo >> pathquery;

// Binding
BOOST_SPIRIT_DEFINE(quoted_char, path, user_or_pass, hostname);

} /* RESTClient */
