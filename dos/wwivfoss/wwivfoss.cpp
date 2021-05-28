// WWIVFOSS- Runs a command under a FOSSIL emulation shell.
#include "fossil.h"
#include "pipe.h"
#include "util.h"
#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma warning(disable : 4505)

class App {
public:
  App() : opts(), cmdline(20) {}
  ~App() {}

  FossilOptions opts;
  Array cmdline;
};

static void show_help() {
  fprintf(stderr,
    "Usage: \r\n"
    "  WWIVFOSS [args] COMMANDLINE\r\n\r\n"
    "Example: \r\n"
    "  WWIVFOSS -N2 BRE.BAT 2 Z:\\CHAIN.TXT\r\n\r\n"
    "Commands: \r\n"
    "  -N#   Node Number for this instance (1-99) [REQUIRED]\r\n"
    "  -P#   Port Number for this fossil instance (unset means any)\r\n"
    "  -?    Help - displays this information\r\n"
    "\r\n");
}

extern char __near* fossil_info_ident;

int main(int argc, char** argv) {
  fprintf(stderr, "WWIVFOSS: ");
  fprintf(stderr, ((char __near*)fossil_info_ident));
  fprintf(stderr, "\r\n"
      "          Copyright (c) 2021, WWIV Software Services\r\n"
      "          Built: " __DATE__ ", " __TIME__ "\r\n\r\n");

  App app;
  int had_positional = 0;
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    const int alen = strlen(arg);
    if (!alen) {
      log("WARNING: Empty arg at position: %d", i);
      continue;
    }
    if (!had_positional && (arg[0] == '-' || arg[0] == '/')) {
      if (alen < 2) {
	      log("Invalid argument: '%s'", arg);
	      continue;
      }
      // Process switch
      char schar = (char)toupper(*(arg+1));
      const char* sval = (arg+2);
      // cerr << "Switch: " <<  schar << "; value: '" << sval << "'" << endl;
      switch (schar) {
      case 'N': {
	      // Node number
	      app.opts.node_number = atoi(sval);
      } break;
      case 'P': {
	      // Node number
	      app.opts.comport = atoi(sval);
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
  if (app.opts.node_number < 1) {
    fprintf(stderr, "Node number not specified.  Exiting.\r\n");
    show_help();
    return 1;
  }

  if (!enable_fossil(&app.opts)) {
    log("Failed to initialize FOSSIL support.");
    return 2;
  }
  
  // Don't call _heapmin, or things crash later.
  int ret = _spawnvp(_P_WAIT, app.cmdline.at(0), 
                     (const char**) app.cmdline.items());
  disable_fossil();
  if (ret < 0) {
    log("Error spawning process. ");
    return 3;
  }
  return 0;
}
