#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include "netutil/dump.h"
#include "netutil/dump_callout.h"

using std::cout;
using std::endl;
using std::string;

int main(int argc, char** argv) {
  if (argc <= 1) {
    dump_usage();
    dump_callout_usage();
    return 0;
  }
  string arg = argv[1];
  if (arg == "dump") {
    argc--;
    argv++;
    return dump(argc, argv);
  }
  if (arg == "dump_callout") {
    argc--;
    argv++;
    return dump_callout(argc, argv);
  }
  cout << "Invalid command." << endl;
  return 1;
}
