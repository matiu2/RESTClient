#include "HTTP.hpp"

#include <sstream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/search.hpp>
#include <boost/range/istream_range.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/system/error_code.hpp>

#include <boost/asio/ssl/rfc2818_verification.hpp>

#ifdef HTTP_ON_STD_OUT
#include <iostream>
#endif

namespace RESTClient {

namespace io = boost::iostreams;

/// This is a boost iostreams source. It reads from the SSL/socket connection
template <typename Connection>
class NetReader : public io::source {
private:
  Connection &connection;
  asio::yield_context &yield;
  bool count_bytes = false;
  size_t totalBytesRead = 0;
  size_t bytes_to_read = 0;
  MAKE THIS INTO A LINE READER, THEN AFTER IT READS A BLANK LINE, IT'LL REQUIRE A SIZE IN ORDER TO CONTINUE READING.

public:
  NetReader(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield) {}
  std::streamsize read(char *s, std::streamsize n) {
    // If we've read all the body, let the stream know that its ended and not
    // going to get any more data out of us
    if ((count_bytes) && (bytes_to_read == 0))
      return 0;

    // Read whatever's in the buffer
    size_t bytes_read;
    if (bytes_to_read)
      // If we know how much we need, just wait for them to come
      bytes_read = asio::async_read(connection, asio::buffer(s, n),
                                    asio::transfer_exactly(bytes_to_read), yield);
    else
      // If we don't know, just give the steam whatever's in the buffer
      bytes_read = asio::async_read(connection, asio::buffer(s, n),
                                    asio::transfer_at_least(1), yield);
    // If we're counting bytes countdown to stream end
    if (bytes_to_read > 0)
      bytes_to_read -= bytes_read;
    return bytes_read;
  }
  /// Start counting down the number of bytes until the end of the stream
  /// In this case until the end of the body
  void startCountdown(size_t n_bytes=0) {
    count_bytes = true;
    bytes_to_read = n_bytes;
  }

  /// Expect n_bytes to come (eg. if we know the body or chunk size)
  /// This makes us just wait for the number of bytes we care about
  /// ignores values over 4k to save RAM, assuming that those bodies will be
  /// going to files anyway
  void expect(size_t n_bytes) {
    // Don't want to use too much RAM, when saving to file,
    // so don't expect more than 4K
    if (n_bytes < 4096)
      bytes_to_read = n_bytes;
  }
};

/// Convenience function to make a NetReader
template <typename Connection>
NetReader<Connection> makeNetReader(Connection &connection,
                                    asio::yield_context &yield) {
  return NetReader<Connection>(connection, yield);
}

/// A boost iostreams filter; it's a gzip decompressor, but can be turned on and
/// off, defaults to off
class OptionalGZipDecompressor {
public:
    typedef char              char_type;
    typedef io::multichar_input_filter_tag category;

    bool enabled = false;
    io::gzip_decompressor worker;

    template <typename Source>
    std::streamsize read(Source &src, char_type *s, std::streamsize n) {
      if (enabled)
        return worker.read(src, s, n);
      else
        return io::read(src, s, n);
    }

    /// Enable compression
    void enable() { enabled = true; }
    /// Disable compression
    void disable() { enabled = false; }
};

#ifdef HTTP_ON_STD_OUT
/// Copies all incoming text to cout
class CopyIncomingToCout {
public:
    typedef char              char_type;
    typedef io::input_filter_tag  category;

    bool first = true; // Is this the first char in a line ?
    bool disabled = false;

    template <typename Source> int get(Source &src) {
      char_type c = io::get(src);
      if (!disabled) {
        if (first)
          std::cout << "> ";
        std::cout.put(c);
        first = (c == '\n');
      }
      return c;
    }
};

/// Copies all outgoing text to cout
class CopyOutgoingToCout {
public:
    typedef char              char_type;
    typedef io::output_filter_tag  category;

    bool first = true; // Is this the first char in a line ?

    template <typename Sink> bool put(Sink &dest, int c) {
      if (first)
        std::cout << "< ";
      std::cout.put(c);
      first = (c == '\n');
      io::put(dest, c);
      return true;
    }
};
#endif

/// This is a boost iostreams sink
/// Everything written to it goes out on the network
template <typename Connection>
class OutputToNet : public io::sink {
private:
  Connection &connection;
  asio::yield_context &yield;
public:
  OutputToNet(Connection &connection, asio::yield_context &yield)
      : connection(connection), yield(yield) {}
  std::streamsize write(const char* s, std::streamsize n) {
    return asio::async_write(connection, asio::buffer(s, n),
                             asio::transfer_at_least(n), yield);
  }
};

/// Convenience function to create an OutputToNet sink
template <typename Connection>
OutputToNet<Connection> make_output_to_net(Connection &connection,
                                           asio::yield_context &yield) {
  return OutputToNet<Connection>(connection, yield);
}


/// Adds the default HTTP headers to a request
void HTTP::addDefaultHeaders(HTTPRequest& request) {
  // Host
  std::string* value = &request.headers["Host"];
  if (value->empty())
    *value = hostName;
  // Accept */*
  value = &request.headers["Accept"];
  if (value->empty())
    *value = "*/*";
  // Accept-Encoding: gzip, deflate
  value = &request.headers["Accept-Encoding"];
  if (value->empty())
    *value = "gzip, deflate";
  // TE: trailers
  value = &request.headers["TE"];
  if (value->empty())
    *value = "trailers";
  // Content-Length
  long size = request.body.size();
  if (size >= 0) {
    value = &request.headers["Content-Length"];
    if (value->empty())
      *value = std::to_string(size);
  }
}

/// Transmits a body with 'chunked' transfer-encoding (untested because
/// httpbin.org doesn't support it, but cloud files API does, we'll have to
/// write some tests that connect to cloud files api later)
void chunkedTransmit(filtering_ostream &transmitter, std::istream &data,
                     asio::yield_context yield) {
  // https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
  const size_t bufferSize = 4096;
  char buffer[bufferSize];
  while (data.good()) {
    data.read(buffer, bufferSize);
    size_t bytes = data.gcount();
    if (bytes == 0)
      break;
    // Write the chunksize
    transmitter << std::ios_base::hex << bytes << "\r\n";
    // Write the data
    io::copy(io::restrict(data, 0, bytes), transmitter);
    // Write a new line
    transmitter << "\r\n";
  }
}

/// Transmits a body. If the body size is positive (not -1), it'll send it in
/// one go. A negative number indicates that we don't know the size, so it
/// should be sent in chunked transfer-encoding
void transmitBody(filtering_ostream &transmitter, HTTPRequest &request,
                  asio::yield_context yield) {
  // If we know the body size
  std::istream& body = request.body;
  if (request.body.size() >= 0)
    io::copy(body, transmitter);
  else
    chunkedTransmit(transmitter, body, yield);
}

/// Handles an HTTP action (verb) GET/POST/ etc..
HTTPResponse HTTP::action(HTTPRequest& request, std::string filePath) {
  ensureConnection();
  addDefaultHeaders(request);
  output << request.verb << " " << request.path << " HTTP/1.1" << "\r\n";

  for (const auto &header : request.headers)
    output << header.first << ": " << header.second << "\r\n";
  output << "\r\n";
  output.flush();

  HTTPResponse result;
  if (!filePath.empty())
    result.body.initWithFile(filePath);
  transmitBody(output, request, yield);
  readHTTPReply(input, result);

  return result;
}

/// Reads the first line of the HTTP reply.
/// Returns the HTTP response/error code, and 'true' if it has 'OK' at the end
/// of the reply.
/// throws std::runtime_error if there aren't three words in the reply.
std::pair<int, bool> readFirstLine(const std::string& line) {
  // First line should be of the format: HTTP/1.1 200 OK
  std::vector<boost::iterator_range<std::string::const_iterator>> words;
  boost::split(words, line, boost::is_any_of(" \t"));
  if (words.size() != 3)
    throw std::runtime_error(
        std::string("Expected 'HTTP/1.1 200 OK' but got '") + line + "'");
  auto word = words.begin();
  if (*word != std::string("HTTP/1.1"))
    throw std::runtime_error(
        std::string("Expected 'HTTP/1.1' in response but got: ") + line);
  // Read the return code and 'OK'
  ++word;
  int code = boost::lexical_cast<int>(*word);
  ++word;
  bool ok = *word == std::string("OK");
  return {code, ok};
}

std::string HTTPError::lookupCode(int code) {
  // TODO: Maybe translate the http error code into a useful message
  // Maybe copy how it's done in curlpp11 where you pass an error code callback
  // handler
  std::stringstream result;
  result << "Code: (" << code << "). We don't translate HTTP error codes yet. Look it up on "
         "https://en.wikipedia.org/wiki/List_of_HTTP_status_codes";
  return std::move(result.str());
}

HTTP::HTTP(std::string hostName, asio::io_service &io_service,
           tcp::resolver &resolver, asio::yield_context yield, bool is_ssl)
    : hostName(hostName), io_service(io_service), resolver(resolver),
      yield(yield), is_ssl(is_ssl), ssl_context(ssl::context::sslv23),
      sslStream(io_service, ssl_context), socket(io_service) {
  ssl_context.set_default_verify_paths();
  sslStream.set_verify_mode(ssl::verify_peer);
  sslStream.set_verify_callback(ssl::rfc2818_verification(hostName));
}

HTTP::~HTTP() {
  close();
}

void HTTP::ensureConnection() {
  // Resolve if needed
  if (endpoints == decltype(endpoints)())
    endpoints =
        resolver.async_resolve({hostName, is_ssl ? "https" : "http"}, yield);
  // Connect if needed
  if (is_ssl) {
    if (!sslStream.lowest_layer().is_open()) {
      asio::async_connect(sslStream.lowest_layer(), endpoints, yield);
      // Perform SSL handshake and verify the remote host's
      // certificate.
      sslStream.async_handshake(decltype(sslStream)::client, yield);
    }
  } else {
    if (!socket.is_open())
      asio::async_connect(socket, endpoints, yield);
  }
  makeInput();
  makeOutput();
}

HTTPResponse HTTP::get(std::string path) {
  HTTPRequest request("GET", path);
  return action(request);
}

HTTPResponse HTTP::getToFile(std::string serverPath, const std::string &filePath) {
  HTTPRequest request("GET", serverPath);
  return action(request, filePath);
}

HTTPResponse HTTP::del(std::string path) {
  HTTPRequest request("DELETE", path);
  return action(request);
}

HTTPResponse HTTP::PUT_OR_POST(std::string verb, std::string path,
                               std::string data) {
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  output << verb << " " << path << " HTTP/1.1" << "\r\n";
  output << "Host: " << hostName << "\r\n";
  output << "Accept: */*" << "\r\n";
  output << "Accept-Encoding: gzip, deflate" << "\r\n";
  output << "TE: trailers" << "\r\n";
  if (data.size() > 0)
    output << "Content-Length: " << data.size() << "\r\n";
  output << "\r\n";
  HTTPResponse result;
  ensureConnection();
  output << data;
  readHTTPReply(input, result);
  return result;

}

HTTPResponse HTTP::put(const std::string path, std::string data) {
  HTTPRequest request("PUT", path, {}, data);
  return action(request);
}

HTTPResponse HTTP::post(const std::string path, std::string data) {
  return PUT_OR_POST("POST", path, data);
}

HTTPResponse HTTP::PUT_OR_POST_STREAM(std::string verb, std::string path,
                                      std::istream &data) {
  boost::asio::streambuf buf;
  std::ostream request(&buf);
  // TODO: urlencode ? parameters ? other headers ? chunked data support
  request << verb << " " << path << " HTTP/1.1" << "\r\n";
  request << "Host: " << hostName << "\r\n";
  request << "Accept: */*" << "\r\n";
  request << "Accept-Encoding: gzip, deflate" << "\r\n";
  request << "TE: trailers" << "\r\n";
  // Find the stream size
  data.seekg(0, std::istream::end);
  long size = data.tellg();
  data.seekg(0);
  if (size != -1)
    request << "Content-Length: " << size << "\r\n";
  request << "\r\n";
  #ifdef HTTP_ON_STD_OUT
  std::cout << std::endl << "> ";
  for (auto& part : buf.data())
    std::cout.write(boost::asio::buffer_cast<const char *>(part),
                    boost::asio::buffer_size(part));
  #endif
  HTTPResponse result;
  ensureConnection();
  io::copy(buf, output);
  io::copy(data, output);
  readHTTPReply(input, result);
  return result;
}

struct CharBufSource : public io::source {
    
  std::vector<char> &data;
  decltype(data.begin()) pos;

  CharBufSource(std::vector<char> &data) : data(data), pos(data.begin()) {}

  std::streamsize read(char *s, std::streamsize n) {
    size_t toRead = std::min(n, data.end() - pos);
    std::copy(pos, pos + toRead, s);
    return toRead;
  }

};

void HTTP::readHTTPReply(filtering_istream &input, HTTPResponse &result) {
  size_t contentLength = 0;
  // Copy the data into a line
  bool keepAlive = true; // All http 1.1 connections are keepalive unless
                         // they contain a 'Connection: close' header
  bool chunked = false;  // Chunked encoding (instead of contentLength)
  bool gzipped = false;  // incoming content is gzip encoded

  std::string line;
  auto getLine = [&input, &line ]() -> std::string & {
    std::getline(input, line);
    line.pop_back(); // Strip off the '\r' on the end
    return line;
  };
  bool ok;
  std::tie(result.http_code, ok) = readFirstLine(getLine());

  // Reads a header into key and value. Trims spaces off both sides
  std::string key;
  std::string value;
  auto readHeader = [&]() {
    // Get the next line
    getLine();
    // If the line is empty, there are no more headers, go read the body
    if (line == "")
      return false;
    // Read the headers into key+value pairs
    auto colon = std::find(line.begin(), line.end(), ':');
    if (colon == line.end())
      throw std::runtime_error(std::string("Unable to read header (no colon): ") + line);
    key.reserve(std::distance(line.begin(), colon));
    value.reserve(std::distance(colon, line.end()));
    std::copy(line.begin(), colon, std::back_inserter(key));
    std::copy(colon + 1, line.end(), std::back_inserter(value));
    boost::trim(key);
    boost::trim(value);
    return true;
  };

  // Reads the headers into the result
  auto readHeaders = [&]() {
    while (true) {
      if (!readHeader())
        break;
      // Check for headers we care about for the transfer
      if (key == "Content-Length")
        contentLength = std::stoi(value);
      else if ((key == "Connection") && (value == "close"))
        keepAlive = false;
      else if ((key == "Transfer-Encoding") && (value == "chunked"))
        chunked = true;
      else if ((key == "Content-Encoding") && (value == "gzip"))
        gzipped = true;
      // Store the headers
      result.headers.emplace(std::move(key), std::move(value));
    }
  };

  readHeaders();

  std::ostream& body = result.body;

  auto readChunk = [&](size_t size) {
    if (gzipped) {
      io::filtering_istream bodyReader;
#ifdef HTTP_ON_STD_OUT
      // Print the unzipped content
      bodyReader.push(CopyIncomingToCout());
#endif
      bodyReader.push(io::gzip_decompressor());
#ifdef HTTP_ON_STD_OUT
      // Disable output of gzipped content
      CopyIncomingToCout *coutCopier = input.component<CopyIncomingToCout>(0);
      assert(coutCopier);
      coutCopier->disabled = true;
#endif
      bodyReader.push(io::restrict(input, size));
      io::copy(bodyReader, body);
    } else {
      for (size_t i=0; i<size; ++i)
        body.put(input.get());
    }
  };

  // If we have a contentLength, read that many bytes
  if (chunked) {
    // Read the chunk size;
    getLine();
    contentLength = stoul(line, 0, 16);
    while (contentLength != 0) {
      // TODO: maybe read 'extended chunk' data one day
    if (is_ssl) {
      auto src = input.component<NetReader<decltype(sslStream)>>(2);
      assert(src);
      src->expect(contentLength);
    } else {
      auto src = input.component<NetReader<decltype(socket)>>(2);
      assert(src);
      src->expect(contentLength);
    }
      readChunk(contentLength);
      std::string emptyLine;
      std::getline(input, emptyLine);
      if (emptyLine != "\r")
        throw std::runtime_error(
            "chunked encoding read error. Expected empty line.");
      getLine(); // Read the next chunk header
      contentLength = std::stoul(line, 0, 16);
    }
    // Read extra headers if there are any
    readHeaders();
  } else {
    // Copy the initial buffer contents to the body
    if (is_ssl) {
      auto src = input.component<NetReader<decltype(sslStream)>>(2);
      assert(src);
      src->startCountdown(contentLength);
    } else {
      auto src = input.component<NetReader<decltype(socket)>>(2);
      assert(src);
      src->startCountdown(contentLength);
    }
    readChunk(contentLength);
  }
  // Close connection if that's what the server wants
  if (!keepAlive)
    close();
  // If the result was bad
  if (!ok)
    throw HTTPError(result.http_code, result.body);
}

void HTTP::makeInput() {
  input.reset();
#ifdef HTTP_ON_STD_OUT
  input.push(CopyIncomingToCout());
#endif
  input.push(OptionalGZipDecompressor());
  if (is_ssl)
    input.push(makeNetReader(sslStream, yield));
  else
    input.push(makeNetReader(socket, yield));
}

void HTTP::makeOutput() {
  output.reset();
#ifdef HTTP_ON_STD_OUT
  output.push(CopyOutgoingToCout());
#endif
  if (is_ssl)
    output.push(make_output_to_net(sslStream, yield));
  else
    output.push(make_output_to_net(socket, yield));
}

HTTPResponse HTTP::putStream(std::string path, std::istream& data) {
  return PUT_OR_POST_STREAM("PUT", path, data);
}

HTTPResponse HTTP::postStream(std::string path, std::istream& data) {
  return PUT_OR_POST_STREAM("POST", path, data);
}

//HTTPResponse HTTP::patch(const std::string path, std::string data);

bool HTTP::is_open() const {
  if (is_ssl)
    return sslStream.lowest_layer().is_open();
  else
    return socket.is_open();
}

void HTTP::close() {
  if (sslStream.lowest_layer().is_open()) {
    boost::system::error_code ec;
    sslStream.async_shutdown(yield[ec]);
    sslStream.lowest_layer().close();
    // Short read is not a real error. Everything else is.
    if (ec.category() == asio::error::get_ssl_category() &&
        ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ))
      return;
    if (ec.category() == boost::system::system_category() &&
        ec.value() == boost::system::errc::success)
      return;
    throw boost::system::system_error(ec);
  } else if (socket.is_open()) {
    socket.close();
  }
}

} /* RESTClient */

