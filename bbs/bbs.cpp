/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2010, WWIV Software Services             */
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
#ifdef _WIN32
#include <direct.h>
#endif  // WIN32

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <cstdarg>

#define _DEFINE_GLOBALS_
// vars.h requires  _DEFINE_GLOBALS_
#include "bbs/vars.h"
// This has to be included here to define "xtrasubsrec *xsubs;"
#include "bbs/subxtr.h"
#undef _DEFINE_GLOBALS_

#include "bbs/bbs.h"
#include "bbs/datetime.h"
#include "bbs/external_edit.h"
#include "bbs/fcns.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/keycodes.h"
#include "bbs/menu.h"
#include "bbs/printfile.h"
#include "bbs/voteedit.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wfc.h"
#include "bbs/wsession.h"
#include "bbs/wstatus.h"
#include "bbs/platform/platformfcns.h"
#include "bbs/platform/wlocal_io.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"

#if defined( _WIN32 )
#include <direct.h>
#include "bbs/platform/win32/InternalTelnetServer.h"
#include "bbs/platform/win32/Wiot.h"
#else
#include <unistd.h>
#endif // _WIN32

static bool bUsingPppProject = true;
extern time_t last_time_c;
static WApplication *app;
static WSession* sess;

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::clog;
using std::cout;
using std::endl;
using std::exception;
using std::stoi;
using std::stol;
using std::string;
using std::min;
using std::max;
using std::string;
using std::unique_ptr;
using wwiv::bbs::InputMode;
using namespace wwiv::os;
using namespace wwiv::strings;

WApplication* application() { return app; }
WSession* session() { return sess; }

#if !defined ( __unix__ )
void WApplication::GetCaller() {
  session()->SetMessageAreaCacheNumber(0);
  session()->SetFileAreaCacheNumber(0);
  SetShutDownStatus(WApplication::shutdownNone);
  wfc_init();
  session()->remoteIO()->ClearRemoteInformation();
  frequent_init();
  if (session()->wfc_status == 0) {
    session()->localIO()->LocalCls();
  }
  session()->usernum = 0;
  SetWfcStatus(0);
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
  session()->ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  session()->usernum = 1;
  session()->ResetEffectiveSl();
  fwaiting = session()->user()->GetNumMailWaiting();
  if (session()->user()->IsUserDeleted()) {
    session()->user()->SetScreenChars(80);
    session()->user()->SetScreenLines(25);
  }
  session()->screenlinest = defscreenbottom + 1;

  int lokb = doWFCEvents();

  if (lokb) {
    modem_speed = 14400;
  }

  session()->using_modem = incom;
  if (lokb == 2) {
    session()->using_modem = -1;
  }

  okskey = true;
  session()->localIO()->LocalCls();
  session()->localIO()->LocalPrintf("Logging on at %s ...\r\n",
                                       session()->GetCurrentSpeed().c_str());
  SetWfcStatus(0);
}

#else  // _unix__

void wfc_screen() {}
void wfc_cls() {}

#endif  // __unix__

int WApplication::doWFCEvents() {
  char ch;
  int lokb;
  static int mult_time;

  unique_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
  do {
    write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    set_net_num(0);
    bool any = false;
    SetWfcStatus(1);
    if (!IsEquals(date(), pStatus->GetLastDate())) {
      if ((session()->GetBeginDayNodeNumber() == 0) || (instance_number == session()->GetBeginDayNodeNumber())) {
        cleanup_events();
        beginday(true);
        wfc_cls();
      }
    }

    if (!do_event) {
      check_event();
    }

    while (do_event) {
      run_event(do_event - 1);
      check_event();
      any = true;
    }

    lokb = 0;
    session()->SetCurrentSpeed("KB");
    time_t lCurrentTime = time(nullptr);
    if (!any && (((rand() % 8000) == 0) || (lCurrentTime - last_time_c > 1200)) &&
        (net_sysnum) && (ok_modem_stuff || bUsingPppProject) &&
        (this->flags & OP_FLAGS_NET_CALLOUT)) {
      lCurrentTime = last_time_c;
      attempt_callout();
      any = true;
    }
    wfc_screen();
    okskey = false;
    if (session()->localIO()->LocalKeyPressed()) {
      SetWfcStatus(0);
      session()->ReadCurrentUser(1);
      read_qscn(1, qsc, false);
      fwaiting = session()->user()->GetNumMailWaiting();
      SetWfcStatus(1);
      ch = wwiv::UpperCase<char>(session()->localIO()->LocalGetChar());
      if (ch == 0) {
        ch = session()->localIO()->LocalGetChar();
        session()->localIO()->skey(ch);
        ch = 0;
      }
    } else {
      ch = 0;
      giveup_timeslice();
    }
    if (ch) {
      SetWfcStatus(2);
      any = true;
      okskey = true;
      resetnsp();
      session()->localIO()->SetCursor(WLocalIO::cursorNormal);
      switch (ch) {
      // Local Logon
      case SPACE:
        lokb = this->LocalLogon();
        break;
      // Reset User Records
      case '=':
        if (AllowLocalSysop()) {
          reset_files();
        }
        break;
      // Show WFC Menu
      case '?':
        if (AllowLocalSysop()) {
          string helpFileName = SWFC_NOEXT;
          char chHelp = ESC;
          do {
            session()->localIO()->LocalCls();
            bout.nl();
            printfile(helpFileName);
            chHelp = getkey();
            helpFileName = (helpFileName == SWFC_NOEXT) ? SONLINE_NOEXT : SWFC_NOEXT;
          } while (chHelp != SPACE && chHelp != ESC);
        }
        break;
      // Force Network Callout
      case '/':
        if (net_sysnum && AllowLocalSysop() && (ok_modem_stuff || bUsingPppProject)) {
          force_callout(0);
        }
        break;
      // War Dial Connect
      case '.':
        if (net_sysnum && AllowLocalSysop() && (ok_modem_stuff || bUsingPppProject)) {
          force_callout(1);
        }
        break;

      // Fast Net Callout from WFC
      case '*': {
        session()->localIO()->LocalCls(); // added   - 02/23/14 - dsn
        do_callout(32767); // changed - 02/23/14 - dsn - changed 1160 to 32767
      } break;
      // Run MenuEditor
      case '!':
        if (AllowLocalSysop()) {
          EditMenus();
        }
        break;
      // Print NetLogs
      case ',':
        if (net_sysnum > 0 || (session()->GetMaxNetworkNumber() > 1 && AllowLocalSysop())) {
          session()->localIO()->LocalGotoXY(2, 23);
          bout << "|#7(|#2Q|#7=|#2Quit|#7) Display Which NETDAT Log File (|#10|#7-|#12|#7): ";
          ch = onek("Q012");
          switch (ch) {
          case '0':
          case '1':
          case '2': {
            print_local_file(StringPrintf("netdat%c.log", ch));
          }
          break;
          }
        }
        break;
      // Net List
      case '`':
        if (net_sysnum && AllowLocalSysop()) {
          print_net_listing(true);
        }
        break;
      // [TAB] Instance Editor
      case TAB:
        if (AllowLocalSysop()) {
          wfc_cls();
          instance_edit();
        }
        break;
      // [ESC] Quit the BBS
      case ESC:
        session()->localIO()->LocalGotoXY(2, 23);
        bout << "|#7Exit the BBS? ";
        if (yesno()) {
          QuitBBS();
        }
        session()->localIO()->LocalCls();
        break;
      // BoardEdit
      case 'B':
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE);
          boardedit();
          cleanup_net();
        }
        break;
      // ChainEdit
      case 'C':
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE);
          chainedit();
        }
        break;
      // DirEdit
      case 'D':
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_DIREDIT, 0, INST_FLAGS_NONE);
          dlboardedit();
        }
        break;
      // Send Email
      case 'E':
        if (AllowLocalSysop()) {
          wfc_cls();
          session()->usernum = 1;
          bout << "|#1Send Email:";
          send_email();
          session()->WriteCurrentUser(1);
          cleanup_net();
        }
        break;
      // GfileEdit
      case 'G':
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
          gfileedit();
        }
        break;
      // EventEdit
      case 'H':
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_EVENTEDIT, 0, INST_FLAGS_NONE);
          eventedit();
        }
        break;
      // InitVotes
      case 'I':
        if (AllowLocalSysop()) {
          wfc_cls();
          write_inst(INST_LOC_VOTEEDIT, 0, INST_FLAGS_NONE);
          ivotes();
        }
        break;
      // ConfEdit
      case 'J':
        if (AllowLocalSysop()) {
          wfc_cls();
          edit_confs();
        }
        break;
      // SendMailFile
      case 'K':
        if (AllowLocalSysop()) {
          wfc_cls();
          session()->usernum = 1;
          bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
          string buffer;
          input(&buffer, 50);
          LoadFileIntoWorkspace(buffer.c_str(), false);
          send_email();
          session()->WriteCurrentUser(1);
          cleanup_net();
        }
        break;
      // Print Log Daily logs
      case 'L':
        if (AllowLocalSysop()) {
          wfc_cls();
          unique_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
          const string sysop_log_file = GetSysopLogFileName(date());
          print_local_file(sysop_log_file);
        }
        break;
      // Read User Mail
      case 'M':
        if (AllowLocalSysop()) {
          wfc_cls();
          session()->usernum = 1;
          readmail(0);
          session()->WriteCurrentUser(1);
          cleanup_net();
        }
        break;
      // Print Net Log
      case 'N':
        if (AllowLocalSysop()) {
          wfc_cls();
          print_local_file("net.log");
        }
        break;
      // EditTextFile
      case 'O':
        if (AllowLocalSysop()) {
          wfc_cls();
          write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
          bout << "\r\n|#1Edit any Text File: \r\n\n|#2Filename: ";
          const string current_dir_slash = File::current_directory() + File::pathSeparatorString;
          string newFileName = Input1(current_dir_slash, 50, true, InputMode::UPPER);
          if (!newFileName.empty()) {
            external_text_edit(newFileName, "", 500, ".", MSGED_FLAG_NO_TAGLINE);
          }
        }
        break;
      // Print Network Pending list
      case 'P':
        if (AllowLocalSysop()) {
          wfc_cls();
          print_pending_list();
        }
        break;
      // Quit BBS
      case 'Q':
        session()->localIO()->LocalGotoXY(2, 23);
        QuitBBS();
        break;
      // Read All Mail
      case 'R':
        wfc_cls();
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
          mailr();
        }
        break;
      // Print Current Status
      case 'S':
        if (AllowLocalSysop()) {
          prstatus(true);
          getkey();
        }
        break;
      // UserEdit
      case 'U':
        if (AllowLocalSysop()) {
          write_inst(INST_LOC_UEDIT, 0, INST_FLAGS_NONE);
          uedit(1, UEDIT_NONE);
        }
        break;
      // Send Internet Mail
      case 'V':
        if (AllowLocalSysop()) {
          wfc_cls();
          session()->usernum = 1;
          session()->SetUserOnline(true);
          get_user_ppp_addr();
          send_inet_email();
          session()->SetUserOnline(false);
          session()->WriteCurrentUser(1);
          cleanup_net();
        }
        break;
      // Edit Gfile
      case 'W':
        if (AllowLocalSysop()) {
          wfc_cls();
          write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
          bout << "|#1Edit " << syscfg.gfilesdir << "<filename>: \r\n";
          text_edit();
        }
        break;
      // Print Environment
      case 'X':
        break;
      // Print Yesterday's Log
      case 'Y':
        if (AllowLocalSysop()) {
          wfc_cls();
          unique_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
          const string sysop_log_file = GetSysopLogFileName(date());
          print_local_file(pStatus->GetLogFileName());
        }
        break;
      // Print Activity (Z) Log
      case 'Z':
        if (AllowLocalSysop()) {
          zlog();
          bout.nl();
          getkey();
        }
        break;
      }
      wfc_cls();  // moved from after getch
      if (!incom && !lokb) {
        frequent_init();
        session()->ReadCurrentUser(1);
        read_qscn(1, qsc, false);
        fwaiting = session()->user()->GetNumMailWaiting();
        session()->ResetEffectiveSl();
        session()->usernum = 1;
      }
      catsl();
      write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    }

    if (!any) {
      if (session()->GetMessageAreaCacheNumber() < session()->num_subs) {
        if (!session()->m_SubDateCache[session()->GetMessageAreaCacheNumber()]) {
          any = true;
          iscan1(session()->GetMessageAreaCacheNumber(), true);
        }
        session()->SetMessageAreaCacheNumber(session()->GetMessageAreaCacheNumber() + 1);
      } else {
        if (session()->GetFileAreaCacheNumber() < session()->num_dirs) {
          if (!session()->m_DirectoryDateCache[session()->GetFileAreaCacheNumber()]) {
            any = true;
            dliscan_hash(session()->GetFileAreaCacheNumber());
          }
          session()->SetFileAreaCacheNumber(session()->GetFileAreaCacheNumber() + 1);
        } else {
          if (this->IsCleanNetNeeded() || labs(timer1() - mult_time) > 1000L) {
            cleanup_net();
            mult_time = timer1();
            giveup_timeslice();
          } else {
            giveup_timeslice();
          }
        }
      }
    }
  } while (!incom && !lokb);
  return lokb;
}


int WApplication::LocalLogon() {
  session()->localIO()->LocalGotoXY(2, 23);
  bout << "|#9Log on to the BBS?";
  double d = timer();
  int lokb = 0;
  while (!session()->localIO()->LocalKeyPressed() && (fabs(timer() - d) < SECONDS_PER_MINUTE_FLOAT))
    ;

  if (session()->localIO()->LocalKeyPressed()) {
    char ch = wwiv::UpperCase<char>(session()->localIO()->LocalGetChar());
    if (ch == 'Y') {
      session()->localIO()->LocalFastPuts(YesNoString(true));
      bout << wwiv::endl;
      lokb = 1;
    } else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The ch == 224 is a Win32'ism
      session()->localIO()->LocalGetChar();
    } else {
      bool fast = false;
      if (!AllowLocalSysop()) {
        return lokb;
      }

      if (ch == 'F') {   // 'F' for Fast
        m_unx = 1;
        fast = true;
      } else {
        switch (ch) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          fast = true;
          m_unx = ch - '0';
          break;
        }
      }
      if (!fast || m_unx > GetStatusManager()->GetUserCount()) {
        return lokb;
      }

      WUser tu;
      users()->ReadUserNoCache(&tu, m_unx);
      if (tu.GetSl() != 255 || tu.IsUserDeleted()) {
        return lokb;
      }

      session()->usernum = m_unx;
      int nSavedWFCStatus = GetWfcStatus();
      SetWfcStatus(0);
      session()->ReadCurrentUser();
      read_qscn(session()->usernum, qsc, false);
      SetWfcStatus(nSavedWFCStatus);
      bputch(ch);
      session()->localIO()->LocalPuts("\r\n\r\n\r\n\r\n\r\n\r\n");
      lokb = 2;
      session()->ResetEffectiveSl();
      changedsl();
      if (!set_language(session()->user()->GetLanguage())) {
        session()->user()->SetLanguage(0);
        set_language(0);
      }
      return lokb;
    }
    if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The 224 is a Win32'ism
      session()->localIO()->LocalGetChar();
    }
  }
  if (lokb == 0) {
    session()->localIO()->LocalCls();
  }
  return lokb;
}

void WApplication::GotCaller(unsigned int ms, unsigned long cs) {
  frequent_init();
  if (session()->wfc_status == 0) {
    session()->localIO()->LocalCls();
  }
  com_speed   = cs;
  modem_speed = ms;
  session()->ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  session()->ResetEffectiveSl();
  session()->usernum = 1;
  if (session()->user()->IsUserDeleted()) {
    session()->user()->SetScreenChars(80);
    session()->user()->SetScreenLines(25);
  }
  session()->screenlinest = 25;
  session()->localIO()->LocalCls();
  session()->localIO()->LocalPrintf("Logging on at %s...\r\n", session()->GetCurrentSpeed().c_str());
  if (ms) {
    if (ok_modem_stuff && nullptr != sess->remoteIO()) {
      sess->remoteIO()->setup('N', 8, 1, cs);
    }
    incom   = true;
    outcom  = true;
    session()->using_modem = 1;
  } else {
    session()->using_modem = 0;
    incom   = false;
    outcom  = false;
  }
}

int WApplication::BBSMainLoop(int argc, char *argv[]) {
// TODO - move this to WIOTelnet
#if defined ( _WIN32 )
  WIOTelnet::InitializeWinsock();
  // If there is only 1 argument "-TELSRV" then use internal telnet daemon
  if (argc == 2 && IsEqualsIgnoreCase(argv[1], "-TELSRV")) {
    WInternalTelnetServer server(this);
    server.RunTelnetServer();
    ExitBBSImpl(0);
    return 0;
  }
#endif // _WIN32

  // We are not running in the telnet server, so proceed as planned.
  int nReturnCode = Run(argc, argv);
  ExitBBSImpl(nReturnCode);
  return nReturnCode;
}

int WApplication::Run(int argc, char *argv[]) {
  int num_min                 = 0;
  unsigned int ui             = 0;
  unsigned long us            = 0;
  unsigned short this_usernum = 0;
  bool ooneuser               = false;
  bool event_only             = false;
  bool bTelnetInstance        = false;
  unsigned int hSockOrComm    = 0;

  char* ss = getenv("BBS");
  if (ss) {
    if (strncmp(ss, "WWIV", 4) == 0) {
      clog << "You are already in the BBS, type 'EXIT' instead.\n\n";
      exit(255);
    }
  }
  curatr = 0x07;
  ss = getenv("WWIV_DIR");
  if (ss) {
    chdir(ss);
  }

  // Set the instance, this may be changed by a command line argument
  instance_number = 1;
  no_hangup = false;
  ok_modem_stuff = true;

#if defined( __unix__ )
  // HACK to make WWIV5/X just work w/o any command line
  m_bUserAlreadyOn = true;
  ui = us = 9600;
  ooneuser = true;
#endif

  string systemPassword;

  for (int i = 1; i < argc; i++) {
    string argumentRaw = argv[i];
    if (argumentRaw.length() > 1 && (argumentRaw[0] == '-' || argumentRaw[0] == '/')) {
      string argument = argumentRaw.substr(2);
      char ch = wwiv::UpperCase<char>(argumentRaw[1]);
      switch (ch) {
      case 'B': {
        ui = static_cast<unsigned int>(atol(argument.c_str()));
        const string current_speed_string = StringPrintf("%u",  ui);
        session()->SetCurrentSpeed(current_speed_string.c_str());
        if (!us) {
          us = ui;
        }
        m_bUserAlreadyOn = true;
      }
      break;
      case 'C':
        break;
      case 'D':
        File::SetDebugLevel(stoi(argument));
        break;
      case 'E':
        event_only = true;
        break;
      case 'S':
        us = static_cast<unsigned int>(stol(argument));
        if ((us % 300) && us != 115200) {
          us = ui;
        }
        break;
      case 'Q':
        m_nOkLevel = stoi(argument);
        break;
      case 'A':
        m_nErrorLevel = stoi(argument);
        break;
      case 'O':
        ooneuser = true;
        break;
      case 'H':
        hSockOrComm = stoi(argument);
        break;
      case 'P':
        systemPassword = argument;
        StringUpperCase(&systemPassword);
        break;
      case 'I':
      case 'N': {
        instance_number = stoi(argument);
        if (instance_number <= 0 || instance_number > 999) {
          clog << "Your Instance can only be 1..999, you tried instance #" << instance_number << endl;
          exit(m_nErrorLevel);
        }
      }
      break;
      case 'M':
#if !defined ( __unix__ )
        ok_modem_stuff = false;
#endif
        break;
      case 'R':
        num_min = stoi(argument);
        break;
      case 'U':
        this_usernum = StringToUnsignedShort(argument);
        if (!m_bUserAlreadyOn) {
          session()->SetCurrentSpeed("KB");
        }
        m_bUserAlreadyOn = true;
        break;
      case 'W': {
        ok_modem_stuff = false;
        this->InitializeBBS();
        this->doWFCEvents();
        exit (m_nOkLevel);
      } break;
      case 'X': {
        char argument2Char = wwiv::UpperCase<char>(argument.at(0));
        if (argument2Char == 'T') {
          // This more of a hack to make sure the Telnet
          // Server's -Bxxx parameter doesn't hose us.
          session()->SetCurrentSpeed("115200");
          session()->SetUserOnline(false);
          us                  = 115200U;
          ui                  = us;
          m_bUserAlreadyOn    = true;
          ooneuser            = true;
          session()->using_modem   = 0;
          hangup              = false;
          incom               = true;
          outcom              = false;
          global_xx           = false;
          bTelnetInstance = true;
        } else {
          clog << "Invalid Command line argument given '" << argumentRaw << "'\r\n\n";
          exit(m_nErrorLevel);
        }
      }
      break;
      case 'Z':
        no_hangup = true;
        break;
      //
      // TODO Add handling for Socket and Comm handle here
      //
      //
      case 'K': {
        this->InitializeBBS();
        session()->localIO()->LocalCls();
        if ((i + 1) < argc) {
          i++;
          bout << "\r\n|#7\xFE |#5Packing specified subs: \r\n";
          while (i < argc) {
            int nSubNumToPack = atoi(argv[i]);
            pack_sub(nSubNumToPack);
            sysoplogf("* Packed Message Area %d", nSubNumToPack);
            i++;
          }
        } else {
          bout << "\r\n|#7\xFE |#5Packing all subs: \r\n";
          sysoplog("* Packing All Message Areas");
          pack_all_subs();
        }
        ExitBBSImpl(m_nOkLevel);
      }
      break;
      case '?':
        ShowUsage();
        exit(0);
        break;
      case '-': {
        if (argumentRaw == "--help") {
          ShowUsage();
          exit(0);
        }
      }
      break;
      default: {
        clog << "Invalid Command line argument given '" << argument << "'\r\n\n";
        exit(m_nErrorLevel);
      }
      break;
      }
    } else {
      // Command line argument did not start with a '-' or a '/'
      clog << "Invalid Command line argument given '" << argumentRaw << "'\r\n\n";
      exit(m_nErrorLevel);
    }
  }

  // Add the environment variable or overwrite the existing one
#if !defined ( __unix__ )
  const string env_str = StringPrintf("WWIV_INSTANCE=%ld", GetInstanceNumber());
  putenv(env_str.c_str());
#else
  // For some reason putenv() doesn't work sometimes when setenv() does...
  const string env_str = StringPrintf("%u", GetInstanceNumber());
  setenv("WWIV_INSTANCE", env_str.c_str(), 1);
  m_bUserAlreadyOn = true;
#endif

  session()->CreateComm(hSockOrComm);
  this->InitializeBBS();

  if (!systemPassword.empty()) {
    strcpy(syscfg.systempw, systemPassword.c_str());
  }

  if (syscfg.sysconfig & sysconfig_no_local) {
    this_usernum = 0;
    m_bUserAlreadyOn = false;
  }
  session()->localIO()->UpdateNativeTitleBar();

  // If we are telnet...
  if (bTelnetInstance) {
    ok_modem_stuff = true;
    sess->remoteIO()->open();
  }

  if (num_min > 0) {
    syscfg.executetime = static_cast<uint16_t>((timer() + num_min * 60) / 60);
    if (syscfg.executetime > 1440) {
      syscfg.executetime -= 1440;
    }
    time_event = static_cast<double>(syscfg.executetime) * MINUTES_PER_HOUR_FLOAT;
    last_time = time_event - timer();
    if (last_time < 0.0) {
      last_time += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
    }
  }

  if (event_only) {
    unique_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
    cleanup_events();
    if (!IsEquals(date(), pStatus->GetLastDate())) {
      // This may be another node, but the user explicitly wanted to run the beginday
      // event from the commandline, so we'll just check the date.
      beginday(true);
    } else {
      sysoplog("!!! Wanted to run the beginday event when it's not required!!!", false);
      clog << "! WARNING: Tried to run beginday event again\r\n\n";
      sleep_for(seconds(2));
    }
    ExitBBSImpl(m_nOkLevel);
  }

  do {
    if (this_usernum) {
      session()->usernum = this_usernum;
      session()->ReadCurrentUser();
      if (!session()->user()->IsUserDeleted()) {
        GotCaller(ui, us);
        session()->usernum = this_usernum;
        session()->ReadCurrentUser();
        read_qscn(session()->usernum, qsc, false);
        session()->ResetEffectiveSl();
        changedsl();
        okmacro = true;
        if (!hangup && session()->usernum > 0 &&
            session()->user()->IsRestrictionLogon() &&
            IsEquals(date(), session()->user()->GetLastOn()) &&
            session()->user()->GetTimesOnToday() > 0) {
          bout << "\r\n|#6Sorry, you can only logon once per day.\r\n\n";
          hangup = true;
        }
      } else {
        this_usernum = 0;
      }
    }
    if (!this_usernum) {
      if (m_bUserAlreadyOn) {
        GotCaller(ui, us);
      }
#if !defined ( __unix__ )
      else {
        GetCaller();
      }
#endif
    }

    if (session()->using_modem > -1) {
      if (!this_usernum) {
        getuser();
      }
    } else {
      session()->using_modem = 0;
      okmacro = true;
      session()->usernum = m_unx;
      session()->ResetEffectiveSl();
      changedsl();
    }
    this_usernum = 0;
    if (!hangup) {
      logon();
      setiia(90);
      set_net_num(0);
      while (!hangup) {
        if (filelist) {
          free(filelist);
          filelist = nullptr;
        }
        zap_ed_info();
        write_inst(INST_LOC_MAIN, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
        wwiv::menus::mainmenu();
      }
      logoff();
    }

    if (!no_hangup && session()->using_modem && ok_modem_stuff) {
      hang_it_up();
    }
    catsl();
    frequent_init();
    if (session()->wfc_status == 0) {
      session()->localIO()->LocalCls();
    }
    cleanup_net();

    if (!no_hangup && ok_modem_stuff) {
      sess->remoteIO()->dtr(false);
    }
    m_bUserAlreadyOn = false;
    if (session()->localIO()->GetSysopAlert() && (!session()->localIO()->LocalKeyPressed())) {
      sess->remoteIO()->dtr(true);
      Wait(0.1);
      double dt = timer();
      session()->localIO()->LocalCls();
      bout << "\r\n>> SYSOP ALERT ACTIVATED <<\r\n\n";
      while (!session()->localIO()->LocalKeyPressed() && (fabs(timer() - dt) < SECONDS_PER_MINUTE_FLOAT)) {
        sound(500, milliseconds(250));
        sleep_for(milliseconds(1));
      }
      session()->localIO()->LocalCls();
    }
    session()->localIO()->SetSysopAlert(false);
  } while (!ooneuser);

  return m_nOkLevel;
}



void WApplication::ShowUsage() {
  cout << "WWIV Bulletin Board System [" << wwiv_version << " - " << beta_version << "]\r\n\n" <<
            "Usage:\r\n\n" <<
            "bbs -I<inst> [options] \r\n\n" <<
            "Options:\r\n\n" <<
            "  -?         - Display command line options (This screen)\r\n\n" <<
            "  -A<level>  - Specify the Error Exit Level\r\n" <<
            "  -B<rate>   - Someone already logged on at rate (modem speed)\r\n" <<
            "  -E         - Load for beginday event only\r\n" <<
            "  -F         - Pass full result code (\"CONNECT 33600/ARQ/HST/V.42BIS\")\r\n" <<
            "  -H<handle> - Socket or Comm handle\r\n" <<
            "  -I<inst>   - Designate instance number <inst>\r\n" <<
            "  -K [# # #] - Pack Message Areas, optionally list the area(s) to pack\r\n" <<
            "  -M         - Don't access modem at all\r\n" <<
            "  -N<inst>   - Designate instance number <inst>\r\n" <<
            "  -O         - Quit WWIV after one user done\r\n" <<
            "  -P<pass>   - Set System Password to <pass>\r\n" <<
            "  -Q<level>  - Normal exit level\r\n" <<
            "  -R<min>    - Specify max # minutes until event\r\n" <<
            "  -S<rate>   - Used only with -B, indicates com port speed\r\n" <<
#if defined (_WIN32)
            "  -TELSRV    - Uses internet telnet server to answer incomming session\r\n" <<
#endif // _WIN32
            "  -U<user#>  - Pass usernumber <user#> online\r\n" <<
#if defined (_WIN32)
            "  -XT        - Someone already logged on via telnet (socket handle)\r\n" <<
#endif // _WIN32
            "  -Z         - Do not hang up on user when at log off\r\n" <<
            endl;
}

WApplication::WApplication() : statusMgr(new StatusMgr()), userManager(new WUserManager()), m_nOkLevel(exitLevelOK), 
    m_nErrorLevel(exitLevelNotOK), instance_number(-1), m_bUserAlreadyOn(false), m_nBbsShutdownStatus(shutdownNone), m_fShutDownTime(0),
    m_nWfcStatus(0) {
  // TODO this should move into the WSystemConfig object (syscfg wrapper) once it is established.
  if (syscfg.userreclen == 0) {
    syscfg.userreclen = sizeof(userrec);
  }
  tzset();
  // Set the home directory
  getcwd(m_szCurrentDirectory, MAX_PATH);
}

void WApplication::CdHome() {
  chdir(m_szCurrentDirectory);
}

const string WApplication::GetHomeDir() {
  string dir = m_szCurrentDirectory;
  File::EnsureTrailingSlash(&dir);
  return string(dir);
}

void WApplication::AbortBBS(bool bSkipShutdown) {
  clog.flush();
  if (bSkipShutdown) {
    exit(m_nErrorLevel);
  } else {
    ExitBBSImpl(m_nErrorLevel);
  }
}

void WApplication::QuitBBS() {
  ExitBBSImpl(WApplication::exitLevelQuit);
}

void WApplication::ExitBBSImpl(int nExitLevel) {
  sysoplog("", false);
  sysoplogfi(false, "WWIV %s, inst %u, taken down at %s on %s with exit code %d.",
             wwiv_version, GetInstanceNumber(), times(), fulldate(), nExitLevel);
  sysoplog("", false);
  catsl();
  write_inst(INST_LOC_DOWN, 0, INST_FLAGS_NONE);
  clog << "\r\n";
  clog << "WWIV Bulletin Board System " << wwiv_version << beta_version << " exiting at error level " << nExitLevel
       << endl << endl;
  delete this;
  exit(nExitLevel);
}

bool WApplication::LogMessage(const char* pszFormat, ...) {
  va_list ap;
  char szBuffer[2048];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, ap);
  va_end(ap);
  sysoplog(szBuffer);
  return true;
}

void WApplication::UpdateTopScreen() {
  if (!GetWfcStatus()) {
    unique_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
    session()->localIO()->UpdateTopScreen(pStatus.get(), session(), GetInstanceNumber());
  }
}

void WApplication::ShutDownBBS(int nShutDownStatus) {
  char xl[81], cl[81], atr[81], cc;
  session()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);

  switch (nShutDownStatus) {
  case 1:
    SetShutDownTime(timer() + 180.0);
  case 2:
  case 3:
    SetShutDownStatus(nShutDownStatus);
    bout.nl(2);
    bout << "|#7***\r\n|#7To All Users, System will shut down in " <<
                       4 - GetShutDownStatus() << " minunte(s) for maintenance.\r \n" <<
                       "|#7Please finish your session and log off. Thank you\r\n|#7***\r\n";
    break;
  case 4:
    bout.nl(2);
    bout << "|#7***\r\n|#7Please call back later.\r\n|#7***\r\n\n";
    session()->user()->SetExtraTime(session()->user()->GetExtraTime() + static_cast<float>
        (nsl()));
    bout << "Time on   = " << ctim(timer() - timeon) << wwiv::endl;
    printfile(LOGOFF_NOEXT);
    hangup = true;
    SetShutDownStatus(WApplication::shutdownNone);
    break;
  default:
    clog << "[utility.cpp] shutdown called with illegal type: " << nShutDownStatus << endl;
    WWIV_ASSERT(false);
  }
  RestoreCurrentLine(cl, atr, xl, &cc);
}


void WApplication::UpdateShutDownStatus() {
  if (IsShutDownActive()) {
    if (((GetShutDownTime() - timer()) < 120) && ((GetShutDownTime() - timer()) > 60)) {
      if (GetShutDownStatus() != WApplication::shutdownTwoMinutes) {
        ShutDownBBS(WApplication::shutdownTwoMinutes);
      }
    }
    if (((GetShutDownTime() - timer()) < 60) && ((GetShutDownTime() - timer()) > 0)) {
      if (GetShutDownStatus() != WApplication::shutdownOneMinute) {
        ShutDownBBS(WApplication::shutdownOneMinute);
      }
    }
    if ((GetShutDownTime() - timer()) <= 0) {
      ShutDownBBS(WApplication::shutdownImmediate);
    }
  }
}

void WApplication::ToggleShutDown() {
  if (IsShutDownActive()) {
    SetShutDownStatus(WApplication::shutdownNone);
  } else {
    ShutDownBBS(WApplication::shutdownThreeMinutes);
  }
}

WApplication::~WApplication() {
  if (sess != nullptr) {
    delete sess;
    sess = nullptr;
  }
}

WApplication* CreateApplication(WLocalIO* localIO) {
  app = new WApplication();
  sess = new WSession(app, localIO);
  File::SetLogger(app);
  return app;
}

int bbsmain(int argc, char *argv[]) {
  try {
    CreateApplication(nullptr);
    return application()->BBSMainLoop(argc, argv);
  } catch (exception& e) {
    // TODO(rushfan): Log this to sysop log or where else?
    clog << "BBS Terminated by exception: " << e.what() << endl;
    return 1;
  }
}
