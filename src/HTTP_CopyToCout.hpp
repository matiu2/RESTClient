#pragma once

#include <iostream>
#include <boost/iostreams/concepts.hpp>

namespace RESTClient {

namespace io = boost::iostreams;

/// Copies all incoming text to cout
class CopyIncomingToCout {
public:
  typedef char char_type;
  typedef io::input_filter_tag category;

  bool first = true; // Is this the first char in a line
  bool disabled = false;

  template <typename Source> int get(Source &src) {
    char_type c = io::get(src);
    if (!disabled) {
      if (first)
        std::cout << "> ";
      std::cout.put(c);
      first = (c == '\n');
    }
    return c;
  }
};

/// Copies all outgoing text to cout
class CopyOutgoingToCout {
public:
  typedef char char_type;
  typedef io::output_filter_tag category;

  bool first = true; // Is this the first char in a line ?

  template <typename Sink> bool put(Sink &dest, int c) {
    if (first)
      std::cout << "< ";
    std::cout.put(c);
    first = (c == '\n');
    io::put(dest, c);
    return true;
  }
};
  
} /* RESTClient */
