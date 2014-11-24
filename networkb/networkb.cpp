#include "networkb/binkp.h"
#include "networkb/connection.h"
#include "networkb/socket_connection.h"
#include "networkb/socket_exceptions.h"

#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using wwiv::net::socket_error;
using wwiv::net::SocketConnection;
using wwiv::net::BinkSide;
using wwiv::net::BinkP;


int main(int argc, char** argv) {
  try {
    SocketConnection c("localhost", 24554);
    BinkP binkp(&c, BinkSide::ORIGINATING, "20000:20000/1@wwivnet");
    binkp.Run();
  } catch (socket_error e) {
    std::clog << e.what() << std::endl;
  }
}
