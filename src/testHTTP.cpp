#include "HTTP.hpp"

#include <iostream>
#include <sstream>

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

void testGet(asio::io_service& io_service, tcp::resolver& resolver, bool is_ssl, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  RESTClient::HTTPResponse response = server.get("/get");
  assert(response.body.empty());
}

void testChunkedGet(asio::io_service &io_service, tcp::resolver &resolver,
                    bool is_ssl, asio::yield_context yield) {
  RESTClient::HTTP server("httpbin.org", io_service, resolver, yield, is_ssl);
  const int size = 1024;
  const int chunk_size = 80;
  std::stringstream path;
  path << "/range/" << size << "?duration=1&chunk_size=" << chunk_size;
  RESTClient::HTTPResponse response = server.get(path.str());
  // Make up the expected string
  const std::string block = "abcdefghijklmnopqrstuvwxyz";
  std::stringstream expected;
  int i = 0;
  size_t block_size = block.size();
  while (i < size) {
    int toWrite = size - i;
    if (toWrite >= block.size()) {
      expected << block;
      i += block_size;
    } else {
      std::copy_n(block.begin(), toWrite,
                  std::ostream_iterator<char>(expected));
      i += toWrite;
    }
  }
  if (expected.str() != response.body) {
    std::stringstream msg;
    using std::endl;
    msg << "Received body not the same as the expected body. "
        << "Expected Length: " << size << endl
        << "Actual Length: " << response.body.size() << endl << endl
        << "=== Expected data begin === " << endl << expected.str() << endl
        << "=== Expected data end ===" << endl
        << "=== Actual data begin === " << endl << response.body << endl
        << "=== Actual data end ===" << endl;
    throw std::runtime_error(msg.str());
  }
}

int main(int argc, char *argv[]) {
  asio::io_service io_service;
  tcp::resolver resolver(io_service);

  using namespace std::placeholders;

  auto runTests = [&](bool is_ssl) {
    // HTTP get
    //asio::spawn(io_service, std::bind(testGet, std::ref(io_service), std::ref(resolver), is_ssl, _1));
    // HTTPS chunked get
    asio::spawn(io_service, std::bind(testChunkedGet, std::ref(io_service),
                                      std::ref(resolver), is_ssl, _1));
  };

  // HTTP tests
  runTests(false);
  // HTTPS tests
  //runTests(false);
  io_service.run();
  return 0;
}
