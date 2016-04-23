#include "HTTP.hpp"

#include <iostream>

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

void testHTTPGet(asio::io_service& io_service, tcp::resolver& resolver, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, false);
  RESTClient::HTTPResponse response = server.get("/get");
  using namespace std;
  cout << "Got the body: " << response.body << endl;
}

int main(int argc, char *argv[]) {
  asio::io_service io_service;
  tcp::resolver resolver(io_service);

  using namespace std::placeholders;
  asio::spawn(io_service, std::bind(testHTTPGet, std::ref(io_service),
                                    std::ref(resolver), _1));

  io_service.run();
  return 0;
}
