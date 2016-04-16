// uses boost:asio to get some http data

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/buffered_read_stream.hpp>
#include <iostream>
#include <memory>
#include <iterator>
#include <iostream>

namespace a = boost::asio;
using namespace boost::asio::ip; // to get 'tcp::'


int main(int argc, char *argv[]) {
  try {
    a::io_service io_service;
    tcp::resolver resolver(io_service);

    a::spawn(io_service, [&](a::yield_context yield) {
      // http://www.boost.org/doc/libs/1_60_0/doc/html/boost_asio/reference/generic__stream_protocol/socket.html
      auto endpoint = resolver.async_resolve({"httpbin.org", "http"}, yield);
      tcp::socket socket(io_service);
      a::async_connect(socket, endpoint, yield);
      std::string request("GET / HTTP/1.1\r\n"
                          "Accept: */*\r\n"
                          "Accept-Encoding: gzip, deflate\r\n\r\n");
      a:async_write(socket, a::buffer(request), yield);
      // Now get the response
      std::array<char, 128> buf;
      int bytes = a::async_read(socket, a::buffer(buf), yield); 
      using namespace std;
      cout << "Got this: ";
      std::copy(buf.begin(), buf.end(), std::ostream_iterator<char>(cout));
      cout << endl;
    });

    io_service.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
