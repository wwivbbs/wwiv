/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "bbs/multinst.h"

#include <string>
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/instmsg.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/subxtr.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local funciton prototypes

string GetInstanceActivityString(instancerec &ir) {
  if (ir.loc >= INST_LOC_CH1 && ir.loc <= INST_LOC_CH10) {
    return string("WWIV Chatroom");
  } 
  switch (ir.loc) {
    case INST_LOC_DOWN: return string("Offline");
    case INST_LOC_INIT: return string("Initializing BBS");
    case INST_LOC_EMAIL: return string("Sending Email");
    case INST_LOC_MAIN: return string("Main Menu");
    case INST_LOC_XFER:
      if (so() && ir.subloc < a()->directories.size()) {
        return fmt::format("Transfer Area: Dir : {}",
                           stripcolors(a()->directories[ir.subloc].name));
      }
      return "Transfer Area";
    case INST_LOC_CHAINS:
      if (ir.subloc > 0 && ir.subloc <= a()->chains->chains().size()) {
        const auto& c = a()->chains->at(ir.subloc - 1);
        return fmt::format("Chains: Door: {}", stripcolors(c.description));
      }
      return "Chains";
    case INST_LOC_NET: return string("Network Transmission");
    case INST_LOC_GFILES: return string("GFiles");
    case INST_LOC_BEGINDAY: return string("Running BeginDay");
    case INST_LOC_EVENT: return string("Executing Event");
    case INST_LOC_CHAT: return string("Normal Chat");
    case INST_LOC_CHAT2: return string("SplitScreen Chat");
    case INST_LOC_CHATROOM: return string("ChatRoom");
    case INST_LOC_LOGON: return string("Logging On");
    case INST_LOC_LOGOFF: return string("Logging off");
    case INST_LOC_FSED: return ("FullScreen Editor");
    case INST_LOC_UEDIT: return ("In UEDIT");
    case INST_LOC_CHAINEDIT: return ("In CHAINEDIT");
    case INST_LOC_BOARDEDIT: return ("In BOARDEDIT");
    case INST_LOC_DIREDIT: return ("In DIREDIT");
    case INST_LOC_GFILEEDIT: return ("In GFILEEDIT");
    case INST_LOC_CONFEDIT: return ("In CONFEDIT");
    case INST_LOC_DOS: return ("In DOS");
    case INST_LOC_DEFAULTS: return ("In Defaults");
    case INST_LOC_REBOOT: return ("Rebooting");
    case INST_LOC_RELOAD: return ("Reloading BBS data");
    case INST_LOC_VOTE: return ("Voting");
    case INST_LOC_BANK: return ("In TimeBank");
    case INST_LOC_AMSG: return ("AutoMessage");
    case INST_LOC_SUBS:
      if (so() && ir.subloc < a()->subs().subs().size()) {
        return fmt::format("Reading Messages: (Sub: {})", stripcolors(a()->subs().sub(ir.subloc).name));
      }
      return "Reading Messages";
    case INST_LOC_CHUSER: return string("Changing User");
    case INST_LOC_TEDIT: return string("In TEDIT");
    case INST_LOC_MAILR: return string("Reading All Mail");
    case INST_LOC_RESETQSCAN: return string("Resetting QSCAN pointers");
    case INST_LOC_VOTEEDIT: return string("In VOTEEDIT");
    case INST_LOC_VOTEPRINT: return string("Printing Voting Data");
    case INST_LOC_RESETF: return string("Resetting NAMES.LST");
    case INST_LOC_FEEDBACK: return string("Leaving Feedback");
    case INST_LOC_KILLEMAIL: return string("Viewing Old Email");
    case INST_LOC_POST:
      if (so() && ir.subloc < a()->subs().subs().size()) {
        return fmt::format("Posting a Message (Sub: {})", stripcolors(a()->subs().sub(ir.subloc).name));
      }
      return "Posting a Message";
    case INST_LOC_NEWUSER: return string("Registering a Newuser");
    case INST_LOC_RMAIL: return string("Reading Email");
    case INST_LOC_DOWNLOAD: return string("Downloading");
    case INST_LOC_UPLOAD: return string("Uploading");
    case INST_LOC_BIXFER: return string("Bi-directional Transfer");
    case INST_LOC_NETLIST: return string("Listing Net Info");
    case INST_LOC_TERM: return string("In a terminal program");
    case INST_LOC_GETUSER: return string("Getting User ID");
    case INST_LOC_WFC: return string("Waiting for Call");
    case INST_LOC_QWK: return string("In QWK");
  }
  return "Unknown BBS Location!";
}

/*
 * Returns a string (in out) like:
 *
 * Instance   1: Offline
 *     LastUser: Sysop #1
 *
 * or
 *
 * Instance  22: Network transmission
 *     CurrUser: Sysop #1
 */
std::string make_inst_str(int instance_num, int format) {
  const auto s = fmt::sprintf("|#1Instance %-3d: |#2", instance_num);

  instancerec ir{};
  get_inst_info(instance_num, &ir);

  const auto activity_string = GetInstanceActivityString(ir);

  switch (format) {
  case INST_FORMAT_WFC:
    return activity_string;
  case INST_FORMAT_OLD:
    // Not used anymore.
    return s;
  case INST_FORMAT_LIST: {
    std::string userName;
    if (ir.user < a()->config()->max_users() && ir.user > 0) {
      User user;
      a()->users()->readuser(&user, ir.user);
      if (ir.flags & INST_FLAGS_ONLINE) {
        userName = a()->names()->UserName(ir.user);
      } else {
        userName = StrCat("Last: ", a()->names()->UserName(ir.user));
      }
    } else {
      userName = "(Nobody)";
    }
    return fmt::sprintf("|#5%-4d |#2%-35.35s |#1%-37.37s", instance_num, userName, activity_string);
  }
  }
  return fmt::format("** INVALID INSTANCE FORMAT PASSED [{}] **", format);
}

void multi_instance() {
  bout.nl();
  auto num = num_instances();
  if (num < 1) {
    bout << "|#6Couldn't find instance data file.\r\n";
    return;
  }

  bout.bprintf("|#5Node |#1%-35.35s |#2%-37.37s\r\n", "User Name", "Activity");
  bout << "==== " << string(35, '=') << " " << string(37, '=') << "\r\n";

  for (int inst = 1; inst <= num; inst++) {
    bout << make_inst_str(inst, INST_FORMAT_LIST) << "\r\n";
  }
}

int inst_ok(int loc, int subloc) {
  if (loc == INST_LOC_FSED) {
    return 0;
  }

  int num = 0;
  File f(PathFilePath(a()->config()->datadir(), INSTANCE_DAT));
  if (!f.Open(File::modeReadOnly | File::modeBinary)) {
    return 0;
  }
  auto num_instances = static_cast<int>(f.length() / sizeof(instancerec));
  f.Close();
  for (int i = 1; i < num_instances; i++) {
    if (f.Open(File::modeReadOnly | File::modeBinary)) {
      f.Seek(i * sizeof(instancerec), File::Whence::begin);
      instancerec in{};
      f.Read(&in, sizeof(instancerec));
      f.Close();
      if (in.loc == loc &&
          in.subloc == subloc &&
          in.number != a()->instance_number()) {
        num = in.number;
      }
    }
  }
  return num;
}
