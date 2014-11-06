#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <io.h>
#include <string>
#include "bbs/net.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

int main(int argc, char** argv) {
  if (argc < 2) {
    cout << "Usage:   dumppacket <filename>" << endl;
    cout << "Example: dumppacket S1.NET" << endl;
    return 2;
  }

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
    if (num_read != sizeof(net_header_rec)) {
      cerr << "error reading header, got short read of size: " << num_read 
           << "; expected: " << sizeof(net_header_rec) << endl;
      return 1;
    }
    cout << "destination: " << h.touser << "@" << h.tosys << endl;
    cout << "from:        " << h.fromuser << "@" << h.fromsys << endl;
    cout << "type:        " << h.main_type << "." << h.minor_type << endl;
    cout << "list_len:    " << h.list_len << endl;
    cout << "daten:       " << h.list_len << endl;
    cout << "length:      " << h.length << endl;
    cout << "method:      " << h.method << endl << endl;
    // skip text.
    lseek(f, h.length, SEEK_CUR);
    if (eof(f)) {
      done = true;
    }
  }
  close(f);
  return 0;
}
