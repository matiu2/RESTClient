#include "HTTP.hpp"

#include <iostream>

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

template <bool ssl>
void testHTTPGet(asio::io_service& io_service, tcp::resolver& resolver, bool is_ssl, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  RESTClient::HTTPResponse response = server.get("/get");
  assert(response.body.empty());
}

int main(int argc, char *argv[]) {
  asio::io_service io_service;
  tcp::resolver resolver(io_service);

  using namespace std::placeholders;
  // HTTP get
  asio::spawn(io_service, std::bind(testHTTPGet<false>, std::ref(io_service),
                                    std::ref(resolver), false, _1));
  // HTTPS get
  asio::spawn(io_service, std::bind(testHTTPGet<true>, std::ref(io_service),
                                    std::ref(resolver), true, _1));

  io_service.run();
  return 0;
}
