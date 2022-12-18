/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/instmsg.h"
#include "common/output.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/filenames.h"
#include "sdk/files/dirs.h"
#include "sdk/names.h"
#include "sdk/subxtr.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local function prototypes
// TODO: Stop calling thi sand call the one in sdk/instance
std::string GetInstanceActivityString(instancerec& ir) {
  if (ir.loc >= INST_LOC_CH1 && ir.loc <= INST_LOC_CH10) {
    return std::string("WWIV Chatroom");
  }
  switch (ir.loc) {
  case INST_LOC_DOWN:
    return std::string("Offline");
  case INST_LOC_INIT:
    return std::string("Initializing BBS");
  case INST_LOC_EMAIL:
    return std::string("Sending Email");
  case INST_LOC_MAIN:
    return std::string("Main Menu");
  case INST_LOC_XFER:
    if (so() && ir.subloc < a()->dirs().size()) {
      return fmt::format("Transfer Area: Dir : {}", stripcolors(a()->dirs()[ir.subloc].name));
    }
    return "Transfer Area";
  case INST_LOC_CHAINS:
    if (ir.subloc > 0 && ir.subloc <= a()->chains->chains().size()) {
      const auto& c = a()->chains->at(ir.subloc - 1);
      return fmt::format("Chains: Door: {}", stripcolors(c.description));
    }
    return "Chains";
  case INST_LOC_NET:
    return std::string("Network Transmission");
  case INST_LOC_GFILES:
    return std::string("GFiles");
  case INST_LOC_BEGINDAY:
    return std::string("Running BeginDay");
  case INST_LOC_EVENT:
    return std::string("Executing Event");
  case INST_LOC_CHAT:
    return std::string("Normal Chat");
  case INST_LOC_CHAT2:
    return std::string("SplitScreen Chat");
  case INST_LOC_CHATROOM:
    return std::string("ChatRoom");
  case INST_LOC_LOGON:
    return std::string("Logging On");
  case INST_LOC_LOGOFF:
    return std::string("Logging off");
  case INST_LOC_FSED:
    return ("FullScreen Editor");
  case INST_LOC_UEDIT:
    return ("In UEDIT");
  case INST_LOC_CHAINEDIT:
    return ("In CHAINEDIT");
  case INST_LOC_BOARDEDIT:
    return ("In BOARDEDIT");
  case INST_LOC_DIREDIT:
    return ("In DIREDIT");
  case INST_LOC_GFILEEDIT:
    return ("In GFILEEDIT");
  case INST_LOC_CONFEDIT:
    return ("In CONFEDIT");
  case INST_LOC_DOS:
    return ("In DOS");
  case INST_LOC_DEFAULTS:
    return ("In Defaults");
  case INST_LOC_REBOOT:
    return ("Rebooting");
  case INST_LOC_RELOAD:
    return ("Reloading BBS data");
  case INST_LOC_VOTE:
    return ("Voting");
  case INST_LOC_BANK:
    return ("In TimeBank");
  case INST_LOC_AMSG:
    return ("AutoMessage");
  case INST_LOC_SUBS:
    if (so() && ir.subloc < a()->subs().subs().size()) {
      return fmt::format("Reading Messages: (Sub: {})",
                         stripcolors(a()->subs().sub(ir.subloc).name));
    }
    return "Reading Messages";
  case INST_LOC_CHUSER:
    return std::string("Changing User");
  case INST_LOC_TEDIT:
    return std::string("In TEDIT");
  case INST_LOC_MAILR:
    return std::string("Reading All Mail");
  case INST_LOC_RESETQSCAN:
    return std::string("Resetting QSCAN pointers");
  case INST_LOC_VOTEEDIT:
    return std::string("In VOTEEDIT");
  case INST_LOC_VOTEPRINT:
    return std::string("Printing Voting Data");
  case INST_LOC_RESETF:
    return std::string("Resetting NAMES.LST");
  case INST_LOC_FEEDBACK:
    return std::string("Leaving Feedback");
  case INST_LOC_KILLEMAIL:
    return std::string("Viewing Old Email");
  case INST_LOC_POST:
    if (so() && ir.subloc < a()->subs().subs().size()) {
      return fmt::format("Posting a Message (Sub: {})",
                         stripcolors(a()->subs().sub(ir.subloc).name));
    }
    return "Posting a Message";
  case INST_LOC_NEWUSER:
    return std::string("Registering a Newuser");
  case INST_LOC_RMAIL:
    return std::string("Reading Email");
  case INST_LOC_DOWNLOAD:
    return std::string("Downloading");
  case INST_LOC_UPLOAD:
    return std::string("Uploading");
  case INST_LOC_BIXFER:
    return std::string("Bi-directional Transfer");
  case INST_LOC_NETLIST:
    return std::string("Listing Net Info");
  case INST_LOC_TERM:
    return std::string("In a terminal program");
  case INST_LOC_GETUSER:
    return std::string("Getting User ID");
  case INST_LOC_WFC:
    return std::string("Waiting for Call");
  case INST_LOC_QWK:
    return std::string("In QWK");
  }
  return "Unknown BBS Location!";
}

/*
 * Returns a std::string (in out) like:
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
  auto ir = a()->instances().at(instance_num);

  switch (format) {
  case INST_FORMAT_WFC:
    return GetInstanceActivityString(ir.ir());
  case INST_FORMAT_OLD:
    // Not used anymore.
    return fmt::sprintf("|#1Instance %-3d: |#2", instance_num);
  case INST_FORMAT_LIST: {
    std::string user_name{"(Nobody)"};
    if (ir.user_number() < a()->config()->max_users() && ir.user_number() > 0) {
      if (ir.online()) {
        user_name = a()->names()->UserName(ir.user_number());
      } else {
        user_name = StrCat("Last: ", a()->names()->UserName(ir.user_number()));
      }
    }
    const auto as = GetInstanceActivityString(ir.ir());
    return fmt::sprintf("|#5%-4d |#2%-35.35s |#1%-37.37s", instance_num, user_name, as);
  }
  default:
    return fmt::format("** INVALID INSTANCE FORMAT PASSED [{}] **", format);
  }
}

void multi_instance() {
  bout.nl();
  const auto num = num_instances();
  if (num < 1) {
    bout.outstr("|#6Couldn't find instance data file.\r\n");
    return;
  }

  bout.printf("|#5Node |#1%-35.35s |#2%-37.37s\r\n", "User Name", "Activity");
  bout.print("==== {} {}\r\n", std::string(35, '='), std::string(37, '='));

  for (int inst = 1; inst <= num; inst++) {
    bout.pl(make_inst_str(inst, INST_FORMAT_LIST));
  }
}

int find_instance_by_loc(int loc, int subloc) {
  if (loc == INST_LOC_FSED) {
    return 0;
  }

  const auto instances = a()->instances().all();
  if (instances.size() <= 1) {
    return 0;
  }
  for (const auto& in : instances) {
    if (in.loc_code() == loc && in.subloc_code() == subloc && in.node_number() > 0 &&
        in.node_number() != a()->sess().instance_number()) {
      return in.node_number();
    }
  }
  return 0;
}
