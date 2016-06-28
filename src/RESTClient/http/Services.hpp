#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace RESTClient {

using namespace boost;
using namespace boost::asio::ip; // to get 'tcp::'

struct Services;

struct Services {
  asio::io_service io_service;
  static Services* instance();
  Services();
  ~Services();
};

  
} /* RESTClient */
