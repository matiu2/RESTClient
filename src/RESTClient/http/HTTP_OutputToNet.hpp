#pragma once

#include <boost/asio/write.hpp>
#include <boost/iostreams/concepts.hpp>

#include <RESTClient/base/logger.hpp>

namespace RESTClient {

namespace io = boost::iostreams;
  
/// This is a boost iostreams sink
/// Everything written to it goes out on the network
template <typename Connection> class OutputToNet : public io::sink {
private:
  Connection &connection;
  asio::yield_context &yield;
  bool writing = false;

public:
  OutputToNet(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield) {}
  std::streamsize write(const char *s, std::streamsize n) {
    LOG_TRACE("OutputToNet writing: " << n);
    size_t result = asio::async_write(connection, asio::buffer(s, n),
                                      asio::transfer_all(), yield);
    LOG_TRACE("Returning that we wrote " << result << " out of " << n);
    return result;
  }
};

} /* RESTClient */
