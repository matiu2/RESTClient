#include <RESTClient/base/url.hpp>
#include <RESTClient/base/logger.hpp>

#include <string>
#include <cassert>

using namespace std;
using namespace RESTClient;

int main(int , char**)
{
  URL test1("http://somewhere.com");
  //assert(test1.protocol == "http");
  //assert(test1.hostname == "somewhere.com");
  return 0;
}
