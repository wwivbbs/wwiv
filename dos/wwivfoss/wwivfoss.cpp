// WWIVFOSS- Runs a command under a FOSSIL emulation shell.
#include "fossil.h"
#include "pipe.h"
#include "util.h"
#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <iostream.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

class App {
public:
  App() : comport(0), node_number(0), cmdline(20) {}
  ~App() {}

  int comport;
  int node_number;
  Array cmdline;
};

static void show_help() {
  cerr << "Usage: \r\n";
  cerr << "  WWIVFOSS [args] COMMANDLINE\r\n\r\n";
  cerr << "Example: \r\n";
  cerr << "  WWIVFOSS -N2 BRE.BAT 2 Z:\\CHAIN.TXT\r\n\r\n";
  cerr << "Commands: \r\n";
  cerr << "  -N#   Node Number for this instance (1-99) [REQUIRED]" << endl;
  cerr << "  -P#   Port Number for this fossil instance (unset means any)\r\n";
  cerr << "  -?    Help - displays this information\r\n";
  cerr << endl;
}

int main(int argc, char** argv) {
  cerr << "WWIVFOSS - WWIV FOSSIL runner for Win32 and OS/2 Named Pipes\r\n";
  cerr << "           Copyright (c) 2021, WWIV Software Services\r\n";
  cerr << "           Built: " << __DATE__ << ", " << __TIME__ << "\r\n" << endl;

  App app;
  int had_positional = 0;
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    const int alen = strlen(arg);
    if (!alen) {
      cerr << "WARNING: Empty arg at position: " << i <<  endl;
      continue;
    }
    if (!had_positional && (arg[0] == '-' || arg[0] == '/')) {
      if (alen < 2) {
	      cerr << "Invalid argument: " << arg << endl;
	      continue;
      }
      // Process switch
      char schar = *(arg+1);
      const char* sval = (arg+2);
      // cerr << "Switch: " <<  schar << "; value: '" << sval << "'" << endl;
      switch (schar) {
      case 'N': {
	      // Node number
	      app.node_number = atoi(sval);
      } break;
      case 'P': {
	      // Node number
	      app.comport = atoi(sval);
      } break;
      case '?': {
        // Help
        show_help();
        return 0;
      } break;
      }
      continue;
    }
    had_positional = 1;
    // Positional arg, must be part of the commandline.
    app.cmdline.push_back(arg);
  } 

  // Args parsed, do something.
  if (app.node_number < 1) {
    cerr << "Node number not specified.  Exiting.\r\n";
    show_help();
    return 1;
  }

  enable_fossil(app.node_number, app.comport);
  int ret = _spawnvp(_P_WAIT, app.cmdline.at(0), 
                     (const char**) app.cmdline.items());
  disable_fossil();
  if (ret < 0) {
    cerr << "Error spawning process. " << endl;
    return 1;
  }
  return 0;
}
