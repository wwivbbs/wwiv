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
  //log("Dos Create Pipe name : %s", pipe_name);
  HFILE hPipe;
  auto rc = DosCreateNPipe((const unsigned char*) pipe_name,
			   &hPipe,
			   NP_ACCESS_DUPLEX,
			   NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 0xFF,
			   PIPE_BUFFER_SIZE,
			   PIPE_BUFFER_SIZE,
			   0); // 0 == No Wait.
  //log("Dos Create Pipe [name: %s][handle: %d][res: %d]", pipe_name, hPipe, rc);
  return hPipe;
}

static bool connect_pipe(HFILE h) {
  ///log("Waiting for pipe to connect: [handle: %d]", h);
  int i = 0;
  auto rc = NO_ERROR;
  do {
    if (i++ > 1000) {
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
  //log("Pipe connected", rc);
  DosSleep(100);
  return true;
}

static int close_pipe(HFILE h) {
  return DosDisConnectNPipe(h);
}

void _System dos_pipe_loop(long unsigned int) {
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
    if (bout.remoteIO()->incoming()) {
      // This isn't likely super performant.
      const auto num_read = bout.remoteIO()->read(buffer, sizeof(buffer));
      unsigned long num_written;
      auto rc = DosWrite(h, buffer, num_read, &num_written);
      if (rc != NO_ERROR) {
	os_yield();
	//log("Error Writing to the pipe: %d", rc);
/*
      } else if (ch == 27) {
	// ESCAPE to exit
	char ch = 'D';
	rc = DosWrite(hc, &ch, 1, &num_written);
	// break; 
*/
      }
    }

    char ch;
    ULONG num_read;

    // Read Data
    if (auto rc = DosRead(h, &ch, 1, &num_read); rc == NO_ERROR && num_read > 0) {
      const auto num_written = bout.remoteIO()->write(&ch, 1);
      continue;
    } else if (rc != 232) {
      // Only yield if we didn't do anything.
      //log("DosRead error [%d], numread: [%d]", rc, num_read);
      os_yield();
      break;
    } else {
      os_yield();
    }

    // Read Control
    
    if (auto rc = DosRead(hc, &ch, 1, &num_read); rc == NO_ERROR && num_read > 0) {
      switch (ch) {
      case 'D': 
	// Froced disconnect.
	stop = true;
	break;
      case 'H': {
	// Heartbeat
      } break;
      }
    } else if (rc != 232) {
      stop = true;
      break;
    } else {
      os_yield();
    }

    auto crc = DosEnterCritSec();
    if (shutdown_pipes) {
      stop = true;
    }
    crc = DosEnterCritSec();

    if (stop) {
      break;
    }

  } while (!stop);

  close_pipe(h);
  close_pipe(hc);
}


static void StartFOSSILPipe() {
  TID tid = 0;
  DosCreateThread( &tid, dos_pipe_loop, 0, CREATE_READY | STACK_SPARSE, 8196 );
  DosSleep(100);
}

static void StopFOSSILPipe() {
  auto crc = DosEnterCritSec();
  shutdown_pipes = true;
  crc = DosEnterCritSec();

}

int exec_cmdline(const std::string& user_command_line, int flags) {
  // HACK: make this legit
  char s[1024];
  wwiv::strings::to_char_array(s, user_command_line);
  int len = wwiv::stl::size_int(user_command_line);
  char* argv[25];
  int arg = 0;
  for (int i=0; i < len; i++) {
    if (s[i] == ' ') s[i] = '\0';
    *argv[arg++] = s[i+1];
  }
  s[len] = 0;
  *argv[arg++] = s[len++];
  s[len] = 0;
  *argv[arg++] = s[len++];


  // Now just reuse SYNC_FOSSIl
  if (flags & EFLAG_SYNC_FOSSIL) {
    StartFOSSILPipe();
  }

  _execv(s, argv);

  if (flags & EFLAG_SYNC_FOSSIL) {
    StopFOSSILPipe();
  }
  return 1;
}
