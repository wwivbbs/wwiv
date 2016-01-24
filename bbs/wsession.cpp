/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
// include this here so it won't get includes by local_io_win32.h
#include "bbs/wwiv_windows.h"
#endif  // WIN32

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <cstdarg>
#include "bbs/asv.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/chnedit.h"
#include "bbs/bgetch.h"
#include "bbs/bputch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/events.h"
#include "bbs/external_edit.h"
#include "bbs/fcns.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/local_io.h"
#include "bbs/local_io_curses.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/menu.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/ssh.h"
#include "bbs/sysoplog.h"
#include "bbs/uedit.h"
#include "bbs/utility.h"
#include "bbs/voteedit.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wfc.h"
#include "bbs/wsession.h"
#include "bbs/workspace.h"
#include "bbs/wstatus.h"
#include "bbs/platform/platformfcns.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"

#if defined( _WIN32 )
#include "bbs/wiot.h"
#include "bbs/local_io_win32.h"
#else
#include "bbs/platform/unix/wiou.h"
#endif // _WIN32

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
using namespace wwiv::sdk;
using namespace wwiv::strings;

extern time_t last_time_c;
WOutStream bout;

WSession::WSession(WApplication* app, LocalIO* localIO) : application_(app), 
    local_io_(localIO),
    statusMgr(new StatusMgr()), 
    m_nOkLevel(exitLevelOK),
    m_nErrorLevel(exitLevelNotOK),
    m_nBbsShutdownStatus(shutdownNone) {
  ::bout.SetLocalIO(localIO);

  memset(&newuser_colors, 0, sizeof(newuser_colors));
  memset(&newuser_bwcolors, 0, sizeof(newuser_bwcolors));
  memset(&asv, 0, sizeof(asv_rec));
  memset(&advasv, 0, sizeof(adv_asv_rec));

  // Set the home directory
  current_dir_ = File::current_directory();
}

WSession::~WSession() {
  if (comm_ && ok_modem_stuff) {
    comm_->close();
  }
  if (local_io_) {
    local_io_->SetCursor(LocalIO::cursorNormal);
  }
}

bool WSession::reset_local_io(LocalIO* wlocal_io) {
  local_io_.reset(wlocal_io);

  const int screen_bottom = localIO()->GetDefaultScreenBottom();
  localIO()->SetScreenBottom(screen_bottom);
  defscreenbottom = screen_bottom;
  screenlinest = screen_bottom + 1;

  ClearTopScreenProtection();
  ::bout.SetLocalIO(wlocal_io);
  return true;
}

void WSession::CreateComm(unsigned int nHandle, CommunicationType type) {
#ifdef __unix__
  comm_.reset(new WIOUnix());
  return;
#endif
if (type == CommunicationType::SSH) {
    const File key_file(config_->datadir(), "wwiv.key");
    const string system_password = config()->config()->systempw;
    wwiv::bbs::Key key(key_file.full_pathname(), system_password);
    if (!key.Open()) {
      if (!key.Create()) {
        clog << "Unable to create or open key file!.  SSH will be disabled!" << endl;
        type = CommunicationType::TELNET;
      }
    }
    comm_.reset(new wwiv::bbs::IOSSH(nHandle, key));
  } else {
    comm_.reset(new WIOTelnet(nHandle));
  }
  bout.SetComm(comm_.get());
}

bool WSession::ReadCurrentUser(int user_number) {
  bool result = users()->ReadUser(&m_thisuser, user_number);

  // Update all other session variables that are dependent.
  screenlinest = (using_modem) ? 
      user()->GetScreenLines() : defscreenbottom + 1;
  return result;
}

bool WSession::WriteCurrentUser(int user_number) {
  return users()->WriteUser(&m_thisuser, user_number);
}

void WSession::tleft(bool check_for_timeout) {
  double nsln = nsl();
  bool temp_sysop = session()->user()->GetSl() != 255 && session()->GetEffectiveSl() == 255;
  bool sysop = sysop1();
  localIO()->tleft(this, temp_sysop, sysop, IsUserOnline());
  if (check_for_timeout && IsUserOnline()) {
    if (nsln == 0.0) {
      bout << "\r\nTime expired.\r\n\n";
      hangup = true;
    }
  }
}

void WSession::handle_sysop_key(uint8_t key) {
  int i, i1;

  if (okskey) {
    if (key >= AF1 && key <= AF10) {
      set_autoval(key - 104);
    } else {
      switch (key) {
      case F1:                          /* F1 */
        OnlineUserEditor();
        break;
      case SF1:
        /* Shift-F1 */
        // Nothing.
        UpdateTopScreen();
        break;
      case CF1:                          /* Ctrl-F1 */
        ToggleShutDown();
        break;
      case F2:                          /* F2 */
        topdata++;
        if (topdata > LocalIO::topdataUser) {
          topdata = LocalIO::topdataNone;
        }
        UpdateTopScreen();
        break;
      case F3:                          /* F3 */
        if (using_modem) {
          incom = !incom;
          dump();
          tleft(false);
        }
        break;
      case F4:                          /* F4 */
        chatcall = false;
        UpdateTopScreen();
        break;
      case F5:                          /* F5 */
        hangup = true;
        remoteIO()->dtr(false);
        break;
      case SF5:                          /* Shift-F5 */
        i1 = (rand() % 20) + 10;
        for (i = 0; i < i1; i++) {
          bputch(static_cast<unsigned char>(rand() % 256));
        }
        hangup = true;
        remoteIO()->dtr(false);
        break;
      case CF5:                          /* Ctrl-F5 */
        bout << "\r\nCall back later when you are there.\r\n\n";
        hangup = true;
        remoteIO()->dtr(false);
        break;
      case F6:                          /* F6 - was Toggle Sysop Alert*/
        tleft(false);
        break;
      case F7:                          /* F7 */
        user()->SetExtraTime(user()->GetExtraTime() -
          static_cast<float>(5.0 * SECONDS_PER_MINUTE_FLOAT));
        tleft(false);
        break;
      case F8:                          /* F8 */
        user()->SetExtraTime(user()->GetExtraTime() +
          static_cast<float>(5.0 * SECONDS_PER_MINUTE_FLOAT));
        tleft(false);
        break;
      case F9:                          /* F9 */
        if (user()->GetSl() != 255) {
          if (GetEffectiveSl() != 255) {
            SetEffectiveSl(255);
          } else {
            ResetEffectiveSl();
          }
          changedsl();
          tleft(false);
        }
        break;
      case F10:                          /* F10 */
        if (chatting == 0) {
          if (syscfg.sysconfig & sysconfig_2_way) {
            chat1("", true);
          } else {
            chat1("", false);
          }
        } else {
          chatting = 0;
        }
        break;
      case CF10:                         /* Ctrl-F10 */
        if (chatting == 0) {
          chat1("", false);
        } else {
          chatting = 0;
        }
        break;
      case HOME:                          /* HOME */
        if (chatting == 1) {
          chat_file = !chat_file;
        }
        break;
      }
    }
  }
}

void WSession::DisplaySysopWorkingIndicator(bool displayWait) {
  const string waitString = "-=[WAIT]=-";
  auto nNumPrintableChars = waitString.length();
  for (std::string::const_iterator iter = waitString.begin(); iter != waitString.end(); ++iter) {
    if (*iter == 3 && nNumPrintableChars > 1) {
      nNumPrintableChars -= 2;
    }
  }

  if (displayWait) {
    if (okansi()) {
      int nSavedAttribute = curatr;
      bout.SystemColor(user()->HasColor() ? user()->GetColor(3) : user()->GetBWColor(3));
      bout << waitString << "\x1b[" << nNumPrintableChars << "D";
      bout.SystemColor(static_cast<unsigned char>(nSavedAttribute));
    } else {
      bout << waitString;
    }
  } else {
    if (okansi()) {
      for (unsigned int j = 0; j < nNumPrintableChars; j++) {
        bputch(' ');
      }
      bout << "\x1b[" << nNumPrintableChars << "D";
    } else {
      for (unsigned int j = 0; j < nNumPrintableChars; j++) {
        bout.bs();
      }
    }
  }
}

void WSession::UpdateTopScreen() {
  if (!GetWfcStatus()) {
    unique_ptr<WStatus> pStatus(status_manager()->GetStatus());
    localIO()->UpdateTopScreen(pStatus.get(), session(), instance_number());
  }
}

void WSession::ClearTopScreenProtection() {
  localIO()->set_protect(this, 0);
}


const char* WSession::network_name() const {
  if (net_networks.empty()) {
    return "";
  }
  return net_networks[m_nNetworkNumber].name;
}

const std::string WSession::network_directory() const {
  if (net_networks.empty()) {
    return "";
  }
  return std::string(net_networks[m_nNetworkNumber].dir);
}


#if !defined ( __unix__ )
void WSession::GetCaller() {
  SetShutDownStatus(WSession::shutdownNone);
  wfc_init();
  remoteIO()->ClearRemoteInformation();
  frequent_init();
  if (wfc_status == 0) {
    localIO()->LocalCls();
  }
  usernum = 0;
  SetWfcStatus(0);
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
  ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  usernum = 1;
  ResetEffectiveSl();
  fwaiting = user()->GetNumMailWaiting();
  if (user()->IsUserDeleted()) {
    user()->SetScreenChars(80);
    user()->SetScreenLines(25);
  }
  screenlinest = defscreenbottom + 1;

  int lokb = doWFCEvents();

  if (lokb) {
    modem_speed = 14400;
  }

  using_modem = incom;
  if (lokb == 2) {
    using_modem = -1;
  }

  okskey = true;
  localIO()->LocalCls();
  localIO()->LocalPrintf("Logging on at %s ...\r\n",
    GetCurrentSpeed().c_str());
  SetWfcStatus(0);
}

#else  // _unix__

void wfc_screen() {}
void wfc_cls() {}

#endif  // __unix__

int WSession::doWFCEvents() {
  unsigned char ch;
  int lokb;
  LocalIO* io = localIO();

  unique_ptr<WStatus> last_date_status(status_manager()->GetStatus());
  do {
    write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    set_net_num(0);
    bool any = false;
    SetWfcStatus(1);

    // If the date has changed since we last checked, then then run the beginday event.
    if (!IsEquals(date(), last_date_status->GetLastDate())) {
      if ((GetBeginDayNodeNumber() == 0) || (instance_number_ == GetBeginDayNodeNumber())) {
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
    SetCurrentSpeed("KB");
    time_t lCurrentTime = time(nullptr);
    if (!any && (((rand() % 8000) == 0) || (lCurrentTime - last_time_c > 1200)) &&
      net_sysnum && (this->flags & OP_FLAGS_NET_CALLOUT)) {
      lCurrentTime = last_time_c;
      attempt_callout();
      any = true;
    }
    wfc_screen();
    okskey = false;
    if (io->LocalKeyPressed()) {
      SetWfcStatus(0);
      ReadCurrentUser(1);
      read_qscn(1, qsc, false);
      fwaiting = user()->GetNumMailWaiting();
      SetWfcStatus(1);
      ch = wwiv::UpperCase<char>(io->LocalGetChar());
      if (ch == 0) {
        ch = io->LocalGetChar();
        handle_sysop_key(ch);
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
      io->SetCursor(LocalIO::cursorNormal);
      switch (ch) {
        // Local Logon
      case SPACE:
        lokb = this->LocalLogon();
        break;
        // Show WFC Menu
      case '?': {
          string helpFileName = SWFC_NOEXT;
          char chHelp = ESC;
          do {
            io->LocalCls();
            bout.nl();
            printfile(helpFileName);
            chHelp = getkey();
            helpFileName = (helpFileName == SWFC_NOEXT) ? SONLINE_NOEXT : SWFC_NOEXT;
          } while (chHelp != SPACE && chHelp != ESC);
        }
        break;
        // Force Network Callout
      case '/':
        if (net_sysnum) {
          force_callout(0);
        }
        break;
        // War Dial Connect
      case '.':
        if (net_sysnum) {
          force_callout(1);
        }
        break;

        // Fast Net Callout from WFC
      case '*':
      {
        io->LocalCls(); // added   - 02/23/14 - dsn
        do_callout(32767); // changed - 02/23/14 - dsn - changed 1160 to 32767
      } break;
      // Run MenuEditor
      case '!':
        EditMenus();
        break;
        // Print NetLogs
      case ',':
        if (net_sysnum > 0 || !net_networks.empty()) {
          io->LocalGotoXY(2, 23);
          bout << "|#7(|#2Q|#7=|#2Quit|#7) Display Which NETDAT Log File (|#10|#7-|#12|#7): ";
          ch = onek("Q012");
          switch (ch) {
          case '0':
          case '1':
          case '2':
          {
            print_local_file(StringPrintf("netdat%c.log", ch));
          }
          break;
          }
        }
        break;
        // Net List
      case '`':
        if (net_sysnum) {
          print_net_listing(true);
        }
        break;
        // [TAB] Instance Editor
      case TAB:
        wfc_cls();
        instance_edit();
        break;
        // [ESC] Quit the BBS
      case ESC:
        io->LocalGotoXY(2, 23);
        bout << "|#7Exit the BBS? ";
        if (yesno()) {
          QuitBBS();
        }
        io->LocalCls();
        break;
        // BoardEdit
      case 'B':
        write_inst(INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE);
        boardedit();
        cleanup_net();
        break;
        // ChainEdit
      case 'C':
        write_inst(INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE);
        chainedit();
        break;
        // DirEdit
      case 'D':
        write_inst(INST_LOC_DIREDIT, 0, INST_FLAGS_NONE);
        dlboardedit();
        break;
        // Send Email
      case 'E':
        wfc_cls();
        usernum = 1;
        bout << "|#1Send Email:";
        send_email();
        WriteCurrentUser(1);
        cleanup_net();
        break;
        // GfileEdit
      case 'G':
        write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
        gfileedit();
        break;
        // EventEdit
      case 'H':
        write_inst(INST_LOC_EVENTEDIT, 0, INST_FLAGS_NONE);
        eventedit();
        break;
        // InitVotes
      case 'I':
        wfc_cls();
        write_inst(INST_LOC_VOTEEDIT, 0, INST_FLAGS_NONE);
        ivotes();
        break;
        // ConfEdit
      case 'J':
        wfc_cls();
        edit_confs();
        break;
        // SendMailFile
      case 'K': {
          wfc_cls();
          usernum = 1;
          bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
          string buffer = input(50);
          LoadFileIntoWorkspace(buffer, false);
          send_email();
          WriteCurrentUser(1);
          cleanup_net();
        }
        break;
        // Print Log Daily logs
      case 'L': {
          wfc_cls();
          unique_ptr<WStatus> pStatus(status_manager()->GetStatus());
          print_local_file(pStatus->GetLogFileName(0));
        }
        break;
        // Read User Mail
      case 'M': {
          wfc_cls();
          usernum = 1;
          readmail(0);
          WriteCurrentUser(1);
          cleanup_net();
        }
        break;
        // Print Net Log
      case 'N': {
          wfc_cls();
          print_local_file("net.log");
        }
        break;
        // EditTextFile
      case 'O': {
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
      case 'P':{
          wfc_cls();
          print_pending_list();
        }
        break;
        // Quit BBS
      case 'Q':
        io->LocalGotoXY(2, 23);
        QuitBBS();
        break;
        // Read All Mail
      case 'R':
        wfc_cls();
        write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
        mailr();
        break;
        // Print Current Status
      case 'S':
        prstatus();
        getkey();
        break;
        // UserEdit
      case 'U':
        write_inst(INST_LOC_UEDIT, 0, INST_FLAGS_NONE);
        uedit(1, UEDIT_NONE);
        break;
        // Send Internet Mail
      case 'V': {
          wfc_cls();
          usernum = 1;
          SetUserOnline(true);
          get_user_ppp_addr();
          send_inet_email();
          SetUserOnline(false);
          WriteCurrentUser(1);
          cleanup_net();
        }
        break;
        // Edit Gfile
      case 'W': {
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
      case 'Y': {
          wfc_cls();
          unique_ptr<WStatus> pStatus(status_manager()->GetStatus());
          print_local_file(pStatus->GetLogFileName(1));
        }
        break;
        // Print Activity (Z) Log
      case 'Z': {
          zlog();
          bout.nl();
          getkey();
        }
        break;
      }
      wfc_cls();  // moved from after getch
      if (!incom && !lokb) {
        frequent_init();
        ReadCurrentUser(1);
        read_qscn(1, qsc, false);
        fwaiting = user()->GetNumMailWaiting();
        ResetEffectiveSl();
        usernum = 1;
      }
      catsl();
      write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    }

    if (!any) {
      static int mult_time = 0;
      if (this->IsCleanNetNeeded() || std::abs(timer1() - mult_time) > 1000L) {
        cleanup_net();
        mult_time = timer1();
      }
      giveup_timeslice();
    }
  } while (!incom && !lokb);
  return lokb;
}


int WSession::LocalLogon() {
  localIO()->LocalGotoXY(2, 23);
  bout << "|#9Log on to the BBS?";
  double d = timer();
  int lokb = 0;
  while (!localIO()->LocalKeyPressed() && (std::abs(timer() - d) < SECONDS_PER_MINUTE_FLOAT))
    ;

  if (localIO()->LocalKeyPressed()) {
    char ch = wwiv::UpperCase<char>(localIO()->LocalGetChar());
    if (ch == 'Y') {
      localIO()->LocalPuts(YesNoString(true));
      bout << wwiv::endl;
      lokb = 1;
    } else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The ch == 224 is a Win32'ism
      localIO()->LocalGetChar();
    } else {
      bool fast = false;

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
      if (!fast || m_unx > status_manager()->GetUserCount()) {
        return lokb;
      }

      User tu;
      users()->ReadUserNoCache(&tu, m_unx);
      if (tu.GetSl() != 255 || tu.IsUserDeleted()) {
        return lokb;
      }

      usernum = m_unx;
      int nSavedWFCStatus = GetWfcStatus();
      SetWfcStatus(0);
      ReadCurrentUser();
      read_qscn(usernum, qsc, false);
      SetWfcStatus(nSavedWFCStatus);
      bputch(ch);
      localIO()->LocalPuts("\r\n\r\n\r\n\r\n\r\n\r\n");
      lokb = 2;
      ResetEffectiveSl();
      changedsl();
      if (!set_language(user()->GetLanguage())) {
        user()->SetLanguage(0);
        set_language(0);
      }
      return lokb;
    }
    if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The 224 is a Win32'ism
      localIO()->LocalGetChar();
    }
  }
  if (lokb == 0) {
    localIO()->LocalCls();
  }
  return lokb;
}

void WSession::GotCaller(unsigned int ms, unsigned long cs) {
  frequent_init();
  if (wfc_status == 0) {
    localIO()->LocalCls();
  }
  com_speed = cs;
  modem_speed = ms;
  ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  ResetEffectiveSl();
  usernum = 1;
  if (user()->IsUserDeleted()) {
    user()->SetScreenChars(80);
    user()->SetScreenLines(25);
  }
  localIO()->LocalCls();
  localIO()->LocalPrintf("Logging on at %s...\r\n", GetCurrentSpeed().c_str());
  if (ms) {
    incom = true;
    outcom = true;
    using_modem = 1;
  } else {
    using_modem = 0;
    incom = false;
    outcom = false;
  }
}


void WSession::CdHome() {
  File::set_current_directory(current_dir_);
}

const string WSession::GetHomeDir() {
  string dir = current_dir_;
  File::EnsureTrailingSlash(&dir);
  return dir;
}

void WSession::AbortBBS(bool bSkipShutdown) {
  clog.flush();
  if (bSkipShutdown) {
    exit(m_nErrorLevel);
  } else {
    ExitBBSImpl(m_nErrorLevel);
  }
}

void WSession::QuitBBS() {
  ExitBBSImpl(WSession::exitLevelQuit);
}

void WSession::ExitBBSImpl(int nExitLevel) {
  if (nExitLevel != WSession::exitLevelOK && nExitLevel != WSession::exitLevelQuit) {
    // Only log the exiting at absnomal error levels, since we see lots of exiting statements
    // in the logs that don't correspond to sessions every being created (network probers, etc).
    sysoplog("", false);
    sysoplogfi(false, "WWIV %s, inst %u, taken down at %s on %s with exit code %d.",
      wwiv_version, instance_number(), times(), fulldate(), nExitLevel);
    sysoplog("", false);
  }
  catsl();
  write_inst(INST_LOC_DOWN, 0, INST_FLAGS_NONE);
  clog << "\r\n";
  clog << "WWIV Bulletin Board System " << wwiv_version << beta_version << " exiting at error level " << nExitLevel
    << endl << endl;
  delete this;
  exit(nExitLevel);
}


void WSession::ShutDownBBS(int nShutDownStatus) {
  char xl[81], cl[81], atr[81], cc;
  localIO()->SaveCurrentLine(cl, atr, xl, &cc);

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
    user()->SetExtraTime(user()->GetExtraTime() + static_cast<float>
      (nsl()));
    bout << "Time on   = " << ctim(timer() - timeon) << wwiv::endl;
    printfile(LOGOFF_NOEXT);
    hangup = true;
    SetShutDownStatus(WSession::shutdownNone);
    break;
  default:
    clog << "[utility.cpp] shutdown called with illegal type: " << nShutDownStatus << endl;
    WWIV_ASSERT(false);
  }
  RestoreCurrentLine(cl, atr, xl, &cc);
}

void WSession::UpdateShutDownStatus() {
  if (!IsShutDownActive()) {
    return;
  }
  if (((GetShutDownTime() - timer()) < 120) && ((GetShutDownTime() - timer()) > 60)) {
    if (GetShutDownStatus() != WSession::shutdownTwoMinutes) {
      ShutDownBBS(WSession::shutdownTwoMinutes);
    }
  }
  if (((GetShutDownTime() - timer()) < 60) && ((GetShutDownTime() - timer()) > 0)) {
    if (GetShutDownStatus() != WSession::shutdownOneMinute) {
      ShutDownBBS(WSession::shutdownOneMinute);
    }
  }
  if ((GetShutDownTime() - timer()) <= 0) {
    ShutDownBBS(WSession::shutdownImmediate);
  }
}

void WSession::ToggleShutDown() {
  if (IsShutDownActive()) {
    SetShutDownStatus(WSession::shutdownNone);
  } else {
    ShutDownBBS(WSession::shutdownThreeMinutes);
  }
}


void WSession::ShowUsage() {
  cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]\r\n\n" <<
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
    "  -Q<level>  - Normal exit level\r\n" <<
    "  -R<min>    - Specify max # minutes until event\r\n" <<
    "  -S<rate>   - Used only with -B, indicates com port speed\r\n" <<
    "  -U<user#>  - Pass usernumber <user#> online\r\n" <<
    "  -V         - Display WWIV Version\r\n" <<
    "  -W         - Display Local 'WFC' menu\r\n" <<
#if defined (_WIN32)
    "  -XT        - Someone already logged on via telnet (socket handle)\r\n" <<
#endif // _WIN32
    "  -Z         - Do not hang up on user when at log off\r\n"
    << endl;
}

int WSession::Run(int argc, char *argv[]) {
  int num_min = 0;
  unsigned int ui = 0;
  unsigned long us = 0;
  unsigned short this_usernum = 0;
  bool ooneuser = false;
  bool event_only = false;
  CommunicationType type = CommunicationType::TELNET;
  unsigned int hSockOrComm = 0;

  curatr = 0x07;
  // Set the instance, this may be changed by a command line argument
  instance_number_ = 1;
  no_hangup = false;
  ok_modem_stuff = true;

  const std::string bbs_env = environment_variable("BBS");
  if (!bbs_env.empty()) {
    if (bbs_env.find("WWIV") != string::npos) {
      std::cerr << "You are already in the BBS, type 'EXIT' instead.\n\n";
      exit(255);
    }
  }
  const string wwiv_dir = environment_variable("WWIV_DIR");
  if (!wwiv_dir.empty()) {
    File::set_current_directory(wwiv_dir);
  }

#if defined( __unix__ )
  // HACK to make WWIV5/X just work w/o any command line
  m_bUserAlreadyOn = true;
  ui = us = 9600;
  ooneuser = true;
#endif

  for (int i = 1; i < argc; i++) {
    string argumentRaw = argv[i];
    if (argumentRaw.length() > 1 && (argumentRaw[0] == '-' || argumentRaw[0] == '/')) {
      string argument = argumentRaw.substr(2);
      char ch = wwiv::UpperCase<char>(argumentRaw[1]);
      switch (ch) {
      case 'B':
      {
        // I think this roundtrip here is just to ensure argument is really a number.
        ui = static_cast<unsigned int>(atol(argument.c_str()));
        const string current_speed_string = std::to_string(ui);
        SetCurrentSpeed(current_speed_string.c_str());
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
      case 'I':
      case 'N':
      {
        instance_number_ = stoi(argument);
        if (instance_number_ <= 0 || instance_number_ > 999) {
          clog << "Your Instance can only be 1..999, you tried instance #" << instance_number_ << endl;
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
          SetCurrentSpeed("KB");
        }
        m_bUserAlreadyOn = true;
        break;
      case 'V':
        cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]" << endl;
        exit(0);
        break;
      case 'W':
      {
        ok_modem_stuff = false;
        this->InitializeBBS();
        wwiv::wfc::ControlCenter control_center;
        control_center.Run();
        exit(m_nOkLevel);
      } break;
      case 'X':
      {
        char argument2Char = wwiv::UpperCase<char>(argument.at(0));
        if (argument2Char == 'T' || argument2Char == 'S') {
          // This more of a hack to make sure the Telnet
          // Server's -Bxxx parameter doesn't hose us.
          SetCurrentSpeed("115200");

          // These are needed for both Telnet or SSH
          SetUserOnline(false);
          us = 115200;
          ui = us;
          m_bUserAlreadyOn = true;
          ooneuser = true;
          using_modem = 0;
          hangup = false;
          incom = true;
          outcom = false;
          global_xx = false;
          if (argument2Char == 'T') {
            type = CommunicationType::TELNET;
          } else if (argument2Char == 'S') {
            type = CommunicationType::SSH;
            //cout << "Waiting for debugger" << endl;
            //getchar();
          }
        } else {
          clog << "Invalid Command line argument given '" << argumentRaw << "'" << std::endl;
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
      case 'K':
      {
        this->InitializeBBS();
        localIO()->LocalCls();
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
          wwiv::bbs::TempDisablePause disable_pause;
          if (!pack_all_subs()) {
            bout << "|#6Aborted.\r\n";
          }
        }
        ExitBBSImpl(m_nOkLevel);
      }
      break;
      case '?':
        ShowUsage();
        exit(0);
        break;
      case '-':
      {
        if (argumentRaw == "--help") {
          ShowUsage();
          exit(0);
        }
      }
      break;
      default:
      {
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
  const string env_str = std::to_string(instance_number());
  set_environment_variable("WWIV_INSTANCE", env_str);
#ifndef _WIN32
  // TODO(rushfan): Don't think we need this, but need to make sure.
  // it was already there.
  m_bUserAlreadyOn = true;
#endif  // _WIN32
  if (!ReadConfig()) {
    // Gotta read the config before we can create the socket handles.
    // Since we may need the SSH key.
    AbortBBS(true);
  }

  CreateComm(hSockOrComm, type);
  InitializeBBS();
  localIO()->UpdateNativeTitleBar(this);

  // If we are telnet...
  if (type == CommunicationType::TELNET || type == CommunicationType::SSH) {
    ok_modem_stuff = true;
    remoteIO()->open();
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
    unique_ptr<WStatus> pStatus(status_manager()->GetStatus());
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
      usernum = this_usernum;
      ReadCurrentUser();
      if (!user()->IsUserDeleted()) {
        GotCaller(ui, us);
        usernum = this_usernum;
        ReadCurrentUser();
        read_qscn(usernum, qsc, false);
        ResetEffectiveSl();
        changedsl();
        okmacro = true;
        if (!hangup && usernum > 0 &&
          user()->IsRestrictionLogon() &&
          IsEquals(date(), user()->GetLastOn()) &&
          user()->GetTimesOnToday() > 0) {
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

    if (using_modem > -1) {
      if (!this_usernum) {
        getuser();
      }
    } else {
      using_modem = 0;
      okmacro = true;
      usernum = m_unx;
      ResetEffectiveSl();
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
        write_inst(INST_LOC_MAIN, usub[GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
        wwiv::menus::mainmenu();
      }
      logoff();
    }

    if (!no_hangup && using_modem && ok_modem_stuff) {
      hang_it_up();
    }
    catsl();
    frequent_init();
    if (wfc_status == 0) {
      localIO()->LocalCls();
    }
    cleanup_net();

    if (!no_hangup && ok_modem_stuff) {
      remoteIO()->dtr(false);
    }
    m_bUserAlreadyOn = false;
  } while (!ooneuser);

  return m_nOkLevel;
}

