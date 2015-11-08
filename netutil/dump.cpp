#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <io.h>
#include <string>
#include <vector>
#include "sdk/net.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

void dump_usage() {
  cout << "Usage:   dumppacket <filename>" << endl;
  cout << "Example: dumppacket S1.NET" << endl;
}

int dump(int argc, char** argv) {
  if (argc < 2) {
    cout << "Usage:   dumppacket <filename>" << endl;
    cout << "Example: dumppacket S1.NET" << endl;
    return 2;
  }
  cout << sizeof(net_header_rec) << endl;
  const string filename(argv[1]);
  int f = open(filename.c_str(), _O_BINARY | _O_RDONLY);
  if (f == -1) {
    cerr << "Unable to open file: " << filename << endl;
    return 1;
  }

  bool done = false;
  while (!done) {
    net_header_rec h;
    int num_read = read(f, &h, sizeof(net_header_rec));
    if (num_read == 0) {
      // at the end of the packet.
      cout << "[End of Packet]" << endl;
      return 0;
    }
    if (num_read != sizeof(net_header_rec)) {
      cerr << "error reading header, got short read of size: " << num_read 
           << "; expected: " << sizeof(net_header_rec) << endl;
      return 1;
    }
    cout << "destination: " << h.touser << "@" << h.tosys << endl;
    cout << "from:        " << h.fromuser << "@" << h.fromsys << endl;
    cout << "type:        " << h.main_type << "." << h.minor_type << endl;
    cout << "list_len:    " << h.list_len << endl;
    cout << "daten:       " << h.daten << endl;
    cout << "length:      " << h.length << endl;
    cout << "method:      " << h.method << endl;

    if (h.list_len > 0) {
      // read list of addresses.
      std::vector<uint16_t> list;
      list.resize(h.list_len);
      int list_num_read = read(f, &list[0], 2 * h.list_len);
      for (const auto item : list) {
        cout << item << " ";
      }
      cout << endl;
    }
    if (h.length > 0) {
      string text;
      text.resize(h.length + 1);
      int text_num_read = read(f, &text[0], h.length);
      cout << "Text:" << endl << text << endl << endl;
    }
    if (eof(f)) {
      done = true;
    }
    cout << endl;
  }
  close(f);
  return 0;
}
