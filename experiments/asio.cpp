// uses boost:asio to get some http data

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/istream_range.hpp>
#include <iostream>
#include <sstream>

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

/// Response from an HTTP request
struct HTTPResponse {
  int http_code;
  std::vector<std::string> headers;
  std::string body;
};

class HTTPError : public std::runtime_error {
private:
  static const char *lookupCode(int code) {
    return "We don't translate HTTP error codes yet. Look it up on "
           "https://en.wikipedia.org/wiki/List_of_HTTP_status_codes";
  };

public:
  HTTPError(int code) : std::runtime_error(lookupCode(code)), code(code) {}
  int code;
};

int main(int argc, char *argv[]) {
  try {
    asio::io_service io_service;
    tcp::resolver resolver(io_service);

    asio::spawn(io_service, [&](asio::yield_context yield) {
      // http://www.boost.org/doc/libs/1_60_0/doc/html/boost_asio/reference/generic__stream_protocol/socket.html
      auto endpoint = resolver.async_resolve({"httpbin.org", "http"}, yield);
      tcp::socket socket(io_service);
      asio::async_connect(socket, endpoint, yield);
      std::string request("GET /get HTTP/1.1\r\n"
                          "Host: httpbin.org\r\n"
                          "Accept: */*\r\n"
                          "Accept-Encoding: gzip, deflate\r\n\r\n");
      asio::async_write(socket, asio::buffer(request), yield);

      // Now get the response
      bool closeWhenDone = false;
      asio::streambuf buf;
      std::istream data(&buf);
      HTTPResponse info;
      unsigned int contentLength = 0;
      // Copy the data into a line
      int bytes = asio::async_read_until(socket, buf, "\n", yield);
      std::string line; // TODO: find a way to not do so much copying of data
      // First line should be of the format: HTTP/1.1 200 OK
      data >> line;  // Reads one word, should be HTTP/1.1
      if (line != "HTTP/1.1")
        throw std::runtime_error(
            std::string("Expected 'HTTP/1.1' in response but got: ") + line);
      data >> info.http_code; // Reads the return code
      data >> line; // Reads OK
      if (line != "OK")
        throw HTTPError(info.http_code);
      // Second line should be empty
      bytes = asio::async_read_until(socket, buf, "\n", yield);
      getline(data, line);
      boost::trim(line);
      if (!line.empty())
        throw std::runtime_error(
            "Expected second HTTP response line to be empty");
      while (true) {
        // Get the next line
        bytes = asio::async_read_until(socket, buf, "\n", yield);
        getline(data, line);
        boost::trim(line);
        // If the line is empty, there are no more headers, go read the body
        if (line.empty())
          break;
        // Check headers we care about
        auto header =
            boost::make_split_iterator(line, first_finder(":", is_iequal()));
        if (algorithm::icontains(*header, "content-length"))
          contentLength = boost::lexical_cast<decltype(contentLength)>(boost::trim_copy(*++header));
        else if (algorithm::icontains(*header, "Connection"))
          if (algorithm::icontains(*++header, "keep-alive"))
            closeWhenDone = true;
        // TODO: check for gzip in response headers
        // Store the headers
        info.headers.emplace_back(std::move(line));
      }
      // Now read the body
      if (contentLength != 0) {
        bytes = 0;
        // Fills our info.body with what ever's in the buffer
        auto fillBody = [&]() {
          auto start = std::istreambuf_iterator<char>(&buf);
          decltype(start) end;
          bytes += buf.in_avail();
          info.body.reserve(info.body.size() + buf.in_avail());
          std::copy(start, end, std::back_inserter(info.body));
        };
        // Copy the initial buffer contents to the body
        fillBody();
        while (bytes < contentLength) {
          // Fill the buffer
          bytes += asio::async_read(socket, buf, yield);
          // Copy that into the body
          fillBody();
        }
      }

      using namespace std;
      cout << "DONE:" << endl << "HTTP response code: " << info.http_code
           << endl << "Headers: " << endl;
      for (const auto &header : info.headers)
        cout << "    " << header << endl;
      cout << "Body: " << endl << info.body << endl;

      if (closeWhenDone)
        socket.close();
      /*
      // Now get the response
      std::vector<std::string> headers;
      std::string body;
      asio::streambuf buf;
      std::istream data(&buf);
      auto dataRange = boost::istream_range<char>(data);

      unsigned int contentLength = 0;
      int bytes = asio::async_read_until(socket, buf, "\n", yield);
      auto line = boost::make_split_iterator(
          dataRange,
          first_finder("\r\n", is_iequal()));
      using namespace std;
      std::string bam;
      std::copy(dataRange.begin(), dataRange.end(), std::back_inserter(bam));
      cout << "First line: " << bam << endl;
      decltype(line) eos;
      while (true) {
        // Read a header line
        auto header =
            boost::make_split_iterator(*line, first_finder(":", is_iequal()));
        bytes = asio::async_read_until(socket, buf, "\n", yield);
        auto name = *header++;
        using namespace std;
        cout << "Header: " << name << endl;
        // If there's no ':' in the header jump to the body
        if (header == eos)
          break;
        auto value = *header;
        cout << "Value: " << value << endl;
        // Parse the header
        if (algorithm::icontains(name, "content-length")) {
          contentLength = boost::lexical_cast<decltype(contentLength)>(value);
          cout << "GOT CONTENT LENGTH: " << contentLength << endl;
        }
      } */
    });

    io_service.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
