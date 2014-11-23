#include "binkp.h"
#include "connection.h"

#include <fcntl.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>


int main(int argc, char* argv) {
  try {
    wwiv::net::Connection c("localhost", 24554);
    wwiv::net::BinkP binkp(&c);
    binkp.Run();
  } catch (wwiv::net::socket_error e) {
    std::clog << e.what() << std::endl;
  }
}