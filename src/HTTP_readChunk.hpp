#pragma once

#ifdef HTTP_ON_STD_OUT
#include "HTTP_CopyToCout.hpp"
#endif

#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

namespace RESTClient {

namespace io = boost::iostreams;

template <typename Connection>
void readChunk(Connection &connection, size_t chunkSize, bool gzipped,
               asio::streambuf &buf, std::istream &rawIn, std::ostream &body,
               asio::yield_context yield) {
  // Work out how much we need to put in the buffer
  size_t avail = rawIn.rdbuf()->in_avail();
  size_t bytesToRead = 0;
  if (avail < chunkSize)
    // If there's not enough in the buffer, read some more
    bytesToRead = chunkSize - avail;
  // Download from net to buf
  asio::async_read(connection, buf, asio::transfer_at_least(bytesToRead),
                   yield);
  // Create the stream
  io::filtering_istream bodyStream;
#ifdef HTTP_ON_STD_OUT
  // Print the unzipped content
  bodyStream.push(CopyIncomingToCout());
#endif
  if (gzipped)
    bodyStream.push(io::gzip_decompressor());

  auto start = rawIn.tellg();
  bodyStream.push(io::restrict(rawIn, 0, chunkSize));
  // Copy to real body
  io::copy(bodyStream, body);
}

} /* RESTCLient */
