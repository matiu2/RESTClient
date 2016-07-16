#include <RESTClient/base/url.hpp>
#include <RESTClient/base/logger.hpp>

#include <string>
#include <cassert>

using namespace std;
using namespace RESTClient;

int main(int , char**)
{
  URL test1("http://somewhere.com");
  using namespace std;
  cout << "Protocol: " << test1.parts().protocol << endl
       << "Hostname: " << test1.parts().hostname << endl;
  //assert(test1.protocol == "http");
  //assert(test1.hostname == "somewhere.com");
  return 0;
}
