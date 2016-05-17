#include "HTTPBody.hpp"

namespace RESTClient {

void HTTPBody::consumeData(std::string &buffer) {
  auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
  if (!asStream)
    throw std::runtime_error("Unkown HTTPBody type. Cannot find stream");
  std::ostream &saving = asStream->writing();
  saving.write(buffer.c_str(), buffer.size());
  buffer.clear();
}

/// Initialize the body with a file stream
void HTTPBody::initWithFile(const std::string &path) {
  body.reset(new HTTPFileBody(path));
}

/// Turn the body into a string (copies the input value)
HTTPBody &HTTPBody::operator=(std::string value) {
  body.reset(new HTTPStringStreamBody(std::move(value)));
  return *this;
}

/// Turn the body into a stringstream
HTTPBody &HTTPBody::operator=(std::stringstream &&value) {
  body.reset(new HTTPStringStreamBody{std::move(value)});
  return *this;
}

/// Copy the body into a new string
HTTPBody::operator std::string() const {
  auto streamBody = dynamic_cast<HTTPStreamBody *>(body.get());
  if (!streamBody)
    return "";
  auto* stringStream = dynamic_cast<std::stringstream*>(&streamBody->reading());
  if (stringStream)
    return stringStream->str();
  else {
    std::istream& in = streamBody->reading();
    in.seekg(0, std::ios_base::end);
    auto size = in.tellg();
    in.seekg(0);
    std::string result;
    result.reserve(size);
    std::copy(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>(), std::back_inserter(result));
    return result;
  }
}

/// Get a reference to a stream for reading from. If the body is stored in a string, move it
/// into a stream and return that.
HTTPBody::operator std::istream &() {
  auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
  if (!asStream)
    throw std::runtime_error("Unkown HTTPBody type. Cannot find stream");
  return asStream->reading();
}

/// Get a reference to a stream for reading from. If the body is stored in a string, move it
/// into a stream and return that.
HTTPBody::operator std::ostream &() {
  auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
  if (!asStream)
    throw std::runtime_error("Unkown HTTPBody type. Cannot find stream");
  return asStream->writing();
}

/// If it's a stream flush it
void HTTPBody::flush() {
  auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
  if (asStream)
    asStream->writing().flush();
}

long HTTPBody::size() {
  auto asStream = dynamic_cast<HTTPStreamBody *>(body.get());
  if (!asStream)
    return 0;
  std::istream& data = asStream->reading();
  data.seekg(0, std::istream::end);
  long size = data.tellg();
  data.seekg(0);
  return size;
}

} /* RESTClient */
