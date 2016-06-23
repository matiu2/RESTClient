#include "Services.hpp"

namespace RESTClient {

pServices globalServices;

pServices Services::instance() {
  if (!globalServices)
    globalServices.reset(new Services());
  return globalServices;
}

} /* RESTClientt */
