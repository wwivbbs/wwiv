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
#include "bbs/wfc.h"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "bbs/bbsovl1.h"
#include "bbs/chnedit.h"
#include "bbs/bbs.h"
#include "bbs/events.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/local_io_curses.h"
#include "bbs/multinst.h"
#include "bbs/netsup.h"
#include "bbs/printfile.h"
#include "bbs/uedit.h"
#include "bbs/voteedit.h"
#include "bbs/wconstants.h"
#include "sdk/status.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "core/inifile.h"
#include "localui/curses_io.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using wwiv::core::FilePath;
using wwiv::os::random_number;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Legacy WFC

// Local Functions
static char* pszScreenBuffer = nullptr;

static int inst_num;

void wfc_cls() {
  if (session()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    bout.ResetColors();
    session()->localIO()->LocalCls();
    session()->wfc_status = 0;
    session()->localIO()->SetCursor(LocalIO::cursorNormal);
  }
}

void wfc_init() {
  session()->localIO()->SetCursor(LocalIO::cursorNormal);              // add 4.31 Build3
  if (session()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    session()->wfc_status = 0;
    inst_num = 1;
  }
}

void wfc_update() {
  if (!session()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  instancerec ir;
  User u;

  get_inst_info(inst_num, &ir);
  session()->users()->ReadUserNoCache(&u, ir.user);
  session()->localIO()->LocalXYAPrintf(57, 18, 15, "%-3d", inst_num);
  if (ir.flags & INST_FLAGS_ONLINE) {
    const string unn = session()->names()->UserName(ir.user);
    session()->localIO()->LocalXYAPrintf(42, 19, 14, "%-25.25s", unn.c_str());
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
    } while (inst_num == session()->instance_number());
  }
}

void wfc_screen() {
  instancerec ir;
  User u;
  static long wfc_time = 0, poll_time = 0;

  if (!session()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  int nNumNewMessages = check_new_mail(session()->usernum);
  std::unique_ptr<WStatus> pStatus(session()->status_manager()->GetStatus());
  if (session()->wfc_status == 0) {
    session()->localIO()->SetCursor(LocalIO::cursorNone);
    session()->localIO()->LocalCls();
    if (pszScreenBuffer == nullptr) {
      pszScreenBuffer = new char[4000];
      File wfcFile(session()->config()->datadir(), WFC_DAT);
      if (!wfcFile.Open(File::modeBinary | File::modeReadOnly)) {
        wfc_cls();
        std::clog << wfcFile.full_pathname() << " NOT FOUND." << std::endl;
        session()->AbortBBS();
      }
      wfcFile.Read(pszScreenBuffer, 4000);
    }
    session()->localIO()->LocalWriteScreenBuffer(pszScreenBuffer);
    const string title = StringPrintf("Activity and Statistics of %s Node %d", syscfg.systemname, session()->instance_number());
    session()->localIO()->LocalXYAPrintf(1 + ((76 - title.size()) / 2), 4, 15, title.c_str());
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

    get_inst_info(session()->instance_number(), &ir);
    if (ir.user < syscfg.maxusers && ir.user > 0) {
      const string unn = session()->names()->UserName(ir.user);
      session()->localIO()->LocalXYAPrintf(33, 16, 14, "%-20.20s", unn.c_str());
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
        session()->localIO()->LocalXYAPrintf(
            random_number(38), random_number(24), random_number(14) + 1,
            "WWIV Screen Saver - Press Any Key For WWIV");
        wfc_time = timer() - session()->screen_saver_time - 1;
        poll_time = timer();
      }
    }
  }
}

