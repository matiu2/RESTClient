#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <cassert>

namespace RESTClient {

struct HTTPBaseBody {
  virtual ~HTTPBaseBody(){};
};

struct HTTPStreamBody : public HTTPBaseBody {
  virtual std::ostream& writing() = 0;
  virtual std::istream& reading() = 0;
};

struct HTTPFileBody : public HTTPStreamBody {
  std::string path;
  std::ofstream _writing;
  std::ifstream _reading;
  HTTPFileBody(std::string path)
      : path(path)  {
    _writing.exceptions(std::fstream::failbit | std::fstream::badbit);
    _reading.exceptions(std::fstream::failbit | std::fstream::badbit);
  }
  virtual std::istream &reading() override {
    if (_writing.is_open())
      _writing.flush();
    if (!_reading.is_open())
      _reading.open(path, std::fstream::in | std::fstream::binary);
    return _reading;
  }
  virtual std::ostream &writing() override {
    if (!_writing.is_open())
      _writing.open(path, std::fstream::out | std::fstream::binary);
    return _writing;
  }
};

struct HTTPStringStreamBody : public HTTPStreamBody {
  std::stringstream data;
  HTTPStringStreamBody(std::stringstream&& input) : data(std::move(input)) {}
  HTTPStringStreamBody(const std::string &input) { data << input; }
  virtual std::istream& reading() override { return data; }
  virtual std::ostream& writing() override { return data; }
};

struct HTTPStringBody : public HTTPBaseBody {
  HTTPStringBody(std::string value="") : body(std::move(value)) {}
  std::string body;
};

struct HTTPBody {
  HTTPBody() {}
  HTTPBody(std::string data) : body(new HTTPStringBody(std::move(data))) {}
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
  /// Turn the body into a string
  HTTPBody &operator=(std::string value);
  /// Turn the body into a stringstream
  HTTPBody &operator=(std::stringstream &&value);
  /// Turn the body into a file stream
  HTTPBody &operator=(std::fstream &&value);
  /// Copy the body into a new string
  operator std::string() const;
  /// Get a reference to a stream for reading. If the body is stored in a
  /// string, move it into a stream and return that.
  operator std::istream &();
  /// Get a reference to a stream for writing. If the body is stored in a
  /// string, move it into a stream and return that.
  operator std::ostream &();
  /// If it's a stream flush it
  void flush();
  /// Return the size of the body. -1 means we don't know. 0 means there is no
  /// body. positive values are the body size. You should never ever get any
  /// other negative values.
  long size();
  /// Return the format in which the body is stored
  Type type() const;
};

} /* RESTClient */
