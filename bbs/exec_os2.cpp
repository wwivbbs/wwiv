/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/exec.h"

#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#include <os2.h>

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"

#include <cctype>
#include <fcntl.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <process.h>
#include <string>
#include <unistd.h>

/*
  Control protocol:

  From  To     MSG   Meaning
 +-----+------+------+--------------------------+
  DOS   OS/2   H      Heartbeat (still alive)
  DOS   OS/2   D      Disconnect (not sure how it'd know)
  OS/2  DOS    D      Disconnect (drop carrier)
  DOS   OS/2   R      FOSSIL Signalled a reboot is needed.
  DOS   OS/2   T      Enable telnet control processing
  DOS   OS/2   B      Switch to binary mode (no telnet processing)
 */

static constexpr int PIPE_BUFFER_SIZE = 4000;

static int saved_key = -1;

static bool shutdown_pipes = false;

static void os_yield() {
  DosSleep(1);
}

static HFILE create_pipe(const char *name) {
  char pipe_name[200];
  sprintf(pipe_name, "\\PIPE\\WWIV%s", name);
  HFILE hPipe;
  auto rc = DosCreateNPipe((const unsigned char*) pipe_name,
			   &hPipe,
			   NP_ACCESS_DUPLEX,
			   NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 0xFF,
			   PIPE_BUFFER_SIZE,
			   PIPE_BUFFER_SIZE,
			   0); // 0 == No Wait.
  VLOG(2) << "create_pipe(" <<  pipe_name << "); handle: " << hPipe;
  return hPipe;
}

static bool connect_pipe(HFILE h) {
  VLOG(2) << "Waiting for pipe to connect: [handle: " << h;
  int i = 0;
  auto rc = NO_ERROR;
  do {
    if (i++ > 1000) {
      LOG(ERROR) << "Failed to connect to pipe: " << h;
      // Failed to connect to pipe.
      return false;
    }
    rc = DosConnectNPipe(h);
    if (rc != 0) {
      // Sleep for 100ms
      DosSleep(100);
      os_yield();
    }
  } while (rc != NO_ERROR);
  VLOG(1) << "connected to pipe: " << h;
  DosSleep(100);
  return true;
}

static int close_pipe(HFILE h) {
  VLOG(2) << "close_pipe(" << h << ")";
  return DosDisConnectNPipe(h);
}

void _System dos_pipe_loop(long unsigned int) {
  VLOG(2) << "dos_pipe_loop()";
  int node_num = a()->sess().instance_number();
  char pipe_name[81];
  sprintf(pipe_name, "%d", node_num);
  auto h = create_pipe(pipe_name);
  if (!connect_pipe(h)) {
    LOG(ERROR) << "Failed to connect to data pipe.";
    return;
  }
  strcat(pipe_name, "C");
  auto hc = create_pipe(pipe_name);
  if (!connect_pipe(hc)) {
    LOG(ERROR) << "Failed to connect to control pipe.";
    return;
  }

  bool stop = false;
  unsigned long num_written;
  char buffer[200];
  do {
    VLOG(2) << "LOOP: dos_pipe_loop()";
    if (bout.remoteIO()->incoming()) {
      // This isn't likely super performant.
      const auto num_read = bout.remoteIO()->read(buffer, sizeof(buffer));
      unsigned long num_written;
      auto rc = DosWrite(h, buffer, num_read, &num_written);
      VLOG(4) << "Wrote bytes to pipe: " << num_written;
      if (rc != NO_ERROR) {
	      os_yield();
	      //log("Error Writing to the pipe: %d", rc);
      }
    }

    char ch;
    ULONG num_read;

    // Read Data
    if (auto rc = DosRead(h, &ch, 1, &num_read); 
      	rc == NO_ERROR && num_read > 0) {
      VLOG(4) << "Read bytes from pipe: " << num_read;
      const auto num_written = bout.remoteIO()->write(&ch, 1);
      continue;
    } else if (rc != 232 && rc != 0) {
      // Only yield if we didn't do anything.
      LOG(ERROR) << "Error on DosRead: " << rc;
      os_yield();
      stop = true;
    } else {
      os_yield();
    }

    // Read Control
    
    if (auto rc = DosRead(hc, &ch, 1, &num_read); rc == NO_ERROR && num_read > 0) {
      VLOG(1) << "Read from control: " << ch;
      switch (ch) {
      case 'D': 
      	// Forced disconnect.
        stop = true;
        VLOG(1) << "Force Disconnect came from control.";
        break;
      case 'H': {
	    // Heartbeat
      } break;
      }
    } else if (rc != 232 && rc != 0) {
      LOG(ERROR) << "Error on control DosRead: " << rc;
      stop = true;
    } else {
      os_yield();
    }

    auto crc = DosEnterCritSec();
    if (shutdown_pipes) {
      stop = true;
    }
    crc = DosEnterCritSec();

    if (stop) {
      VLOG(1) << "Stopping Pipes";
      break;
    }

  } while (!stop);

  close_pipe(h);
  close_pipe(hc);
  VLOG(1) << "dos_pipe_loop: exited";
}


static void StartFOSSILPipe() {
  VLOG(2) << "StartFOSSILPipe";
  TID tid = 0;
  DosCreateThread( &tid, dos_pipe_loop, 0, CREATE_READY | STACK_SPARSE, 8196 );
  DosSleep(100);
}

static void StopFOSSILPipe() {
  VLOG(2) << "StopFOSSILPipe";
  auto crc = DosEnterCritSec();
  shutdown_pipes = true;
  crc = DosEnterCritSec();
}

int exec_cmdline(const std::string& cmd, int flags) {
  // Now just reuse SYNC_FOSSIl
  if (flags & EFLAG_SYNC_FOSSIL) {
    StartFOSSILPipe();
  }
 
  // HACK: make this legit
  std::string exe = cmd;
  auto ss = wwiv::strings::SplitString(cmd, " ");
  char * argv[30];

  for (int i=0; i < ss.size(); i++) {
    auto& s = ss[i];
    argv[i] = &s[0];
  }
  argv[ss.size()] = NULL;

  int mode = P_WAIT;//  |  P_WINDOWED; //  | P_SESSION; // | P_WINDOWED;
  VLOG(1) << "before spawnvp ";
  int code = spawnvp(mode, argv[0], argv);
  VLOG(1) << "after spawnvp; result: " << code;


  if (flags & EFLAG_SYNC_FOSSIL) {
    StopFOSSILPipe();
  }
  VLOG(1) << "Exiting exec_cmdline";
  return 1;
}
