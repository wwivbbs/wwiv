/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "bbs/wfc.h"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "bbs/wwiv.h"
#include "bbs/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/local_io_curses.h"
#include "bbs/printfile.h"
#include "bbs/voteedit.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "core/inifile.h"
#include "initlib/curses_io.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using wwiv::core::FilePath;
using wwiv::os::random_number;
using namespace wwiv::strings;


namespace wwiv {
namespace wfc {

namespace {
static CursesWindow* CreateBoxedWindow(const std::string& title, int nlines, int ncols, int y, int x) {
  unique_ptr<CursesWindow> window(new CursesWindow(out->window(), out->color_scheme(), nlines, ncols, y, x));
  window->SetColor(SchemeId::WINDOW_BOX);
  window->Box(0, 0);
  window->SetTitle(title);
  window->SetColor(SchemeId::WINDOW_TEXT);
  return window.release();
}
}

auto noop = [](){};

static void wfc_command(int instance_location_id, std::function<void()> f, 
    std::function<void()> f2 = noop, std::function<void()> f3 = noop, std::function<void()> f4 = noop) {
  session()->reset_local_io(new CursesLocalIO(out->window()->GetMaxY()));

  if (!AllowLocalSysop()) {
    // TODO(rushfan): Show messagebox error?
    return;
  }
  wfc_cls();
  write_inst(instance_location_id, 0, INST_FLAGS_NONE);
  f();
  f2();
  f3();
  f4();
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
}

auto send_email_f = []() {
  session()->usernum = 1;
  bout << "|#1Send Email:";
  send_email();
  session()->WriteCurrentUser(1);
};

auto view_sysop_log_f = []() {
  unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
  const string sysop_log_file = GetSysopLogFileName(date());
  print_local_file(sysop_log_file);
};

auto view_yesterday_sysop_log_f = []() {
  unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
  print_local_file(pStatus->GetLogFileName(1));
};

auto read_mail_f = []() {
  session()->usernum = 1;
  readmail(0);
  session()->WriteCurrentUser(1);
};

auto getkey_f = []() { getkey(); };

ControlCenter::ControlCenter() {
  const string title = StringPrintf("WWIV %s%s Server.", wwiv_version, beta_version);
  CursesIO::Init(title);
  // take ownership of out.
  out_scope_.reset(out);
  application()->SetWfcStatus(0);
}

ControlCenter::~ControlCenter() {}

static void DrawCommands(CursesWindow* commands) {
  commands->PutsXY(1, 1, "[B]oardEdit [C]hainEdit");
  commands->PutsXY(1, 2, "[D]irEdit [E]mail [G]-FileEdit");
  commands->PutsXY(1, 3, "[I]nit Voting Data  [J] ConfEdit");
  commands->PutsXY(1, 4, "Sysop[L]og  Read [M]ail  [N]etLog");
  commands->PutsXY(1, 5, "[P]ending Net Data [/] Net Callout");
  commands->PutsXY(1, 6, "[R]ead all email [S]ystem Status");
  commands->PutsXY(1, 7, "[U]serEdit [Y]-Log [Z]-Log");
}

static void DrawStatus(CursesWindow* status) {
  status->PutsXY(2, 1, "Today:");
  status->PutsXY(2, 2, "Calls: XXXXX Minutes: XXXXX");
  status->PutsXY(2, 3, "M: XXX L: XXX E: XXX F: XXX FW: XXX");
  status->PutsXY(2, 4, "Totals:");
  status->PutsXY(2, 5, "Users: XXXXX Calls: XXXXX");
  status->PutsXY(2, 6, "Last User:");
  status->PutsXY(2, 7, "XXXXXXXXXXXXXXXXXXXXXXXXXX");
}

void ControlCenter::Initialize() {
  // Initialization steps that have to happen before we
  // have a functional WFC system. This also supposes that
  // application()->InitializeBBS has been called.
  out->Cls(ACS_CKBOARD);
  const int logs_y_padding = 1;
  const int logs_start = 11;
  const int logs_length = out->window()->GetMaxY() - logs_start - logs_y_padding;
  log_.reset(new WfcLog(logs_length - 2));
  commands_.reset(CreateBoxedWindow("Commands", 9, 38, 1, 1));
  status_.reset(CreateBoxedWindow("Status", 9, 39, 1, 40));
  logs_.reset(CreateBoxedWindow("Logs", logs_length, 78, 11, 1));

  DrawCommands(commands_.get());
  DrawStatus(status_.get());
  vector<HelpItem> help_items0 = { { "?", "All Commands" },};
  vector<HelpItem> help_items1 = { {"Q", "Quit" } };
  out->footer()->ShowHelpItems(0, help_items0);
  out->footer()->ShowHelpItems(1, help_items1);

  session()->ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  session()->ResetEffectiveSl();
  session()->usernum = 1;

  fwaiting = session()->user()->GetNumMailWaiting();
  application()->SetWfcStatus(1);
}

void ControlCenter::Run() {
  Initialize();
  bool need_refresh = false;
  for (bool done = false; !done;) {
    if (need_refresh) {
      // refresh the window since we call endwin before invoking bbs code.
      RefreshAll();
      need_refresh = false;
    }

    wtimeout(commands_->window(), 500);
    int key = commands_->GetChar();
    if (key == ERR) {
      // we have a timeout. process other events
      need_refresh = false;
      UpdateLog();
      continue;
    }
    need_refresh = true;
    application()->SetWfcStatus(2);
    // Call endwin since we'll be out of curses IO
    endwin();
    switch (toupper(key)) {
    case 'B': wfc_command(INST_LOC_BOARDEDIT, boardedit, cleanup_net); log_->Put("Ran BoardEdit"); break;
    case 'C': wfc_command(INST_LOC_CHAINEDIT, chainedit); log_->Put("Ran ChainEdit"); break;
    case 'D': wfc_command(INST_LOC_DIREDIT, dlboardedit); log_->Put("Ran DirEdit"); break;
    case 'E': wfc_command(INST_LOC_EMAIL, send_email_f, cleanup_net); break;
    case 'G': wfc_command(INST_LOC_GFILEEDIT, gfileedit); break;
    case 'H': wfc_command(INST_LOC_EVENTEDIT, eventedit); break;
    case 'I': wfc_command(INST_LOC_VOTEEDIT, ivotes); break;
    case 'J': wfc_command(INST_LOC_CONFEDIT, edit_confs); break;
    case 'L': wfc_command(INST_LOC_WFC, view_sysop_log_f); break;
    case 'M': wfc_command(INST_LOC_EMAIL, read_mail_f, cleanup_net); break;
    case 'N': wfc_command(INST_LOC_WFC, []() { print_local_file("net.log"); }); break;
    case 'P': wfc_command(INST_LOC_WFC, print_pending_list); break;
    case 'R': wfc_command(INST_LOC_MAILR, mailr); break;
    case 'S': wfc_command(INST_LOC_WFC, prstatus, getkey_f); break;
    case 'U': wfc_command(INST_LOC_UEDIT, []() { uedit(1, UEDIT_NONE); } ); break;
    case 'Y': wfc_command(INST_LOC_WFC, view_yesterday_sysop_log_f); break;
    case 'Z': wfc_command(INST_LOC_WFC, zlog, getkey_f); break;
    case 'Q': done=true; break;
    case ' ': log_->Put("Not Implemented Yet"); break; 
    }
    TouchAll();
  }
}

void ControlCenter::TouchAll() {
  out->window()->TouchWin();
  commands_->TouchWin();
  status_->TouchWin();
  logs_->TouchWin();
}

void ControlCenter::RefreshAll() {
  out->window()->Refresh();
  commands_->Refresh();
  status_->Refresh();
  logs_->Refresh();
}

void ControlCenter::UpdateLog() {
  if (!log_->dirty()) {
    return;
  }

  vector<string> lines;
  if (!log_->Get(lines)) {
    return;
  }

  int start = 1;
  const int width = logs_->GetMaxX() - 4;
  for (const auto& line : lines) {
    logs_->PutsXY(1, start, line);
    logs_->PutsXY(1 + line.size(), start, string(width - line.size(), ' '));
    start++;
  }
}

}
}

// Legacy WFC

#if !defined ( __unix__ )

// Local Functions
void DisplayWFCScreen(const char *pszBuffer);
static char* pszScreenBuffer = nullptr;

static int inst_num;

void wfc_cls() {
  if (application()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    bout.ResetColors();
    session()->localIO()->LocalCls();
    session()->wfc_status = 0;
    session()->localIO()->SetCursor(LocalIO::cursorNormal);
  }
}

void wfc_init() {
  session()->localIO()->SetCursor(LocalIO::cursorNormal);              // add 4.31 Build3
  if (application()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    session()->wfc_status = 0;
    inst_num = 1;
  }
}

void wfc_update() {
  if (!application()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  instancerec ir;
  WUser u;

  get_inst_info(inst_num, &ir);
  application()->users()->ReadUserNoCache(&u, ir.user);
  session()->localIO()->LocalXYAPrintf(57, 18, 15, "%-3d", inst_num);
  if (ir.flags & INST_FLAGS_ONLINE) {
    session()->localIO()->LocalXYAPrintf(42, 19, 14, "%-25.25s", u.GetUserNameAndNumber(ir.user));
  } else {
    session()->localIO()->LocalXYAPrintf(42, 19, 14, "%-25.25s", "Nobody");
  }

  string activity_string;
  make_inst_str(inst_num, &activity_string, INST_FORMAT_WFC);
  session()->localIO()->LocalXYAPrintf(42, 20, 14, "%-25.25s", activity_string.c_str());
  if (num_instances() > 1) {
    do {
      ++inst_num;
      if (inst_num > num_instances()) {
        inst_num = 1;
      }
    } while (inst_num == application()->GetInstanceNumber());
  }
}

void wfc_screen() {
  char szBuffer[ 255 ];
  instancerec ir;
  WUser u;
  static double wfc_time = 0, poll_time = 0;

  if (!application()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  int nNumNewMessages = check_new_mail(session()->usernum);
  std::unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
  if (session()->wfc_status == 0) {
    session()->localIO()->SetCursor(LocalIO::cursorNone);
    session()->localIO()->LocalCls();
    if (pszScreenBuffer == nullptr) {
      pszScreenBuffer = new char[4000];
      File wfcFile(syscfg.datadir, WFC_DAT);
      if (!wfcFile.Open(File::modeBinary | File::modeReadOnly)) {
        wfc_cls();
        std::clog << wfcFile.full_pathname() << " NOT FOUND." << std::endl;
        application()->AbortBBS();
      }
      wfcFile.Read(pszScreenBuffer, 4000);
    }
    DisplayWFCScreen(pszScreenBuffer);
    sprintf(szBuffer, "Activity and Statistics of %s Node %d", syscfg.systemname, application()->GetInstanceNumber());
    session()->localIO()->LocalXYAPrintf(1 + ((76 - strlen(szBuffer)) / 2), 4, 15, szBuffer);
    session()->localIO()->LocalXYAPrintf(8, 1, 14, fulldate());
    std::string osVersion = wwiv::os::os_version_string();
    session()->localIO()->LocalXYAPrintf(40, 1, 3, "OS: ");
    session()->localIO()->LocalXYAPrintf(44, 1, 14, osVersion.c_str());
    session()->localIO()->LocalXYAPrintf(21, 6, 14, "%d", pStatus->GetNumCallsToday());
    session()->localIO()->LocalXYAPrintf(21, 7, 14, "%d", fwaiting);
    if (nNumNewMessages) {
      session()->localIO()->LocalXYAPrintf(29, 7 , 3, "New:");
      session()->localIO()->LocalXYAPrintf(34, 7 , 12, "%d", nNumNewMessages);
    }
    session()->localIO()->LocalXYAPrintf(21, 8, 14, "%d", pStatus->GetNumUploadsToday());
    session()->localIO()->LocalXYAPrintf(21, 9, 14, "%d", pStatus->GetNumMessagesPostedToday());
    session()->localIO()->LocalXYAPrintf(21, 10, 14, "%d", pStatus->GetNumLocalPosts());
    session()->localIO()->LocalXYAPrintf(21, 11, 14, "%d", pStatus->GetNumEmailSentToday());
    session()->localIO()->LocalXYAPrintf(21, 12, 14, "%d", pStatus->GetNumFeedbackSentToday());
    session()->localIO()->LocalXYAPrintf(21, 13, 14, "%d Mins (%.1f%%)", pStatus->GetMinutesActiveToday(),
                                            100.0 * static_cast<float>(pStatus->GetMinutesActiveToday()) / 1440.0);
    session()->localIO()->LocalXYAPrintf(58, 6, 14, "%s%s", wwiv_version, beta_version);

    session()->localIO()->LocalXYAPrintf(58, 7, 14, "%d", pStatus->GetNetworkVersion());
    session()->localIO()->LocalXYAPrintf(58, 8, 14, "%d", pStatus->GetNumUsers());
    session()->localIO()->LocalXYAPrintf(58, 9, 14, "%ld", pStatus->GetCallerNumber());
    if (pStatus->GetDays()) {
      session()->localIO()->LocalXYAPrintf(58, 10, 14, "%.2f", static_cast<float>(pStatus->GetCallerNumber()) /
                                              static_cast<float>(pStatus->GetDays()));
    } else {
      session()->localIO()->LocalXYAPrintf(58, 10, 14, "N/A");
    }
    session()->localIO()->LocalXYAPrintf(58, 11, 14, sysop2() ? "Available    " : "Not Available");
    session()->localIO()->LocalXYAPrintf(58, 12, 14, "Local %code", (syscfgovr.primaryport) ? 'M' : 'N');
    session()->localIO()->LocalXYAPrintf(58, 13, 14, "Waiting For Command");

    get_inst_info(application()->GetInstanceNumber(), &ir);
    if (ir.user < syscfg.maxusers && ir.user > 0) {
      application()->users()->ReadUserNoCache(&u, ir.user);
      session()->localIO()->LocalXYAPrintf(33, 16, 14, "%-20.20s", u.GetUserNameAndNumber(ir.user));
    } else {
      session()->localIO()->LocalXYAPrintf(33, 16, 14, "%-20.20s", "Nobody");
    }

    session()->wfc_status = 1;
    wfc_update();
    poll_time = wfc_time = timer();
  } else {
    if ((timer() - wfc_time < session()->screen_saver_time) ||
        (session()->screen_saver_time == 0)) {
      session()->localIO()->LocalXYAPrintf(28, 1, 14, times());
      session()->localIO()->LocalXYAPrintf(58, 11, 14, sysop2() ? "Available    " : "Not Available");
      if (timer() - poll_time > 10) {
        wfc_update();
        poll_time = timer();
      }
    } else {
      if ((timer() - poll_time > 10) || session()->wfc_status == 1) {
        session()->wfc_status = 2;
        session()->localIO()->LocalCls();
        session()->localIO()->LocalXYAPrintf(random_number(38),
                                                random_number(24),
                                                random_number(14) + 1,
                                                "WWIV Screen Saver - Press Any Key For WWIV");
        wfc_time = timer() - session()->screen_saver_time - 1;
        poll_time = timer();
      }
    }
  }
}

void DisplayWFCScreen(const char *pszScreenBuffer) {
  session()->localIO()->LocalWriteScreenBuffer(pszScreenBuffer);
}

#endif // __unix__
