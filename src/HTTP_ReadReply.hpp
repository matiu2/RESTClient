#pragma once

#include <boost/algorithm/string/trim.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/algorithm/string/find.hpp>

#include "HTTP_readChunk.hpp"
#include "HTTP_CopyToCout.hpp"

namespace RESTClient {

/// Reads the first line of the HTTP reply and all the headers
template <typename T>
std::tuple<bool, int> readFirstLine(T &connection, std::istream &data,
                                    asio::yield_context yield) {
  // Read in all the header data from the net
  // For later
  std::string temp, ok;
  // Read in the first line
  int code;
  data >> temp >> code >> ok;
  // Consume the '\r\n' at the end of the line
  data.get(); 
  data.get(); 
  return {ok == "OK", code};
}

/// Returns the HTTP response/error code, and 'true' if it has 'OK' at the end
/// of the reply.
/// throws std::runtime_error if there aren't three words in the reply.
/// Expects streambuf to have at least all the header info
void
readHeaders(std::istream &data, Headers &headers, asio::yield_context yield,
            std::function<void(std::istream &, std::string &)> getLine =
                [](std::istream &i, std::string &s) { std::getline(i, s); }) {
  std::string line, key, value;
  while (true) {
    // Parse it a line at a time
    getLine(data, line);
    if (line == "\r")
      return;
    auto pos = boost::find_first(line, ":");
    assert(pos);
    using namespace boost::range;
    key.clear();
    copy(make_iterator_range(line.begin(), pos.begin()), std::back_inserter(key));
    value.clear();
    copy(make_iterator_range(pos.end(), line.end()), std::back_inserter(value));
    boost::trim(key);
    boost::trim(value);
    headers.insert({key, value});
  }
}

template <typename Connection>
void readHTTPReply(HTTPResponse &result, Connection &connection,
                   asio::yield_context &yield, std::function<void()> close) {
  size_t contentLength = 0;
  // Copy the data into a line
  bool keepAlive = true; // All http 1.1 connections are keepalive unless
                         // they contain a 'Connection: close' header
  bool chunked = false;  // Chunked encoding (instead of contentLength)
  bool gzipped = false;  // incoming content is gzip encoded
  bool ok;

  // Reads the headers into the result
  asio::streambuf buf;
  asio::async_read_until(connection, buf, "\r\n\r\n", yield);
  std::istream data(&buf);
  data.exceptions(std::ios_base::failbit | std::ios_base::badbit);

  std::tie(ok, result.http_code) = readFirstLine(connection, data, yield);
  readHeaders(data, result.headers, yield);

  // Read important header values
  // Content-Length
  auto found = result.headers.find("Content-Length");
  if (found != result.headers.end())
    contentLength = std::stol(found->second);
  // Connection: close
  found = result.headers.find("Connection");
  if ((found != result.headers.end()) && (found->second == "close"))
    keepAlive = false;
  // Chunked encoding
  found = result.headers.find("Transfer-Encoding");
  if ((found != result.headers.end()) && (found->second == "chunked"))
    chunked = true;
  // gzip encoding
  found = result.headers.find("Content-Encoding");
  if ((found != result.headers.end()) && (found->second == "gzip"))
    gzipped = true;

  /// Tries to read a line, and if there's not a whole line in the buffer, gets
  /// more from the net
  auto getLine = [&](std::string &line) {
    // If there's not enough to hold a line, just get more
    if (buf.in_avail() < 2) {
      asio::async_read_until(connection, buf, "\r\n", yield);
      std::getline(data, line);
      return;
    }

    // Otherwise read carefully until we find '\n' or hit eof
    // Loop at most twice. If the first read fails, then we fill the buffer from
    // the net and continue reading
    for (int i = 0; i < 2; ++i) {
      char c = data.get();
      while (!data.eof()) {
        if (c == '\n')
          // Found \n; return
          return; 
        else
          // Not \n yet, store the character
          line.push_back(c);
        c = data.get();
      }
      // We've hit eof, read some more buffer then start again
      asio::async_read_until(connection, buf, "\r\n", yield);
      data.clear();
    }
    assert (false); // Code should never reath here
  };

  if ((!chunked) && (contentLength > 0)) {
    // Read a straight content length body
    size_t bytesToRead = contentLength - buf.in_avail();
    readChunk(connection, contentLength, gzipped, buf, data, result.body, yield);
  } else {
    // The body is chunked
    while (true) {
      // read the chunk size (in hex (16 base) ascii numbers)
      // eg. F means 16
      std::string line;
      getLine(line); // Use our slightly smarter getline
      size_t chunkSize = std::stoul(line, 0, 16);
      if (chunkSize == 0)
        break;

      std::ostream& body = result.body;;
      auto start = body.tellp();
      readChunk(connection, chunkSize, gzipped, buf, data, body, yield);
      // Read an empty line
      char c;
      data.get(c);
      assert(c = '\r');
      data.get(c);
      assert(c = '\n');
    }
    // See if we have any trailing headers after the chunks
    readHeaders(data, result.headers, yield,
                [&getLine](std::istream &, std::string &s) {
      s.clear();
      getLine(s);
    });
  }

  // We don't want to read from the next request's buffer
  assert(buf.in_avail() == 0); 
  // Close connection if that's what the server wants
  if (!keepAlive)
    close();
  // If the result was bad
  if (!ok)
    throw HTTPError(result.http_code, result.body);
}

} /* RESTClient */

