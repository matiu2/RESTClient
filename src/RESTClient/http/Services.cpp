#include "Services.hpp"

namespace RESTClient {

std::unique_ptr<Services> globalServices;

Services::Services() : io_service(), resolver(io_service) {}

Services& Services::instance() {
  if (!globalServices)
    globalServices.reset(new Services());
  // You need to create a services instance in your main function before you can use it
  return *globalServices;
}

} /* RESTClientt */
