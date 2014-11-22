#include "connection.h"

#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>

using std::clog;
using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;
using wwiv::net::Connection;

static const int M_NUL = 0;
static const int M_OK  = 1;

void process_command(int16_t length, Connection& c) {
  const uint8_t command_id = c.read_uint8();
  unique_ptr<char[]> data(new char[length]);
  c.receive(data.get(), length - 1);
  switch (command_id) {
  case M_NUL: {
    string s(data.get(), length - 1);
    cout << "M_NUL: " << s << endl;
  } break;
  }
}

void process_data(int16_t length, Connection& c) {
  unique_ptr<char[]> data(new char[length]);
  int length_received = c.receive(data.get(), length - 1);
  cout << "len: " << length_received << "; data: " << data.get() << endl;
}

int main(int argc, char* argv) {
  try {
    Connection c("localhost", 24554);
    while (true) {
      uint16_t header = c.read_uint16();
      if (header & 0x8000) {
        process_command(header & 0x7fff, c);
      } else {
        process_data(header & 0x7fff, c);
      }
    }
  } catch (wwiv::net::socket_error e) {
    clog << e.what() << std::endl;
  }
}