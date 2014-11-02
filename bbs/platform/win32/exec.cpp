/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <ctime>
#include <string>
#include <sstream>

#include "bbs/bbs.h"
#include "bbs/wcomm.h"
#include "core/WFile.h"
#include "bbs/platform/platformfcns.h"
#include "core/strings.h"
#include "bbs/sysoplog.h"
#include "core/wtextfile.h"
#include "bbs/wsession.h"
#include "bbs/vars.h"

// from com.cpp.
// this is only used in the 9x support and will be remvoed shortly
void makeansi(int attr, char *pszOutBuffer, bool forceit);

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
using wwiv::strings::StrCat;

// Helper functions

static string GetSyncFosTempFilePath() {
  return StrCat(syscfgovr.tempdir, "WWIVSYNC.ENV");
}

static const string GetDosXtrnPath() {
  std::stringstream sstream;
  sstream << GetApplication()->GetHomeDir() << "DOSXTRN.EXE";
  return string(sstream.str());
}

static void CreateSyncFosCommandLine(string *out, const string& tempFilePath, int nSyncMode) {
  std::stringstream sstream;
  sstream << GetDosXtrnPath() << " " << tempFilePath << " " << "NT" << " ";
  sstream << GetApplication()->GetInstanceNumber() << " " << nSyncMode << " " << CONST_SBBSFOS_LOOPS_BEFORE_YIELD;
  out->assign(sstream.str());
}

// returns true if the file is deleted.
static bool DeleteSyncTempFile() {
  const string tempFileName = GetSyncFosTempFilePath();
  if (WFile::Exists(tempFileName)) {
    WFile::Remove(tempFileName);
    return true;
  }
  return false;
}

static bool CreateSyncTempFile(string *out, const string commandLine) {
  out->assign(GetSyncFosTempFilePath());
  DeleteSyncTempFile();

  WTextFile file(*out, "wt");
  if (!file.IsOpen()) {
    return false;
  }
  file.Write(commandLine);
  if (!file.Close()) {
    return false;
  }

  return true;
}

#define SYNC_LOOP_CLEANUP() {   \
    if ( hSyncHangupEvent != INVALID_HANDLE_VALUE ) CloseHandle( hSyncHangupEvent ); hSyncHangupEvent = INVALID_HANDLE_VALUE; \
    if ( hSyncReadSlot    != INVALID_HANDLE_VALUE ) CloseHandle( hSyncReadSlot );    hSyncReadSlot    = INVALID_HANDLE_VALUE; \
    if ( hSyncWriteSlot   != INVALID_HANDLE_VALUE ) CloseHandle( hSyncWriteSlot );   hSyncWriteSlot   = INVALID_HANDLE_VALUE; \
}

bool DoSyncFosLoopNT(HANDLE hProcess, HANDLE hSyncHangupEvent, HANDLE hSyncReadSlot, int nSyncMode) {
  fprintf(hLogFile, "Starting DoSyncFosLoopNT()\r\n");
  HANDLE hSyncWriteSlot   = INVALID_HANDLE_VALUE;     // Mailslot for writing

  char szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + CONST_SBBSFOS_BUFFER_PADDING ];
  ::ZeroMemory(szReadBuffer, CONST_SBBSFOS_BUFFER_SIZE + CONST_SBBSFOS_BUFFER_PADDING);
  szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 1 ] = '\xFE';

  int nCounter = 0;
  for (;;) {
    nCounter++;
    if (GetSession()->using_modem && (!GetSession()->remoteIO()->carrier())) {
      SetEvent(hSyncHangupEvent);
      fprintf(hLogFile, "Setting Hangup Event and Sleeping\r\n");
      ::Sleep(1000);
    }

    if (nCounter > CONST_NUM_LOOPS_BEFORE_EXIT_CHECK) {
      // Only check for exit when we've been idle
      DWORD dwExitCode = 0;
      if (GetExitCodeProcess(hProcess, &dwExitCode)) {
        if (dwExitCode != STILL_ACTIVE) {
          // process is done and so are we.
          fprintf(hLogFile, "Process finished, exiting\r\n");
          SYNC_LOOP_CLEANUP();
          return true;
        }
      }
    }

    if (GetSession()->remoteIO()->incoming()) {
      nCounter = 0;
      // SYNCFOS_DEBUG_PUTS( "Char available to send to the door" );
      int nNumReadFromComm = GetSession()->remoteIO()->read(szReadBuffer, CONST_SBBSFOS_BUFFER_SIZE);
      fprintf(hLogFile, "Read [%d] from comm\r\n", nNumReadFromComm);
#if 1
      int nLp = 0;
      for (nLp = 0; nLp < nNumReadFromComm; nLp++) {
        fprintf(hLogFile, "[%u]", static_cast<unsigned char>(szReadBuffer[ nLp ]), (unsigned char) szReadBuffer[ nLp ]);
      }
      fprintf(hLogFile, "   ");
      for (nLp = 0; nLp < nNumReadFromComm; nLp++) {
        fprintf(hLogFile, "[%c]", static_cast<unsigned char>(szReadBuffer[ nLp ]));
      }
      fprintf(hLogFile, "\r\n");
#endif // 0

      if (hSyncWriteSlot == INVALID_HANDLE_VALUE) {
        // Create Write handle.
        char szWriteSlotName[ MAX_PATH ];
        ::Sleep(500);
        _snprintf(szWriteSlotName, sizeof(szWriteSlotName), "\\\\.\\mailslot\\sbbsexec\\wr%d",
                  GetApplication()->GetInstanceNumber());
        fprintf(hLogFile, "Creating Mail Slot [%s]\r\n", szWriteSlotName);

        hSyncWriteSlot = CreateFile(szWriteSlotName,
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    (HANDLE) nullptr);
        if (hSyncWriteSlot == INVALID_HANDLE_VALUE) {
          sysoplogf("!!! Unable to create mail slot for writing for SyncFoss External program [%ld]", GetLastError());
          fprintf(hLogFile, "!!! Unable to create mail slot for writing for SyncFoss External program [%ld]", GetLastError());
          SYNC_LOOP_CLEANUP();
          return false;
        }
        fprintf(hLogFile, "Created MailSlot\r\n");

      }

      DWORD dwNumWrittenToSlot = 0;
      WriteFile(hSyncWriteSlot, szReadBuffer, nNumReadFromComm, &dwNumWrittenToSlot, nullptr);
      fprintf(hLogFile, "Wrote [%ld] to MailSlot\r\n", dwNumWrittenToSlot);
    }

    int nBufferPtr      = 0;    // BufPtr
    DWORD dwNextSize    = 0;    // InUsed
    DWORD dwNumMessages = 0;    // TmpLong
    DWORD dwReadTimeOut = 0;    // -------

    if (GetMailslotInfo(hSyncReadSlot, nullptr, &dwNextSize, &dwNumMessages, &dwReadTimeOut)) {
      if (dwNumMessages > 0) {
        fprintf(hLogFile, "[%ld/%ld] slot messages\r\n", dwNumMessages, std::min<DWORD>(dwNumMessages,
                CONST_SBBSFOS_BUFFER_SIZE - 1000));
        dwNumMessages = std::min<DWORD>(dwNumMessages, CONST_SBBSFOS_BUFFER_SIZE - 1000);
        dwNextSize = 0;

        for (unsigned int nSlotCounter = 0; nSlotCounter < dwNumMessages; nSlotCounter++) {
          if (ReadFile(hSyncReadSlot,
                       &szReadBuffer[ nBufferPtr ],
                       CONST_SBBSFOS_BUFFER_SIZE - nBufferPtr,
                       &dwNextSize,
                       nullptr)) {
            if (dwNextSize > 1) {
              fprintf(hLogFile, "[%ld] bytes read   \r\n", dwNextSize);
            }
            if (dwNextSize == MAILSLOT_NO_MESSAGE) {
              fprintf(hLogFile, "** MAILSLOT thinks it's empty!\r\n");
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
          szReadBuffer[ nBufferPtr ] = '\0';
          fprintf(hLogFile, "{%s}\r\n", szReadBuffer);
          GetSession()->bout << szReadBuffer;

          //ExpandWWIVHeartCodes( szReadBuffer );
          //int nNumWritten = GetSession()->remoteIO()->write( szReadBuffer, strlen( szReadBuffer )  );
          //fprintf( hLogFile, "Wrote [%d] bytes to comm.\r\n", nNumWritten );
        } else {
          int nNumWritten = GetSession()->remoteIO()->write(szReadBuffer, nBufferPtr);
          fprintf(hLogFile, "Wrote [%d] bytes to comm.\r\n", nNumWritten);
        }

      }
      if (szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 1 ] != '\xFE') {
        fprintf(hLogFile, "!!!!!! MEMORY CORRUPTION ON szReadBuffer !!!!!!!!\r\n\r\n\n\n");
      }
    }
    if (nCounter > 0) {
      ::Sleep(0);
    }
  }
}

bool ExpandWWIVHeartCodes(char *pszBuffer) {
  curatr = 0;
  char szTempBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 100 ];
  char *pIn = pszBuffer;
  char *pOut = szTempBuffer;
  int n = 0;
  while (*pIn && n++ < (CONST_SBBSFOS_BUFFER_SIZE)) {
    if (*pIn == '\x03') {
      pIn++;
      if (*pIn >= '0' && *pIn <= '9') {
        char szTempColor[ 81 ];
        int nColor = *pIn - '0';
        makeansi(GetSession()->GetCurrentUser()->GetColor(nColor), szTempColor, false);
        char *pColor = szTempColor;
        while (*pColor) {
          *pOut++ = *pColor++;
        }
        pIn++;
      } else {
        *pOut++ = '\x03';
      }
    }
    *pOut++ = *pIn++;
  }
  *pOut++ = '\0';
  strcpy(pszBuffer, szTempBuffer);
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
  if (GetSession()->using_modem) {
    if (flags & EFLAG_FOSSIL) {
      bShouldUseSync = true;
    } else if (flags & EFLAG_COMIO) {
      nSyncMode |= CONST_SBBSFOS_DOSIN_MODE;
      nSyncMode |= CONST_SBBSFOS_DOSOUT_MODE;
      bShouldUseSync = true;
    }
  }

  if (bShouldUseSync) {
    string syncFosTempFile;
    if (!CreateSyncTempFile(&syncFosTempFile, commandLine)) {
      return -1;
    }
    CreateSyncFosCommandLine(&workingCommandLine, syncFosTempFile, nSyncMode);
    bUsingSync = true;

    char szTempLogFileName[ MAX_PATH ];
    _snprintf(szTempLogFileName, sizeof(szTempLogFileName), "%swwivsync.log", GetApplication()->GetHomeDir().c_str());
    hLogFile = fopen(szTempLogFileName, "at");
    fprintf(hLogFile, charstr(78, '='));
    fprintf(hLogFile, "\r\n\r\n");
    fprintf(hLogFile, "Cmdline = [%s]\r\n\n", workingCommandLine.c_str());
  } else {
    workingCommandLine = commandLine;
  }

  DWORD dwCreationFlags = 0;
  char * pszTitle = new char[ 255 ];
  memset(pszTitle, 0, sizeof(pszTitle));
  if (flags & EFLAG_NETPROG) {
    strcpy(pszTitle, "NETWORK");
  } else {
    _snprintf(pszTitle, sizeof(pszTitle), "%s in door on node %d",
              GetSession()->GetCurrentUser()->GetName(), GetApplication()->GetInstanceNumber());
  }
  si.lpTitle = pszTitle;

  if (ok_modem_stuff && !bUsingSync && GetSession()->using_modem) {
    GetSession()->remoteIO()->close(true);
  }

  HANDLE hSyncHangupEvent = INVALID_HANDLE_VALUE;
  HANDLE hSyncReadSlot    = INVALID_HANDLE_VALUE;     // Mailslot for reading
    
  if (bUsingSync) {
    // Create Hangup Event.
    char szHangupEventName[ MAX_PATH + 1 ];
    _snprintf(szHangupEventName, sizeof(szHangupEventName), "sbbsexec_hungup%d", GetApplication()->GetInstanceNumber());
    hSyncHangupEvent = CreateEvent(nullptr, TRUE, FALSE, szHangupEventName);
    if (hSyncHangupEvent == INVALID_HANDLE_VALUE) {
      fprintf(hLogFile, "!!! Unable to create Hangup Event for SyncFoss External program [%ld]", GetLastError());
      sysoplogf("!!! Unable to create Hangup Event for SyncFoss External program [%ld]", GetLastError());
      return false;
    }

    // Create Read Mail Slot
    char szReadSlotName[ MAX_PATH + 1];
    _snprintf(szReadSlotName, sizeof(szReadSlotName), "\\\\.\\mailslot\\sbbsexec\\rd%d",
              GetApplication()->GetInstanceNumber());
    hSyncReadSlot = CreateMailslot(szReadSlotName, CONST_SBBSFOS_BUFFER_SIZE, 0, nullptr);
    if (hSyncReadSlot == INVALID_HANDLE_VALUE) {
      fprintf(hLogFile, "!!! Unable to create mail slot for reading for SyncFoss External program [%ld]", GetLastError());
      sysoplogf("!!! Unable to create mail slot for reading for SyncFoss External program [%ld]", GetLastError());
      CloseHandle(hSyncHangupEvent);
      hSyncHangupEvent = INVALID_HANDLE_VALUE;
      return false;
    }
  }

  char szCurDir[ MAX_PATH ];
  WWIV_GetDir(szCurDir, true);

  ::Sleep(250);

  char szTempWorkingCommandline[MAX_PATH + 1];
  strncpy(szTempWorkingCommandline, workingCommandLine.c_str(), MAX_PATH);
  BOOL bRetCP = CreateProcess(
                  nullptr,
                  szTempWorkingCommandline,
                  nullptr,
                  nullptr,
                  TRUE,
                  dwCreationFlags,
                  nullptr, // GetApplication()->xenviron not using nullptr causes things to not work.
                  szCurDir,
                  &si,
                  &pi);

  if (!bRetCP) {
    delete[] pszTitle;
    sysoplogf("!!! CreateProcess failed for command: [%s] with Error Code %ld", workingCommandLine.c_str(), GetLastError());
    if (bUsingSync) {
      fprintf(hLogFile, "!!! CreateProcess failed for command: [%s] with Error Code %ld", workingCommandLine.c_str(),
              GetLastError());
    }

    // If we return here, we may have to reopen the communications port.
    if (ok_modem_stuff && !bUsingSync && GetSession()->using_modem) {
      GetSession()->remoteIO()->open();
      GetSession()->remoteIO()->dtr(true);
    }
    return -1;
  }

  // If we are on Windows NT and GetSession()->IsExecUseWaitForInputIdle() is true use this.
  if (GetSession()->IsExecUseWaitForInputIdle()) {
    int dwWaitRet = ::WaitForInputIdle(pi.hProcess, GetSession()->GetExecWaitForInputTimeout());
    if (dwWaitRet != 0) {
      if (bUsingSync) {
        fprintf(hLogFile, "!!! WaitForInputIdle Failed with code %ld", dwWaitRet);
      }
      sysoplogf("!!! WaitForInputIdle Failed with code %ld", dwWaitRet);
    }
  } else {
    ::Sleep(GetSession()->GetExecWaitForInputTimeout());
    ::Sleep(0);
    ::Sleep(0);
    ::Sleep(0);
    ::Sleep(0);
  }
  ::Sleep(0);

  CloseHandle(pi.hThread);


  if (bUsingSync) {
    bool bSavedBinaryMode = GetSession()->remoteIO()->GetBinaryMode();
    GetSession()->remoteIO()->SetBinaryMode(true);
    bool bSyncLoopStatus = DoSyncFosLoopNT(pi.hProcess, hSyncHangupEvent, hSyncReadSlot, nSyncMode);
    fprintf(hLogFile,  "DoSyncFosLoopNT: Returning %s\r\n", (bSyncLoopStatus) ? "TRUE" : "FALSE");

    fprintf(hLogFile, charstr(78, '='));
    fprintf(hLogFile, "\r\n\r\n\r\n");
    fclose(hLogFile);

    if (!bSyncLoopStatus) {
      // TODO Log error
    } else {
      DWORD dwExitCode = 0;
      GetExitCodeProcess(pi.hProcess, &dwExitCode);
      if (dwExitCode == STILL_ACTIVE) {
        TerminateProcess(pi.hProcess, 0);
      }
    }
    GetSession()->remoteIO()->SetBinaryMode(bSavedBinaryMode);
  } else {
    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);
  }

  DWORD dwExitCode = 0;
  GetExitCodeProcess(pi.hProcess, &dwExitCode);

  // Close process and thread handles.
  CloseHandle(pi.hProcess);

  delete[] pszTitle;

  // reengage comm stuff
  if (ok_modem_stuff && !bUsingSync && GetSession()->using_modem) {
    GetSession()->remoteIO()->open();
    GetSession()->remoteIO()->dtr(true);
  }

  return static_cast< int >(dwExitCode);
}
