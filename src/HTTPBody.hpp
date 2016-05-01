#pragma once

#include <iostream>
#include <fstream>
#include <string>

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
  std::unique_ptr<HTTPBaseBody> body;
  /// Called when downloading. It'll consume from the buffer and append into our
  /// body
  void consumeData(std::string &buffer) {
    auto asString = dynamic_cast<HTTPStringBody *>(body.get());
    if (asString) {
      std::string &input = asString->body;
      input.reserve(input.size() + buffer.size());
      input.append(buffer);
    } else {
      auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
      if ((!asStream) || (!asStream->body))
        throw std::runtime_error("Unkown HTTPBody type. Cannot find stream");
      std::iostream &input = *asStream->body;
      std::copy(buffer.begin(), buffer.end(),
                std::ostream_iterator<char>(input));
    }
    buffer.clear();
  }
  /// Return true if the body has been initialized
  operator bool() const { return body != nullptr; }
  /// Initialize the body with a file stream
  void initWithFile(const std::string &path) {
    *this = std::move(std::fstream(path, std::fstream::in | std::fstream::out));
  }
  /// If the body is stored as a string, return a pointer to it. If the body is
  /// not initialized, initialize it as a string, then return the pointer to it.
  /// Otherwise return nullptr.
  std::string *asString() {
    if (!body)
      body.reset(new HTTPStringBody());
    auto tmp = dynamic_cast<HTTPStringBody *>(body.get());
    return (tmp != nullptr) ? &tmp->body : nullptr;
  }
  /// Turn the body into a string (copies the input value)
  HTTPBody &operator=(const std::string &value) {
    body.reset(new HTTPStringBody(value));
    return *this;
  }
  /// Turn the body into a stringstream
  HTTPBody &operator=(std::stringstream &&value) {
    body.reset(new HTTPStringStreamBody{std::move(value)});
    return *this;
  }
  /// Turn the body into a file stream
  HTTPBody &operator=(std::fstream &&value) {
    body.reset(new HTTPFileBody(std::move(value)));
    return *this;
  }
  /// Copy the body into a new string
  operator std::string() const {
    auto asString = dynamic_cast<HTTPStringBody *>(body.get());
    if (asString)
      return asString->body;
    auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
    if (!asStream)
      return "";
    if (!asStream->body)
      return "";
    auto stringStream = dynamic_cast<std::stringstream *>(asStream->body);
    if (stringStream)
      return stringStream->str();
    std::string result;
    std::copy(std::istream_iterator<char>(*asStream->body),
              std::istream_iterator<char>(), std::back_inserter(result));
    return std::move(result);
  }
  /// Get a reference to a stream. If the body is stored in a string, move it
  /// into a stream and return that.
  operator std::iostream &() {
    auto asString = dynamic_cast<HTTPStringBody *>(body.get());
    if (asString) {
      // If it came as a string, turn into a stringstream
      body.reset(new HTTPStringStreamBody(asString->body));
    }
    auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
    if ((!asStream) || (!asStream->body))
      throw std::runtime_error("Unkown HTTPBody type. Cannot find stream");
    return *asStream->body;
  }
  /// If it's a stream flush it
  void flush() {
    auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
    if ((asStream) && (asStream->body))
      asStream->body->flush();
  }
  /// If it's an fstream close it
  void close() {
    auto asFile = dynamic_cast<HTTPFileBody *>(body.get());
    if (asFile)
      asFile->file.close();
  }
};

} /* RESTClient */
