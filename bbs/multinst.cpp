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
#include <string>
#include "bbs/multinst.h"

#include "bbs/wwiv.h"
#include "bbs/instmsg.h"
#include "bbs/wconstants.h"

#include "core/strings.h"

using std::string;
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
      if (so() && ir.subloc < session()->num_dirs) {
        string temp = StringPrintf("Dir : %s", stripcolors(directories[ ir.subloc ].name));
        return StrCat("Transfer Area", temp);
      }
      return string("Transfer Area");
    case INST_LOC_CHAINS:
      if (ir.subloc > 0 && ir.subloc <= session()->GetNumberOfChains()) {
        string temp = StringPrintf("Door: %s", stripcolors(chains[ ir.subloc - 1 ].description));
        return StrCat("Chains", temp);
      }
      return string("Chains");
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
      if (so() && ir.subloc < session()->num_subs) {
        string temp = StringPrintf("(Sub: %s)", stripcolors(subboards[ ir.subloc ].name));
        return StrCat("Reading Messages", temp);
      }
      return string("Reading Messages");
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
      if (so() && ir.subloc < session()->num_subs) {
        string temp = StringPrintf(" (Sub: %s)", stripcolors(subboards[ir.subloc].name));
        return StrCat("Posting a Message", temp);
      }
      return string("Posting a Message");
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
  return string("Unknown BBS Location!");
}

/*
 * Builds a string (in pszOutInstanceString) like:
 *
 * Instance   1: Offline
 *     LastUser: Sysop #1
 *
 * or
 *
 * Instance  22: Network transmission
 *     CurrUser: Sysop #1
 */
void make_inst_str(int nInstanceNum, std::string *out, int nInstanceFormat) {
  const string s = StringPrintf("|#1Instance %-3d: |#2", nInstanceNum);

  instancerec ir;
  get_inst_info(nInstanceNum, &ir);

  const string activity_string = GetInstanceActivityString(ir);

  switch (nInstanceFormat) {
  case INST_FORMAT_WFC:
    out->assign(activity_string); // WFC addition
    break;
  case INST_FORMAT_OLD:
    // Not used anymore.
    out->assign(s);
    break;
  case INST_FORMAT_LIST: {
    std::string userName;
    if (ir.user < syscfg.maxusers && ir.user > 0) {
      WUser user;
      application()->users()->ReadUser(&user, ir.user);
      if (ir.flags & INST_FLAGS_ONLINE) {
        userName = user.GetUserNameAndNumber(ir.user);
      } else {
        userName = "Last: ";
        userName += user.GetUserNameAndNumber(ir.user);
      }
    } else {
      userName = "(Nobody)";
    }
    out->assign(StringPrintf("|#5%-4d |#2%-35.35s |#1%-37.37s", nInstanceNum, userName.c_str(), activity_string.c_str()));
  }
  break;
  default:
    out->assign(StringPrintf("** INVALID INSTANCE FORMAT PASSED [%d] **", nInstanceFormat));
    break;
  }
}

void multi_instance() {
  bout.nl();
  int nNumInstances = num_instances();
  if (nNumInstances < 1) {
    bout << "|#6Couldn't find instance data file.\r\n";
    return;
  }

  bout.bprintf("|#5Node |#1%-35.35s |#2%-37.37s\r\n", "User Name", "Activity");
  char s1[81], s2[81], s3[81];
  strcpy(s1, charstr(4, '='));
  strcpy(s2, charstr(35, '='));
  strcpy(s3, charstr(37, '='));
  bout.bprintf("|#7%-4.4s %-35.35s %-37.37s\r\n", s1, s2, s3);

  for (int nInstance = 1; nInstance <= nNumInstances; nInstance++) {
    string activity;
    make_inst_str(nInstance, &activity, INST_FORMAT_LIST);
    bout << activity;
    bout.nl();
  }
}

int inst_ok(int loc, int subloc) {
  instancerec instance_temp;

  if (loc == INST_LOC_FSED) {
    return 0;
  }

  int nInstNum = 0;
  File instFile(syscfg.datadir, INSTANCE_DAT);
  if (!instFile.Open(File::modeReadOnly | File::modeBinary)) {
    return 0;
  }
  int nNumInstances = static_cast<int>(instFile.GetLength() / sizeof(instancerec));
  instFile.Close();
  for (int nInstance = 1; nInstance < nNumInstances; nInstance++) {
    if (instFile.Open(File::modeReadOnly | File::modeBinary)) {
      instFile.Seek(nInstance * sizeof(instancerec), File::seekBegin);
      instFile.Read(&instance_temp, sizeof(instancerec));
      instFile.Close();
      if (instance_temp.loc == loc &&
          instance_temp.subloc == subloc &&
          instance_temp.number != application()->GetInstanceNumber()) {
        nInstNum = instance_temp.number;
      }
    }
  }
  return nInstNum;
}
