/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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

#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/input.h"
#include "core/inifile.h"
#include "bbs/keycodes.h"
#include "bbs/instmsg.h"
#include "bbs/multinst.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/uedit.h"
#include "bbs/wconstants.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"


using wwiv::core::IniFile;
using wwiv::core::FilePath;
using wwiv::strings::IsEqualsIgnoreCase;

static int g_nChatOpSecLvl;
static int g_nNumActions;
static ch_action *actions[MAX_NUM_ACT];
static ch_type channels[11];

int  rip_words(int nStartPos, char *cmsg, char *wd, int size, char lookfor);
int  f_action(int nStartPos, int nEndPos, char *pszAWord);
int  main_loop(char *pszMessage, char *pszFromMessage, char *pszColorString, char *pszMessageSent, bool &bActionMode,
               int loc, int g_nNumActions);
void who_online(int *nodes, int loc);
void intro(int loc);
void ch_direct(const char *pszMessage, int loc, char *pszColorString, int node, int nOffSet);
void ch_whisper(const char *pszMessage, char *pszColorString, int node, int nOffSet);
int  wusrinst(char *n);
void secure_ch(int ch);
void cleanup_chat();
void page_user(int loc);
void moving(bool bOnline, int loc);
void out_msg(const char *pszMessage, int loc);
void get_colors(char *pszColorString, IniFile *pIniFile);
void load_actions(IniFile *pIniFile);
void add_action(ch_action act);
void free_actions();
bool check_action(char *pszMessage, char *pszColorString, int loc);
void exec_action(const char *pszMessage, char *pszColorString, int loc, int nact);
void action_help(int num);
void ga(const char *pszMessage, char *pszColorString, int loc, int type);
void list_channels();
int  change_channels(int loc);
bool check_ch(int ch);
void load_channels(IniFile *pIniFile);
int  userinst(char *user);
int  grabname(const char *pszMessage, int ch);
bool usercomp(const char *st1, const char *st2);

using wwiv::bbs::TempDisablePause;

void chat_room() {
  int oiia = iia;
  char szColorString[15];
  bool bShowPrompt = false;

  g_nNumActions = -1;
  g_nChatOpSecLvl = 256;

  char szMessageSent[80], szFromMessage[50];
  strcpy(szMessageSent, "|#1[|#9Message Sent|#1]\r\n");
  strcpy(szFromMessage, "|#9From %.12s|#1: %s%s");
  IniFile iniFile(FilePath(session()->GetHomeDir(), CHAT_INI), "CHAT");
  if (iniFile.IsOpen()) {
    g_nChatOpSecLvl = iniFile.GetNumericValue("CHATOP_SL");
    bShowPrompt = iniFile.GetBooleanValue("CH_PROMPT");
    load_channels(&iniFile);
    load_actions(&iniFile);
    get_colors(szColorString, &iniFile);
    iniFile.Close();
  } else {
    bout << "|#6[CHAT] SECTION MISSING IN CHAT.INI - ALERT THE SYSOP IMMEDIATELY!\r\n";
    pausescr();
    return;
  }
  cleanup_chat();
  bout << "\r\n|#2Welcome to the WWIV Chatroom\n\r\n";
  int loc = 0;
  if (bShowPrompt) {
    while (!loc) {
      bout.nl();
      bout << "|#1Select a chat channel to enter:\r\n";
      loc = change_channels(-1);
    }
  } else {
    if (session()->user()->IsRestrictionMultiNodeChat() || !check_ch(1)) {
      bout << "\r\n|#6You may not access inter-instance chat facilities.\r\n";
      pausescr();
      return;
    }
    loc = INST_LOC_CH1;
    write_inst(static_cast< unsigned short >(loc), 0, INST_FLAGS_NONE);
    moving(true, loc);
    intro(loc);
    bout.nl();
  }

  TempDisablePause disable_pause;
  bChatLine = false;
  in_chatroom = true;
  setiia(9);
  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%sEXTCHAT", syscfg.datadir);

  bool bActionMode = true;
  while (!hangup) {
    CheckForHangup();
    if (inst_msg_waiting()) {
      process_inst_msgs();
    }
    bout << "|#1: " << szColorString;
    session()->localIO()->tleft(true);
    bChatLine = false;
    char szMessage[ 300 ];
    inputl(szMessage, 250);
    if (!szMessage[0]) {
      intro(loc);
    } else {
      int c = main_loop(szMessage, szFromMessage, szColorString, szMessageSent, bActionMode, loc, g_nNumActions);
      if (!c) {
        break;
      } else {
        loc = c;
      }
    }
  }
  setiia(oiia);
  moving(false, loc);
  free_actions();
  in_chatroom = false;
}

int rip_words(int nStartPos, char *pszMessage, char *wd, int size, char lookfor) {
  unsigned int nPos;
  int nSpacePos = -1;

  for (nPos = nStartPos; nPos <= strlen(pszMessage); nPos++) {
    if (nSpacePos == -1 && (pszMessage[nPos] == ' ')) {
      continue;
    }
    if (pszMessage[nPos] != lookfor) {
      nSpacePos++;
      if (nSpacePos > size) {
        break;
      }
      wd[nSpacePos] = pszMessage[nPos];
    } else {
      nSpacePos++;
      nPos++;
      wd[nSpacePos] = '\0';
      break;
    }
  }
  wd[nSpacePos] = '\0';
  return nPos;
}

int f_action(int nStartPos, int nEndPos, char *pszAWord) {
  int test = ((nEndPos - nStartPos) / 2) + nStartPos;
  if (!((nEndPos - nStartPos) / 2)) {
    test++;
    if (IsEqualsIgnoreCase(pszAWord, actions[test]->aword)) {
      return test;
    }
    if (IsEqualsIgnoreCase(pszAWord, actions[test - 1]->aword)) {
      return test - 1;
    } else {
      return -1;
    }
  }
  if (wwiv::strings::StringCompareIgnoreCase(pszAWord, actions[test]->aword) < 0) {
    nEndPos = test;
  } else if (wwiv::strings::StringCompareIgnoreCase(pszAWord, actions[test]->aword) > 0) {
    nStartPos = test;
  } else {
    return test;
  }
  if (nStartPos != nEndPos) {
    test = f_action(nStartPos, nEndPos, pszAWord);
  }
  return test;
}


int main_loop(char *pszMessage, char *pszFromMessage, char *pszColorString, char *pszMessageSent, bool &bActionMode,
              int loc, int num_actions) {
  char szText[300];
  WUser u;

  bool bActionHandled = true;
  if (bActionMode) {
    bActionHandled = !check_action(pszMessage, pszColorString, loc);
  }
  if (IsEqualsIgnoreCase(pszMessage, "/r")) {
    /* "Undocumented Feature" - the original alpha version of WMChat had a /r
    * command to look up a user's registry from inside chat.  I took this
    * out when I released the program, but am now putting it back in due to
    * popular request.  However, most systems don't have a registry
    * installed, so I couldn't just include the code.  If you do have a
    * registry look-up function, put #define REG_IS_INSTALLED in your
    * VARDEC.H file and rename the function to read_entry() */
#ifdef REG_IS_INSTALLED
    read_entry(1);
    bout.nl();
    bActionHandled = 0;
#endif
  } else if (IsEqualsIgnoreCase(pszMessage, "/w")) {
    bActionHandled = 0;
    multi_instance();
    bout.nl();
  } else if (IsEqualsIgnoreCase(pszMessage, "list")) {
    bout.nl();
    for (int i2 = 0; i2 <= num_actions; i2++) {
      bout.bprintf("%-16.16s", actions[i2]->aword);
    }
    bout.nl();
    bActionHandled = 0;
  } else if (IsEqualsIgnoreCase(pszMessage, "/q") ||
             IsEqualsIgnoreCase(pszMessage, "x")) {
    bActionHandled = 0;
    bout << "\r\n|#2Exiting Chatroom\r\n";
    return 0;
  } else if (IsEqualsIgnoreCase(pszMessage, "/a")) {
    bActionHandled = 0;
    if (bActionMode) {
      bout << "|#1[|#9Action mode disabled|#1]\r\n";
      bActionMode = false;
    } else {
      bout << "|#1[|#9Action mode enabled|#1]\r\n";
      bActionMode = true;
    }
  } else if (IsEqualsIgnoreCase(pszMessage, "/s")) {
    bActionHandled = 0;
    secure_ch(loc);
  } else if (IsEqualsIgnoreCase(pszMessage, "/u")) {
    bActionHandled = 0;
    char szFileName[ MAX_PATH ];
    sprintf(szFileName, "CHANNEL.%d", (loc + 1 - INST_LOC_CH1));
    if (File::Exists(szFileName)) {
      File::Remove(szFileName);
      sprintf(szFileName, "\r\n|#1[|#9%s has unsecured the channel|#1]", session()->user()->GetName());
      out_msg(szFileName, loc);
      bout << "|#1[|#9Channel Unsecured|#1]\r\n";
    } else {
      bout << "|#1[|#9Channel not secured!|#1]\r\n";
    }
  } else if (IsEqualsIgnoreCase(pszMessage, "/l") &&
             session()->user()->GetSl() >= g_nChatOpSecLvl) {
    bout << "\r\n|#9Username: ";
    input(szText, 30);
    bout.nl();
    int nTempUserNum = finduser1(szText);
    if (nTempUserNum > 0) {
      session()->users()->ReadUser(&u, nTempUserNum);
      print_data(nTempUserNum, &u, true, false);
    } else {
      bout << "|#6Unknown user.\r\n";
    }
    bActionHandled = 0;
  } else if (IsEqualsIgnoreCase(pszMessage, "/p")) {
    bActionHandled = 0;
    page_user(loc);
  } else if (IsEqualsIgnoreCase(pszMessage, "/c")) {
    int nChannel = change_channels(loc);
    loc = nChannel;
    bActionHandled = 0;
  } else if (IsEqualsIgnoreCase(pszMessage, "?") ||
             IsEqualsIgnoreCase(pszMessage, "/?")) {
    bActionHandled = 0;
    printfile(CHAT_NOEXT);
  } else if (bActionHandled && pszMessage[0] == '>') {
    bActionHandled = 0;
    int nUserNum = grabname(pszMessage + 1, loc);
    if (nUserNum) {
      ch_direct(pszMessage + 1, loc, pszColorString, nUserNum, pszMessage[1]);
    }
  } else if (bActionHandled && pszMessage[0] == '/') {
    int nUserNum = grabname(pszMessage + 1, 0);
    if (nUserNum) {
      ch_whisper(pszMessage + 1, pszColorString, nUserNum, pszMessage[1]);
    }
    bActionHandled = 0;
  } else {
    if (bActionHandled) {
      bout << pszMessageSent;
    }
    if (!pszMessage[0]) {
      return loc;
    }
  }
  if (bActionHandled) {
    sprintf(szText, pszFromMessage, session()->user()->GetName(), pszColorString, pszMessage);
    out_msg(szText, loc);
  }
  return loc;
}


// Fills an array with information of who's online.
void who_online(int *nodes, int loc) {
  instancerec ir;

  int c = 0;
  for (int i = 1; i <= num_instances(); i++) {
    get_inst_info(i, &ir);
    if ((!(ir.flags & INST_FLAGS_INVIS)) || so())
      if ((ir.loc == loc) && (i != session()->GetInstanceNumber())) {
        c++;
        nodes[c] = ir.user;
      }
  }
  nodes[0] = c;
}

// Displays which channel the user is in, who's in the channel with them,
// whether or not the channel is secured, and tells the user how to obtain
// help
void intro(int loc) {
  int nodes[20];

  bout << "|#7You are in " << channels[loc - INST_LOC_CH1 + 1].name << wwiv::endl;
  who_online(nodes, loc);
  if (nodes[0]) {
    for (int i = 1; i <= nodes[0]; i++) {
      WUser u;
      session()->users()->ReadUser(&u, nodes[i]);
      if (((nodes[0] - i) == 1) && (nodes[0] >= 2)) {
        bout << "|#1" << u.GetName() << " |#7and ";
      } else {
        bout << "|#1" << u.GetName() << (((nodes[0] > 1) && (i != nodes[0])) ? "|#7, " : " ");
      }
    }
  }
  if (nodes[0] == 1) {
    bout << "|#7is here with you.\r\n";
  } else if (nodes[0] > 1) {
    bout << "|#7are here with you.\r\n";
  } else {
    bout << "|#7You are the only one here.\r\n";
  }
  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "CHANNEL.%d", (loc + 1 - INST_LOC_CH1));
  if (loc != INST_LOC_CH1 && File::Exists(szFileName)) {
    bout << "|#7This channel is |#1secured|#7.\r\n";
  }
  bout << "|#7Type ? for help.\r\n";
}

// This function is called when a > sign is encountered at the beginning of
//   a line, it's used for directing messages

void ch_direct(const char *pszMessage, int loc, char *pszColorString, int node, int nOffSet) {
  if (strlen(pszMessage + nOffSet) == 0) {
    bout << "|#1[|#9Message required after using a / or > command.|#1]\r\n";
    return;
  }

  instancerec ir;
  get_inst_info(node, &ir);
  char szMessage[ 512 ];
  if (ir.loc == loc) {
    if (!strlen(pszMessage + nOffSet)) {
      bout << "|#1[|#9Message required after using a / or > command.|#1]\r\n";
      return;
    }
    WUser u;
    session()->users()->ReadUser(&u, ir.user);
    char szUserName[ 81 ];
    strcpy(szUserName, u.GetName());
    sprintf(szMessage, "|#9From %.12s|#6 [to %s]|#1: %s%s",
            session()->user()->GetName(), szUserName, pszColorString,
            pszMessage + nOffSet + 1);
    for (int i = 1; i <= num_instances(); i++) {
      get_inst_info(i, &ir);
      if (ir.loc == loc &&  i != session()->GetInstanceNumber()) {
        send_inst_str(i, szMessage);
      }
    }
    bout << "|#1[|#9Message directed to " << szUserName << "|#1\r\n";
  } else {
    bout << szMessage;
    bout.nl();
  }
}

// This function is called when a / sign is encountered at the beginning of
//   a message, used for whispering
void ch_whisper(const char *pszMessage, char *pszColorString, int node, int nOffSet) {
  if (strlen(pszMessage + nOffSet) == 0) {
    bout << "|#1[|#9Message required after using a / or > command.|#1]\r\n";
    return;
  }
  if (!node) {
    return;
  }
  instancerec ir;
  get_inst_info(node, &ir);

  char szText[ 512 ];
  if (ir.loc >= INST_LOC_CH1 && ir.loc <= INST_LOC_CH10) {
    sprintf(szText, "|#9From %.12s|#6 [WHISPERED]|#2|#1:%s%s", session()->user()->GetName(), pszColorString,
            pszMessage + nOffSet);
  } else {
    strcpy(szText, pszMessage + nOffSet);
  }
  send_inst_str(node, szText);
  WUser u;
  session()->users()->ReadUser(&u, ir.user);
  bout << "|#1[|#9Message sent only to " << u.GetName() << "|#1]\r\n";
}

// This function determines whether or not user N is online

int wusrinst(char *n) {
  instancerec ir;

  for (int i = 0; i <= num_instances(); i++) {
    get_inst_info(i, &ir);
    if (ir.flags & INST_FLAGS_ONLINE) {
      WUser user;
      session()->users()->ReadUser(&user, ir.user);
      if (IsEqualsIgnoreCase(user.GetName(), n)) {
        return i;
      }
    }
  }
  return 0;
}

// Secures a channel

void secure_ch(int ch) {
  char szFileName[ MAX_PATH ];
  if (ch == INST_LOC_CH1) {
    bout << "|#1[|#9Cannot secure channel 1|#1]\r\n";
    return;
  }
  sprintf(szFileName, "CHANNEL.%d", (ch + 1 - INST_LOC_CH1));
  if (File::Exists(szFileName)) {
    bout << "|#1[|#9Channel already secured!|#1]\r\n";
  } else {
    File file(szFileName);
    file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeText);
    file.Write(session()->user()->GetName(), strlen(session()->user()->GetName()));
    file.Close();
    bout << "|#1[|#9Channel Secured|#1]\r\n";
    sprintf(szFileName, "\r\n|#1[|#9%s has secured the channel|#1]", session()->user()->GetName());
    out_msg(szFileName, ch);
  }
}

// Eliminates unnecessary lockfiles

void cleanup_chat() {
  int nodes[10];
  char szFileName[MAX_PATH];

  for (int x = INST_LOC_CH2; x <= INST_LOC_CH10; x++) {
    sprintf(szFileName, "CHANNEL.%d", (x + 1 - INST_LOC_CH1));
    if (File::Exists(szFileName)) {
      who_online(nodes, x);
      if (!nodes[0]) {
        File::Remove(szFileName);
      }
    }
  }
}

// Pages a user

void page_user(int loc) {
  char s[200];
  instancerec ir;
  int i = 0;

  loc = loc + 1 - INST_LOC_CH1;
  bout.nl();
  multi_instance();
  bout.nl();
  while ((i < 1 || i > num_instances()) && !hangup) {
    CheckForHangup();
    bout << "|#2Which instance would you like to page? (1-" << num_instances() << ", Q): ";
    input(s, 2);
    if (s[0] == 'Q') {
      return;
    }
    i = atoi(s);
  }
  if (i == session()->GetInstanceNumber()) {
    bout << "|#1[|#9Cannot page the instance you are on|#1]\r\n";
    return;
  } else {
    get_inst_info(i, &ir);
    if ((!(ir.flags & INST_FLAGS_ONLINE)) ||
        ((ir.flags & INST_FLAGS_INVIS) && (!so()))) {
      bout << "|#1[|#9There is no user on instance " << i << " |#1]\r\n";
      return;
    }
    if ((!(ir.flags & INST_FLAGS_MSG_AVAIL)) && (!so())) {
      bout << "|#1[|#9That user is not available for chat!|#1]";
      return;
    }
    sprintf(s, "%s is paging you from Chatroom channel %d.  Type /C from the MAIN MENU to enter the Chatroom.",
            session()->user()->GetName(), loc);
    send_inst_str(i, s);
  }
  bout << "|#1[|#9Page Sent|#1]\r\n";
}

// Announces when a user has left a channel

void moving(bool bOnline, int loc) {
  char space[55];

  if (!chat_invis) {
    sprintf(space, "|#6%s %s", session()->user()->GetName(), (bOnline ? "is on the air." :
            "has signed off."));
    out_msg(space, loc);
  }
}

// Sends out a message to everyone in channel LOC
void out_msg(const char *pszMessage, int loc) {
  for (int i = 1; i <= num_instances(); i++) {
    instancerec ir;
    get_inst_info(i, &ir);
    if ((ir.loc == loc) && (i != session()->GetInstanceNumber())) {
      send_inst_str(i, pszMessage);
    }
  }
}

// Sets pszColorString string for current node

void get_colors(char *pszColorString, IniFile *pIniFile) {
  char szKey[10];

  sprintf(szKey, "C%u", session()->GetInstanceNumber());
  strcpy(pszColorString, pIniFile->GetValue(szKey));
}

// Loads the actions into memory

void load_actions(IniFile *pIniFile) {
  int to_read = pIniFile->GetNumericValue("NUM_ACTIONS");
  if (!to_read) {
    return;
  }
  for (int cn = 1; cn <= to_read; cn++) {
    ch_action act;
    memset(&act, 0, sizeof(ch_action));
    for (int ca = 0; ca <= 5; ca++) {
      char rstr[10];
      sprintf(rstr, "%d%c", cn, 65 + ca);
      const char* pszIniValue = pIniFile->GetValue(rstr);
      switch (ca) {
      case 0:
        act.r = atoi((pszIniValue != nullptr) ? pszIniValue : "0");
        break;
      case 1:
        strcpy(act.aword, (pszIniValue != nullptr) ? pszIniValue : "");
        break;
      case 2:
        strcpy(act.toprint, (pszIniValue != nullptr) ? pszIniValue : "");
        break;
      case 3:
        strcpy(act.toperson, (pszIniValue != nullptr) ? pszIniValue : "");
        break;
      case 4:
        strcpy(act.toall, (pszIniValue != nullptr) ? pszIniValue : "");
        break;
      case 5:
        strcpy(act.singular, (pszIniValue != nullptr) ? pszIniValue : "");
        break;
      default:
        //TODO Should an error be displayed here?
        break;
      }
    }
    add_action(act);
  }
}


// Used by load_actions(), adds an action into the array
void add_action(ch_action act) {
  if (g_nNumActions < 100) {
    g_nNumActions++;
  } else {
    return;
  }
  ch_action* addact = static_cast<ch_action *>(calloc(sizeof(ch_action) + 1, 1));
  WWIV_ASSERT(addact != nullptr);
  addact->r = act.r;
  strcpy(addact->aword, act.aword);
  strcpy(addact->toprint, act.toprint);
  strcpy(addact->toperson, act.toperson);
  strcpy(addact->toall, act.toall);
  strcpy(addact->singular, act.singular);
  actions[g_nNumActions] = addact;
}


// Removes the actions from memory
void free_actions() {
  for (int i = 0; i <= g_nNumActions; i++) {
    free(actions[i]);
  }
}


// Determines if a word is an action, if so, executes it
bool check_action(char *pszMessage, char *pszColorString, int loc) {
  char s[12];

  unsigned int x = rip_words(0, pszMessage, s, 12, ' ');
  if (IsEqualsIgnoreCase("GA", s)) {
    ga(pszMessage + x, pszColorString, loc, 0);
    return true;
  }
  if (IsEqualsIgnoreCase("GA's", s)) {
    ga(pszMessage + x, pszColorString, loc, 1);
    return true;
  }
  int p = f_action(0, g_nNumActions, s);
  if (p != -1) {
    if (strlen(pszMessage) <= x) {
      exec_action("\0", pszColorString, loc, p);
    } else {
      exec_action(pszMessage + x, pszColorString, loc, p);
    }
    return true;
  }
  return false;
}

// "Executes" an action

void exec_action(const char *pszMessage, char *pszColorString, int loc, int nact) {
  char tmsg[150], final[170];
  instancerec ir;
  WUser u;

  bool bOk = (strlen(pszMessage) == 0) ? false : true;
  if (IsEqualsIgnoreCase(pszMessage, "?")) {
    action_help(nact);
    return;
  }

  int p = 0;
  if (bOk) {
    p = grabname(pszMessage, loc);
    if (!p) {
      return;
    }
  }
  if (bOk) {
    get_inst_info(p, &ir);
    session()->users()->ReadUser(&u, ir.user);
    sprintf(tmsg, actions[nact]->toperson, session()->user()->GetName());
  } else if (actions[nact]->r) {
    bout << "This action requires a recipient.\r\n";
    return;
  } else {
    sprintf(tmsg, actions[nact]->singular, session()->user()->GetName());
  }
  bout << actions[nact]->toprint << wwiv::endl;
  sprintf(final, "%s%s", pszColorString, tmsg);
  if (!bOk) {
    out_msg(final, loc);
  } else {
    send_inst_str(p, final);
    sprintf(tmsg, actions[nact]->toall, session()->user()->GetName(), u.GetName());
    sprintf(final, "%s%s", pszColorString, tmsg);
    for (int c = 1; c <= num_instances(); c++) {
      get_inst_info(c, &ir);
      if ((ir.loc == loc) && (c != session()->GetInstanceNumber()) && (c != p)) {
        send_inst_str(c, final);
      }
    }
  }
}

// Displays help on an action
void action_help(int num) {
  char szBuffer[150];
  char ac[12], rec[17];

  strcpy(ac, "|#6[USER]|#1");
  strcpy(rec, "|#6[RECIPIENT]|#1");
  bout << "\r\n|#5Word:|#1 " << actions[num]->aword << wwiv::endl;
  bout << "|#5To user:|#1 " << actions[num]->toprint << wwiv::endl;
  sprintf(szBuffer, actions[num]->toperson, ac);
  bout << "|#5To recipient:|#1 " << szBuffer << wwiv::endl;
  sprintf(szBuffer, actions[num]->toall, ac, rec);
  bout << "|#5To everyone else:|#1 " << szBuffer << wwiv::endl;
  if (actions[num]->r == 1) {
    bout << "|#5This action requires a recipient.\n\r\n";
    return;
  }
  sprintf(szBuffer, actions[num]->singular, ac);
  bout << "|#5Used singular:|#1 " << szBuffer << wwiv::endl;
  bout << "|#5This action does not require a recipient.\n\r\n";
}

// Executes a GA command

void ga(const char *pszMessage, char *pszColorString, int loc, int type) {
  if (!strlen(pszMessage) || pszMessage[0] == '\0') {
    bout << "|#1[|#9A message is required after the GA command|#1]\r\n";
    return;
  }
  char szBuffer[500];
  sprintf(szBuffer, "%s%s%s %s", pszColorString, session()->user()->GetName(), (type ? "'s" : ""),
          pszMessage);
  bout << "|#1[|#9Generic Action Sent|#1]\r\n";
  out_msg(szBuffer, loc);
}

// Toggles user availability, called when ctrl-N is hit

void toggle_avail() {
  instancerec ir;
  char xl[81], cl[81], atr[81], cc;

  session()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
  get_inst_info(session()->GetInstanceNumber(), &ir);
  chat_avail = !chat_avail;

  bout << "\n\rYou are now ";
  bout << (chat_avail ? "available for chat.\n\r\n" : "not available for chat.\n\r\n");
  write_inst(ir.loc, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
  RestoreCurrentLine(cl, atr, xl, &cc);
}

// Toggles invisibility, called when ctrl-L is hit by a sysop

void toggle_invis() {
  instancerec ir;
  char xl[81], cl[81], atr[81], cc;

  session()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
  get_inst_info(session()->GetInstanceNumber(), &ir);
  chat_invis = !chat_invis;

  bout << "\r\n|#1You are now ";
  bout << (chat_invis ? "invisible.\n\r\n" : "visible.\n\r\n");
  write_inst(ir.loc, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
  RestoreCurrentLine(cl, atr, xl, &cc);
}

// Lists the chat channels
void list_channels() {
  int tl = 0, nodes[20], secure[10], check[10];
  char s[12];
  WUser u;
  instancerec ir;

  for (int i = 1; i <= 10; i++) {
    sprintf(s, "CHANNEL.%d", i);
    secure[i] = (File::Exists(s)) ? 1 : 0;
    check[i] = 0;
  }

  for (int i1 = 1; i1 <= num_instances(); i1++) {
    get_inst_info(i1, &ir);
    if ((!(ir.flags & INST_FLAGS_INVIS)) || so()) {
      if ((ir.loc >= INST_LOC_CH1) && (ir.loc <= INST_LOC_CH10)) {
        check[ir.loc - INST_LOC_CH1 + 1] = 1;
      }
    }
  }

  for (tl = 1; tl <= 10; tl++) {
    bout << "|#1" << tl << " |#7-|#9 " << channels[tl].name << wwiv::endl;
    if (check[tl]) {
      who_online(nodes, tl + INST_LOC_CH1 - 1);
      if (nodes[0]) {
        if (tl == 10) {
          bputch(SPACE);
        }
        bout << "    |#9Users in channel: ";
        for (int i2 = 1; i2 <= nodes[0]; i2++) {
          session()->users()->ReadUser(&u, nodes[i2]);
          if (((nodes[0] - i2) == 1) && (nodes[0] >= 2)) {
            bout << "|#1" << u.GetName() << " |#7and ";
          } else {
            bout << "|#1" << u.GetName() << (((nodes[0] > 1) && (i2 != nodes[0])) ? "|#7, " : " ");
          }
        }
        if (secure[tl]) {
          bout << "|#6[SECURED]";
        }
        bout.nl();
      }
    }
  }
}


// Calls list_channels() then prompts for a channel to change to.
int change_channels(int loc) {
  int ch_ok = 0, temploc = 0;
  char szMessage[80];

  cleanup_chat();
  bout.nl();
  list_channels();
  bout.nl();
  while ((temploc < 1 || temploc > 10) && !hangup) {
    CheckForHangup();
    bout << "|#1Enter a channel number, 1 to 10, Q to quit: ";
    input(szMessage, 2);
    if (wwiv::UpperCase<char>(szMessage[0]) == 'Q') {
      return loc;
    }
    temploc = atoi(szMessage);
  }
  if (check_ch(temploc)) {
    sprintf(szMessage, "CHANNEL.%d", temploc);
    if (!File::Exists(szMessage)) {
      ch_ok = 1;
    } else {
      if (session()->user()->GetSl() >= g_nChatOpSecLvl || so()) {
        bout << "|#9This channel is secured.  Are you |#1SURE|#9 you wish to enter? ";
        if (yesno()) {
          ch_ok = 1;
        }
      } else {
        bout << "|#1Channel is secured.  You cannot enter it.\r\n";
      }
    }
    if (ch_ok) {
      if (loc != -1) {
        moving(false, loc);
      }
      loc = temploc + (INST_LOC_CH1 - 1);
      write_inst(static_cast< unsigned short >(loc), 0, INST_FLAGS_NONE);
      moving(true, loc);
      bout.nl();
      intro(loc);
    }
  }
  return loc;
}

// Checks to see if user has requirements to enter a channel
//   (SL, AR, Sex, Age, etc)

bool check_ch(int ch) {
  unsigned short c_ar;
  char szMessage[80];

  if (session()->user()->GetSl() < channels[ch].sl && !so()) {
    bout << "\r\n|#9A security level of |#1" << channels[ch].sl <<
                       "|#9 is required to access this channel.\r\n";
    bout << "|#9Your security level is |#1" << session()->user()->GetSl() << "|#9.\r\n";
    return false;
  }
  if (channels[ch].ar != '0') {
    c_ar = static_cast< unsigned short >(1 << (channels[ch].ar - 65));
  } else {
    c_ar = 0;
  }
  if (c_ar && !session()->user()->HasArFlag(c_ar)) {
    sprintf(szMessage, "\r\n|#9The \"|#1%c|#9\" AR is required to access this chat channel.\r\n", channels[ch].ar);
    bout << szMessage;
    return false;
  }
  char gender = channels[ch].sex;
  if (gender != 65 && session()->user()->GetGender() != gender
      && session()->user()->GetSl() < g_nChatOpSecLvl) {
    if (gender == 77) {
      bout << "\r\n|#9Only |#1males|#9 are allowed in this channel.\r\n";
    } else if (gender == 70) {
      bout << "\r\n|#9Only |#1females|#9 are allowed in this channel.\r\n";
    } else {
      bout << "\r\n|#6The sysop has configured this channel improperly!\r\n";
    }
    return false;
  }
  if (session()->user()->GetAge() < channels[ch].min_age
      && session()->user()->GetSl() < g_nChatOpSecLvl) {
    bout << "\r\n|#9You must be |#1" << channels[ch].min_age << "|#9 or older to enter this channel.\r\n";
    return false;
  }
  if (session()->user()->GetAge() > channels[ch].max_age
      && session()->user()->GetSl() < g_nChatOpSecLvl) {
    bout << "\r\n|#9You must be |#1" << channels[ch].max_age << "|#9 or younger to enter this channel.\r\n";
    return false;
  }
  return true;
}

// Loads channel information into memory
void load_channels(IniFile *pIniFile) {
  char szBuffer[6], szTemp[10];

  for (int cn = 1; cn <= 10; cn++) {
    for (int ca = 0; ca <= 5; ca++) {
      sprintf(szBuffer, "CH%d%c", cn, 65 + ca);
      switch (ca) {
      case 0:
        strcpy(channels[cn].name, pIniFile->GetValue(szBuffer));
        break;
      case 1:
        channels[cn].sl = pIniFile->GetNumericValue(szBuffer);
        break;
      case 2:
        strcpy(szTemp, pIniFile->GetValue(szBuffer));
        if (szTemp[0] != '0') {
          channels[cn].ar = szTemp[0];
        } else {
          channels[cn].ar = 0;
        }
        break;
      case 3:
        strcpy(szTemp, pIniFile->GetValue(szBuffer));
        channels[cn].sex = szTemp[0];
        break;
      case 4:
        channels[cn].min_age = static_cast< char >(pIniFile->GetNumericValue(szBuffer));
        break;
      case 5:
        channels[cn].max_age = wwiv::strings::StringToChar(pIniFile->GetValue(szBuffer));
        break;
      }
    }
  }
}

// Determines the node number a user is on
int userinst(char *user) {
  instancerec ir;

  if (strlen(user) == 0) {
    return 0;
  }
  int p = wusrinst(user);
  if (p) {
    get_inst_info(p, &ir);
    if ((!(ir.flags & INST_FLAGS_INVIS)) || so()) {
      return p;
    }
  }
  p = atoi(user);
  if (p > 0 && p <= num_instances()) {
    get_inst_info(p, &ir);
    if (((!(ir.flags & INST_FLAGS_INVIS)) || so()) && (ir.flags & INST_FLAGS_ONLINE)) {
      return p;
    }
  }
  return 0;
}


int grabname(const char *pszMessage, int ch) {
  int c = 0, node = 0, dupe = 0, sp = 0;
  char name[41];
  WUser u;
  instancerec ir;

  if (pszMessage[0] == ' ') {
    return 0;
  }

  int n = atoi(pszMessage);
  if (n) {
    if (n < 1 || n > num_instances()) {
      char szBuffer[ 255 ];
      sprintf(szBuffer, "%s%d|#1]\r\n", "|#1[|#9There is no user on instance ", n);
      bout << szBuffer;
      return 0;
    }
    get_inst_info(n, &ir);
    if ((ir.flags & INST_FLAGS_ONLINE) &&
        ((!(ir.flags & INST_FLAGS_INVIS)) || so())) {
      if (ch && (ir.loc != ch)) {
        bout << "|#1[|#9That user is not in this chat channel|#1]\r\n";
        return 0;
      }
      while (!sp) {
        c++;
        if ((pszMessage[c] == ' ') || (pszMessage[c] == '\0')) {
          sp = 1;
        }
      }
      return n;
    }
    char szBuffer[ 255 ];
    sprintf(szBuffer, "%s%d|#1]\r\n", "|#1[|#9There is no user on instance ", n);
    bout << szBuffer;
    return 0;
  }
  while (!node && c < wwiv::strings::GetStringLength(pszMessage) && c < 40) {
    int x = 0;
    if (sp) {
      name[sp++] = ' ';
    }
    while (!x) {
      if ((pszMessage[c] == ' ') || (pszMessage[c] == '\0')) {
        x = 1;
      } else {
        name[sp++] = wwiv::UpperCase<char>(pszMessage[c]);
      }
      if (sp == 40) {
        break;
      }
      c++;
    }
    name[sp] = '\0';
    for (int i = 1; i <= num_instances(); i++) {
      get_inst_info(i, &ir);
      if ((ir.flags & INST_FLAGS_ONLINE) && ((!(ir.flags & INST_FLAGS_INVIS)) || so())) {
        if (ch && (ir.loc != ch)) {
          continue;
        }
        session()->users()->ReadUser(&u, ir.user);
        int t1 = strlen(name);
        int t2 = strlen(u.GetName());
        if (t1 > t2) {
          continue;
        }
        if (usercomp(name, u.GetName())) {
          if (node && (t1 != t2)) {
            dupe = 1;
            node = 0;
            break;
          } else {
            node = i;
            if (t1 == t2) {
              break;
            }
          }
        }
      }
    }
  }
  if (!node) {
    if (dupe) {
      bout << "|#1[|#9Name specified matched multiple users.|#1]\r\n";
    } else if (ch) {
      bout << "|#1[|#9That user is not in this chat channel|#1]\r\n";
    } else {
      bout << "|#1[|#9Specified user is not online|#1]\r\n";
    }
  }
  return node;
}

bool usercomp(const char *st1, const char *st2) {
  for (int i = 0; i < wwiv::strings::GetStringLength(st1); i++) {
    if (st1[i] != st2[i]) {
      return false;
    }
  }
  return true;
}

