/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <sstream>

#include "bbs/bbs.h"
#include "bbs/remote_io.h"
#include "bbs/platform/platformfcns.h"
#include "bbs/sysoplog.h"
#include "bbs/application.h"
#include "bbs/vars.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"

static FILE* hLogFile;

const int   CONST_SBBSFOS_FOSSIL_MODE           = 0;
const int   CONST_SBBSFOS_DOSIN_MODE            = 1;
const int   CONST_SBBSFOS_DOSOUT_MODE           = 2;
const int   CONST_SBBSFOS_LOOPS_BEFORE_YIELD    = 10;
const int   CONST_SBBSFOS_BUFFER_SIZE           = 5000;
const int   CONST_SBBSFOS_BUFFER_PADDING        = 1000; // padding in case of errors.

const DWORD SBBSEXEC_IOCTL_START            = 0x8001;
const DWORD SBBSEXEC_IOCTL_COMPLETE           = 0x8002;
const DWORD SBBSEXEC_IOCTL_READ             = 0x8003;
const DWORD SBBSEXEC_IOCTL_WRITE            = 0x8004;
const DWORD SBBSEXEC_IOCTL_DISCONNECT         = 0x8005;
const DWORD SBBSEXEC_IOCTL_STOP             = 0x8006;
const int CONST_NUM_LOOPS_BEFORE_EXIT_CHECK         = 500;
typedef HANDLE(WINAPI *OPENVXDHANDLEFUNC)(HANDLE);

using std::string;
using namespace wwiv::strings;

// Helper functions

static void LogToSync(const char* s) {
  if (a()->IsExecLogSyncFoss()) {
    fprintf(hLogFile, s);
  }
}

static void LogToSync(const std::string& s) {
  LogToSync(s.c_str());
}

static string GetSyncFosTempFilePath() {
  return StrCat(a()->temp_directory(), "WWIVSYNC.ENV");
}

static const string GetDosXtrnPath() {
  std::stringstream sstream;
  sstream << a()->GetHomeDir() << "DOSXTRN.EXE";
  return string(sstream.str());
}

static void CreateSyncFosCommandLine(string *out, const string& tempFilePath, int nSyncMode) {
  std::stringstream sstream;
  sstream << GetDosXtrnPath() << " " << tempFilePath << " " << "NT" << " ";
  sstream << a()->instance_number() << " " << nSyncMode << " " << CONST_SBBSFOS_LOOPS_BEFORE_YIELD;
  out->assign(sstream.str());
}

// returns true if the file is deleted.
static bool DeleteSyncTempFile() {
  const string tempFileName = GetSyncFosTempFilePath();
  if (File::Exists(tempFileName)) {
    File::Remove(tempFileName);
    return true;
  }
  return false;
}

static bool CreateSyncTempFile(string *out, const string commandLine) {
  out->assign(GetSyncFosTempFilePath());
  DeleteSyncTempFile();

  TextFile file(*out, "wt");
  if (!file.IsOpen()) {
    return false;
  }
  file.Write(commandLine);
  if (!file.Close()) {
    return false;
  }

  return true;
}

bool DoSyncFosLoopNT(HANDLE hProcess, HANDLE hSyncHangupEvent, HANDLE hSyncReadSlot, int nSyncMode) {
  LogToSync("Starting DoSyncFosLoopNT()\r\n");
  HANDLE hSyncWriteSlot = INVALID_HANDLE_VALUE;     // Mailslot for writing

  // Cleanup on all exit paths
  wwiv::core::ScopeExit at_exit([&] {
    if (hSyncHangupEvent != INVALID_HANDLE_VALUE) { 
      CloseHandle(hSyncHangupEvent);
      hSyncHangupEvent = INVALID_HANDLE_VALUE;
    }
    if (hSyncReadSlot != INVALID_HANDLE_VALUE) { 
      CloseHandle(hSyncReadSlot); 
      hSyncReadSlot = INVALID_HANDLE_VALUE;
    }
    if (hSyncWriteSlot != INVALID_HANDLE_VALUE) { 
      CloseHandle(hSyncWriteSlot); 
      hSyncWriteSlot = INVALID_HANDLE_VALUE;
    }
  });

  char szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + CONST_SBBSFOS_BUFFER_PADDING ];
  ::ZeroMemory(szReadBuffer, CONST_SBBSFOS_BUFFER_SIZE + CONST_SBBSFOS_BUFFER_PADDING);
  szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 1 ] = '\xFE';

  int nCounter = 0;
  for (;;) {
    nCounter++;
    if (a()->using_modem && (!a()->remoteIO()->connected())) {
      SetEvent(hSyncHangupEvent);
      LogToSync("Setting Hangup Event and Sleeping\r\n");
      ::Sleep(1000);
    }

    if (nCounter > CONST_NUM_LOOPS_BEFORE_EXIT_CHECK) {
      // Only check for exit when we've been idle
      DWORD dwExitCode = 0;
      if (GetExitCodeProcess(hProcess, &dwExitCode)) {
        if (dwExitCode != STILL_ACTIVE) {
          // process is done and so are we.
          LogToSync("Process finished, exiting\r\n");
          return true;
        }
      }
    }

    if (a()->remoteIO()->incoming()) {
      nCounter = 0;
      // SYNCFOS_DEBUG_PUTS( "Char available to send to the door" );
      int nNumReadFromComm = a()->remoteIO()->read(szReadBuffer, CONST_SBBSFOS_BUFFER_SIZE);
      LogToSync(StrCat("Read [", nNumReadFromComm, "] from comm\r\n"));

      int nLp = 0;
      if (a()->IsExecLogSyncFoss()) {
        for (nLp = 0; nLp < nNumReadFromComm; nLp++) {
          fprintf(hLogFile, "[%u]", static_cast<unsigned char>(szReadBuffer[nLp]));
        }
        fprintf(hLogFile, "   ");
        for (nLp = 0; nLp < nNumReadFromComm; nLp++) {
          fprintf(hLogFile, "[%c]", static_cast<unsigned char>(szReadBuffer[ nLp ]));
        }
        fprintf(hLogFile, "\r\n");
      }

      if (hSyncWriteSlot == INVALID_HANDLE_VALUE) {
        // Create Write handle.
        ::Sleep(500);
        string write_slot_name = StringPrintf("\\\\.\\mailslot\\sbbsexec\\wr%d", a()->instance_number());
        LogToSync(StrCat("Creating Mail Slot: '", write_slot_name, "'\r\n"));

        hSyncWriteSlot = CreateFile(write_slot_name.c_str(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    (HANDLE) nullptr);
        if (hSyncWriteSlot == INVALID_HANDLE_VALUE) {
          sysoplog() << "!!! Unable to create mail slot for writing for SyncFoss External program: " << GetLastError();
          LogToSync(StrCat("!!! Unable to create mail slot for writing for SyncFoss External program: ", GetLastError()));
          return false;
        }
        LogToSync("Created MailSlot\r\n");
      }

      DWORD dwNumWrittenToSlot = 0;
      WriteFile(hSyncWriteSlot, szReadBuffer, nNumReadFromComm, &dwNumWrittenToSlot, nullptr);
      LogToSync(StrCat("Wrote [", dwNumWrittenToSlot, "] to MailSlot\r\n"));
    }

    int nBufferPtr      = 0;    // BufPtr
    DWORD dwNextSize    = 0;    // InUsed
    DWORD dwNumMessages = 0;    // TmpLong
    DWORD dwReadTimeOut = 0;    // -------

    if (GetMailslotInfo(hSyncReadSlot, nullptr, &dwNextSize, &dwNumMessages, &dwReadTimeOut)) {
      if (dwNumMessages > 0) {
        LogToSync(StrCat("[", dwNumMessages, "/", std::min<DWORD>(dwNumMessages, CONST_SBBSFOS_BUFFER_SIZE - 1000), "] slot messages\r\n"));
        dwNumMessages = std::min<DWORD>(dwNumMessages, CONST_SBBSFOS_BUFFER_SIZE - 1000);
        dwNextSize = 0;

        for (DWORD nSlotCounter = 0; nSlotCounter < dwNumMessages; nSlotCounter++) {
          if (ReadFile(hSyncReadSlot,
                       &szReadBuffer[ nBufferPtr ],
                       CONST_SBBSFOS_BUFFER_SIZE - nBufferPtr,
                       &dwNextSize,
                       nullptr)) {
            if (dwNextSize > 1) {
              LogToSync(StrCat("[", dwNextSize, "] bytes read   \r\n"));
            }
            if (dwNextSize == MAILSLOT_NO_MESSAGE) {
              LogToSync("** MAILSLOT thinks it's empty!\r\n");
            } else {
              nBufferPtr += dwNextSize;   // was just ++.. oops
            }

            if (nBufferPtr >= CONST_SBBSFOS_BUFFER_SIZE - 10) {
              break;
            }
          }
        }
      } else {
        ::Sleep(0);
      }

      if (nBufferPtr > 0) {
        nCounter = 0;
        if (nSyncMode & CONST_SBBSFOS_DOSOUT_MODE) {
          // For some reason this doesn't write twice locally, so it works pretty well.
          szReadBuffer[nBufferPtr] = '\0';
          // LogToSync(StrCat("{", szReadBuffer, "}\r\n"));
          bout << szReadBuffer;

          // ExpandWWIVHeartCodes( szReadBuffer );
          // int nNumWritten = a()->remoteIO()->write( szReadBuffer, strlen( szReadBuffer )  );
        } else {
          int nNumWritten = a()->remoteIO()->write(szReadBuffer, nBufferPtr);
          LogToSync(StrCat("Wrote [", nNumWritten, "] bytes to comm.\r\n"));
        }

      }
      if (a()->IsExecLogSyncFoss()) {
        if (szReadBuffer[CONST_SBBSFOS_BUFFER_SIZE + 1] != '\xFE') {
          LogToSync("!!!!!! MEMORY CORRUPTION ON szReadBuffer !!!!!!!!\r\n\r\n\n\n");
        }
      }
    }
    if (nCounter > 0) {
      ::Sleep(0);
    }
  }
}

static bool SetupSyncFoss(DWORD& dwCreationFlags, HANDLE& hSyncHangupEvent, HANDLE& hSyncReadSlot) {
  hSyncHangupEvent = INVALID_HANDLE_VALUE;
  hSyncReadSlot = INVALID_HANDLE_VALUE;     // Mailslot for reading

  // Create each syncfoss window in it's own WOW VDM.
  dwCreationFlags |= CREATE_SEPARATE_WOW_VDM;

  // Create Hangup Event.
  const string event_name = StringPrintf("sbbsexec_hungup%d", a()->instance_number());
  hSyncHangupEvent = CreateEvent(nullptr, TRUE, FALSE, event_name.c_str());
  if (hSyncHangupEvent == INVALID_HANDLE_VALUE) {
    LogToSync(StrCat("!!! Unable to create Hangup Event for SyncFoss External program: ", GetLastError()));
    sysoplog() << "!!! Unable to create Hangup Event for SyncFoss External program: " << GetLastError();
    return false;
  }

  // Create Read Mail Slot
  const string readslot_name = StringPrintf("\\\\.\\mailslot\\sbbsexec\\rd%d", a()->instance_number());
  hSyncReadSlot = CreateMailslot(readslot_name.c_str(), CONST_SBBSFOS_BUFFER_SIZE, 0, nullptr);
  if (hSyncReadSlot == INVALID_HANDLE_VALUE) {
    LogToSync(StrCat("!!! Unable to create mail slot for reading for SyncFoss External program: ", GetLastError()));
    sysoplog() << "!!! Unable to create mail slot for reading for SyncFoss External program: " << GetLastError();
    CloseHandle(hSyncHangupEvent);
    hSyncHangupEvent = INVALID_HANDLE_VALUE;
    return false;
  }
  return true;
}

//  Main code that launches external programs and handle sbbsexec support

int ExecExternalProgram(const string commandLine, int flags) {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));
  string workingCommandLine;

  bool bShouldUseSync = false;
  bool bUsingSync = false;
  int nSyncMode = 0;
  if (a()->using_modem) {
    if (flags & EFLAG_FOSSIL) {
      bShouldUseSync = true;
    } else if (flags & EFLAG_COMIO) {
      nSyncMode |= CONST_SBBSFOS_DOSIN_MODE;
      nSyncMode |= CONST_SBBSFOS_DOSOUT_MODE;
      bShouldUseSync = true;
    }
  }
  if (flags & EFLAG_STDIO) {
    if (bShouldUseSync) {
      // Not allowed.
      sysoplog() << "Tried to execute command with sync and stdio: " << commandLine;
      LOG(ERROR) << "Tried to execute command with sync and stdio: " << commandLine;
      return false;
    }

    // Set the socket to be std{in,out}
    auto sock = a()->remoteIO()->GetDoorHandle();
    si.hStdInput = (HANDLE) sock;
    si.hStdOutput = (HANDLE)sock;
    si.hStdError = (HANDLE)sock;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }

  if (bShouldUseSync) {
    string syncFosTempFile;
    if (!CreateSyncTempFile(&syncFosTempFile, commandLine)) {
      return -1;
    }
    CreateSyncFosCommandLine(&workingCommandLine, syncFosTempFile, nSyncMode);
    bUsingSync = true;

    const string logfile_name = StrCat(a()->GetHomeDir(), "wwivsync.log");
    hLogFile = fopen(logfile_name.c_str(), "at");
    LogToSync(charstr(78, '='));
    LogToSync("\r\n\r\n");
  } else {
    workingCommandLine = commandLine;
  }

  DWORD dwCreationFlags = 0;
  auto title = std::make_unique<char[]>(500);
  if (flags & EFLAG_NETPROG) {
    strcpy(title.get(), "NETWORK");
  } else {
    sprintf(title.get(), "%s in door on node %d",
        a()->user()->GetName(), a()->instance_number());
  }
  si.lpTitle = title.get();

  if (ok_modem_stuff && !bUsingSync && a()->using_modem) {
    a()->remoteIO()->close(true);
  }

  HANDLE hSyncHangupEvent = INVALID_HANDLE_VALUE;
  HANDLE hSyncReadSlot = INVALID_HANDLE_VALUE;     // Mailslot for reading
    
  if (bUsingSync) {
    SetupSyncFoss(dwCreationFlags, hSyncHangupEvent, hSyncReadSlot);
    ::Sleep(250);
  }

  const string current_directory = File::current_directory();

  // Need a non-const string for the commandline
  char szTempWorkingCommandline[MAX_PATH+1];
  strncpy(szTempWorkingCommandline, workingCommandLine.c_str(), MAX_PATH);
  BOOL bRetCP = CreateProcess(
                  nullptr,
                  szTempWorkingCommandline,
                  nullptr,
                  nullptr,
                  TRUE,
                  dwCreationFlags,
                  nullptr, // a()->xenviron not using nullptr causes things to not work.
                  current_directory.c_str(),
                  &si,
                  &pi);

  if (!bRetCP) {
    sysoplog() << "!!! CreateProcess failed for command: [" << workingCommandLine << "] with Error Code: " << GetLastError();

    // If we return here, we may have to reopen the communications port.
    if (ok_modem_stuff && !bUsingSync && a()->using_modem) {
      a()->remoteIO()->open();
    }
    return -1;
  }

  if (bUsingSync) {
    // Kinda hacky but WaitForInputIdle doesn't work on console application.
    ::Sleep(a()->GetExecChildProcessWaitTime());
    const int sleep_zero_times = 5;
    for (int iter = 0; iter < sleep_zero_times; iter++) {
      ::Sleep(0);
    }
  }
  CloseHandle(pi.hThread);

  if (bUsingSync) {
    bool saved_binary_mode = a()->remoteIO()->binary_mode();
    a()->remoteIO()->set_binary_mode(true);
    bool sync_loop_status = DoSyncFosLoopNT(pi.hProcess, hSyncHangupEvent, hSyncReadSlot, nSyncMode);
    LogToSync(StrCat("DoSyncFosLoopNT: Returning ", sync_loop_status, "\r\n", charstr(78, '='), "\r\n\r\n\r\n"));
    fclose(hLogFile);

    if (sync_loop_status) {
      DWORD dwExitCode = 0;
      GetExitCodeProcess(pi.hProcess, &dwExitCode);
      if (dwExitCode == STILL_ACTIVE) {
        LOG(INFO) << "Terminating sync process.";
        TerminateProcess(pi.hProcess, 0);
      }
    }
    a()->remoteIO()->set_binary_mode(saved_binary_mode);
  } else {
    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);
  }

  DWORD dwExitCode = 0;
  GetExitCodeProcess(pi.hProcess, &dwExitCode);

  // Close process and thread handles.
  CloseHandle(pi.hProcess);

  // reengage comm stuff
  if (ok_modem_stuff && !bUsingSync && a()->using_modem) {
    a()->remoteIO()->open();
  }

  return static_cast<int>(dwExitCode);
}
