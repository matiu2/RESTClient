#include "Services.hpp"

namespace RESTClient {

Services* globalServices = nullptr;

Services::Services() : io_service() {
  globalServices = this;
}

Services::~Services() {
  globalServices = nullptr;
}

Services* Services::instance() {
  // You need to create a services instance in your main function before you can use it
  assert(globalServices);
  return globalServices;
}

} /* RESTClientt */
