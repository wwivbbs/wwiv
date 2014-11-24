#include "networkb/socket_exceptions.h"

#include <stdexcept>
#include "core/strings.h"

using std::string;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

connection_error::connection_error(const string& host, int port) 
  : socket_error(StringPrintf("Error connecting to: %s:%d", host.c_str(), port)) {}

}  // namespace net
} // namespace wwiv
