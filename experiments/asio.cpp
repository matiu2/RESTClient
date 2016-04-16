// uses boost:asio to get some http data

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <memory>

namespace a = boost::asio;
using namespace boost::asio::ip; // to get 'tcp::'

class session : public std::enable_shared_from_this<session> {
public:
  explicit session(tcp::socket socket)
      : socket_(std::move(socket)), timer_(socket_.get_io_service()),
        strand_(socket_.get_io_service()) {}

  void go() {
    auto self(shared_from_this());
    spawn(strand_, [this, self](a::yield_context yield) {
      try {
        char data[128];
        for (;;) {
          timer_.expires_from_now(std::chrono::seconds(10));
          std::size_t n =
              socket_.async_read_some(a::buffer(data), yield);
          a::async_write(socket_, a::buffer(data, n),
                                   yield);
        }
      } catch (std::exception &e) {
        socket_.close();
        timer_.cancel();
      }
    });

    a::spawn(strand_, [this, self](a::yield_context yield) {
      while (socket_.is_open()) {
        boost::system::error_code ignored_ec;
        timer_.async_wait(yield[ignored_ec]);
        if (timer_.expires_from_now() <= std::chrono::seconds(0))
          socket_.close();
      }
    });
  }

private:
  tcp::socket socket_;
  a::steady_timer timer_;
  a::io_service::strand strand_;
};

int main(int argc, char *argv[]) {
  try {
    a::io_service io_service;
    tcp::resolver resolver(io_service);

    a::spawn(io_service, [&](a::yield_context yield) {
      // http://www.boost.org/doc/libs/1_60_0/doc/html/boost_asio/reference/generic__stream_protocol/socket.html
      auto endpoint = resolver.async_resolve({"httpbin.org", "http"}, yield);
      tcp::socket socket(io_service);
      a::async_connect(socket, endpoint, yield);
    });

    io_service.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
