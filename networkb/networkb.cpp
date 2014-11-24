#include "binkp.h"
#include "connection.h"
#include "socket_connection.h"
#include "socket_exceptions.h"

#include <fcntl.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>

using wwiv::net::socket_error;
using wwiv::net::SocketConnection;
using wwiv::net::BinkP;

int main(int argc, char** argv) {
  try {
    SocketConnection c("localhost", 24554);
    BinkP binkp(&c);
    binkp.Run();
  } catch (socket_error e) {
    std::clog << e.what() << std::endl;
  }
}
