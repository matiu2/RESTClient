#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

namespace RESTClient {

struct HTTPBaseBody {
  virtual ~HTTPBaseBody(){};
};

struct HTTPStreamBody : public HTTPBaseBody {
  std::iostream* body;
};

struct HTTPFileBody : public HTTPStreamBody {
  std::fstream file;
  HTTPFileBody(std::fstream &&input) : file(std::move(input)) {
    HTTPStreamBody::body = &file;
  }
  HTTPFileBody(std::string path)
      : file(path, std::fstream::in | std::fstream::out) {
    HTTPStreamBody::body = &file;
  }
};

struct HTTPStringStreamBody : public HTTPStreamBody {
  std::stringstream data;
  HTTPStringStreamBody(std::stringstream&& input) : data(std::move(input)) {}
  HTTPStringStreamBody(const std::string& input) {
    data << input;
    HTTPStreamBody::body = &data;
  }
};

struct HTTPStringBody : public HTTPBaseBody {
  HTTPStringBody(std::string value="") : body(std::move(value)) {}
  std::string body;
};

struct HTTPBody {
  enum class Type {string, stream, empty};
  std::unique_ptr<HTTPBaseBody> body;
  /// Called when downloading. It'll consume from the buffer and append into our
  /// body
  void consumeData(std::string &buffer);
  /// Return true if the body has been initialized
  operator bool() const { return body != nullptr; }
  /// Initialize the body with a file stream
  void initWithFile(const std::string &path);
  /// If the body is stored as a string, return a pointer to it. If the body is
  /// not initialized, initialize it as a string, then return the pointer to it.
  /// Otherwise return nullptr.
  std::string *asString();
  /// If the body is stored as a string, return a const pointer to it.
  /// Otherwise return nullptr.
  const std::string *asStringConst() const;
  /// Turn the body into a string (copies the input value)
  HTTPBody &operator=(const std::string &value);
  /// Turn the body into a stringstream
  HTTPBody &operator=(std::stringstream &&value);
  /// Turn the body into a file stream
  HTTPBody &operator=(std::fstream &&value);
  /// Copy the body into a new string
  operator std::string() const;
  /// Get a reference to a stream. If the body is stored in a string, move it
  /// into a stream and return that.
  operator std::iostream &();
  /// If it's a stream flush it
  void flush();
  /// If it's an fstream close it
  void close();
  /// Return the size of the body. -1 means we don't know. 0 means there is no
  /// body. positive values are the body size. You should never ever get any
  /// other negative values.
  long size();
  /// Return the format in which the body is stored
  Type type() const;
};

} /* RESTClient */
