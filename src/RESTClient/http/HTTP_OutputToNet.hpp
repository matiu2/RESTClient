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

public:
  OutputToNet(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield) {}
  std::streamsize write(const char *s, std::streamsize n) {
    LOG_TRACE("OutputToNet writing: " << n);
    return asio::async_write(connection, asio::buffer(s, n),
                             asio::transfer_at_least(n), yield);
  }
};

} /* RESTClient */
