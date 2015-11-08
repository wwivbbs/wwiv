#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <io.h>
#include <string>
#include <vector>
#include "netutil/dump.h"

using std::cout;
using std::endl;
using std::string;

int main(int argc, char** argv) {
  if (argc <= 1) {
    dump_usage();
    return 0;
  }
  string arg = argv[1];
  if (arg == "dump") {
    argc--;
    argv++;
    return dump(argc, argv);
  }
  cout << "Invalid command." << endl;
  return 1;
}
