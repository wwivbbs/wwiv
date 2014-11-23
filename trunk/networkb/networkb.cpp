#include "connection.h"

#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>

using std::chrono::seconds;
using std::clog;
using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;
using wwiv::net::Connection;

static const int M_NUL  = 0;
static const int M_ADR  = 1;
static const int M_PWD  = 2;
static const int M_FILE = 3;
static const int M_OK   = 4;
static const int M_EOB  = 5;
static const int M_GOT  = 6;
static const int M_ERR  = 7;
static const int M_BSY  = 8;
static const int M_GET  = 9;
static const int M_SKIP = 10;


static string addresses;
static bool ok_received = false;

void process_command(int16_t length, Connection& c) {
  const uint8_t command_id = c.read_uint8(seconds(3));
  unique_ptr<char[]> data(new char[length]);
  c.receive(data.get(), length - 1,seconds(3));
  string s(data.get(), length - 1);
  switch (command_id) {
  case M_NUL: {
    cout << "M_NUL: " << s << endl;
  } break;
  case M_ADR: {
    cout << "M_ADR: " << s << endl;
    addresses = s;
  } break;
  case M_OK: {
    cout << "M_OK: " << s << endl;
    ok_received = true;
  } break;
  }
}

void process_data(int16_t length, Connection& c) {
  unique_ptr<char[]> data(new char[length]);
  int length_received = c.receive(data.get(), length - 1, seconds(3));
  string s(data.get(), length - 1);
  cout << "len: " << length_received << "; data: " << s << endl;
}

void process_one_frame(Connection& c) {
  uint16_t header = c.read_uint16(seconds(2));
  if (header & 0x8000) {
    process_command(header & 0x7fff, c);
  } else {
    process_data(header & 0x7fff, c);
  }
}

void maybe_process_one_frame(Connection& c) {
  try {
    process_one_frame(c);
  } catch (wwiv::net::timeout_error e) {
    clog << "process_one_frame: " << e.what() << endl;
  }
}


void send_command(Connection& c, uint8_t command_id, const string& data) {
  const std::size_t size = 3 + data.size(); /* header + command + data + null*/
  unique_ptr<char[]> packet(new char[size]);
  // Actual packet size parameter does not include the size parameter itself.
  // And for sending a commmand this will be 2 less than our actual packet size.
  uint16_t packet_length = (data.size() + sizeof(uint8_t)) | 0x8000;
  uint8_t b0 = ((packet_length & 0xff00) >> 8) | 0x80;
  uint8_t b1 = packet_length & 0x00ff;

  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  *p++ = command_id;
  memcpy(p, data.data(), data.size());

  c.send(packet.get(), size, seconds(3));
  clog << "Sending: command: " << (int) command_id
       << "; packet_length: " << (packet_length & 0x7fff)
       << "; data: " << string(packet.get(), size) << endl;
}

void send_data(Connection& c, std::size_t size, char* data) {
  // for now assume everything fits within a single frame.
  uint16_t packet_length = size & 0x7fff;
  std::unique_ptr<char[]> packet(new char[packet_length + 2]);
  uint8_t b0 = ((packet_length & 0x7f00) >> 8);
  uint8_t b1 = packet_length & 0x00ff;
  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  memcpy(p, data, size);

  c.send(packet.get(), packet_length + 2, seconds(3));
  clog << "Sending: packet_length: " << (int) packet_length
       << "; size: " << size
       << "; data: " << string(packet.get(), size + 2) << endl;
}

void ConnInit(Connection &c) {
  clog << "ConnInit" << endl;
  try {
    while (true) {
      process_one_frame(c);
    }
  } catch (wwiv::net::timeout_error e) {
    clog << e.what() << endl;
  }
}

void WaitConn(Connection &c) {
  clog << "ConnInit" << endl;
  send_command(c, M_NUL, "OPT wwivnet");
  send_command(c, M_NUL, "SYS NETWORKB test app");
  send_command(c, M_NUL, "ZYZ Unknown Sysop");
  send_command(c, M_NUL, "VER networkb/0.0 binkp/1.0");
  send_command(c, M_NUL, "LOC San Francisco, CA");
  send_command(c, M_ADR, "20000:20000/2@wwivnet");
}

void SendPasswd(Connection& c) {
  clog << "SendPasswd" << endl;

  send_command(c, M_PWD, "-");
}

void WaitAddr(Connection& c) {
  clog << "WaitAddr" << endl;
  while (addresses.empty()) {
    process_one_frame(c);
  }
}

void WaitOk(Connection& c) {
  clog << "WaitOk" << endl;
  if (ok_received) {
    return;
  }
  while (!ok_received) {
    process_one_frame(c);
  }
}

void SendDummyFile(Connection& c) {
  clog << "SendDummyFile" << endl;
  // Send a file 5 chars long with timestamp of around Sat Nov 22 15:23:00 PST 2014
  send_command(c, M_FILE, "test.txt 5 1165408993 0"); 

  // TODO(rushfan): appears that we need to process all frames waiting for us before sending the data frames
  maybe_process_one_frame(c);

  send_data(c, 5, "ABCDE");
  send_command(c, M_EOB, "End of Batch");
}

int main(int argc, char* argv) {
  try {
    Connection c("localhost", 24554);
    ConnInit(c);
    WaitConn(c);
    SendPasswd(c);
    WaitAddr(c);
    WaitOk(c);
    SendDummyFile(c);
    clog << "End of the line..." << endl;
    while (true) {
      process_one_frame(c);
    }
  } catch (wwiv::net::socket_error e) {
    clog << e.what() << std::endl;
  }
}