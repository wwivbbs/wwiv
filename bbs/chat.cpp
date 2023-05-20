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

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/instmsg.h"
#include "bbs/multinst.h"
#include "common/input.h"
#include "common/output.h"
#include "common/pause.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include <string>

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::strings;
using namespace wwiv::sdk;

struct ch_type {
  char name[60];
  int sl;
  char ar;
  char sex;
  uint8_t min_age;
  uint8_t max_age;
};

struct ch_action {
  int r;
  char aword[12];
  char toprint[80];
  char toperson[80];
  char toall[80];
  char singular[80];
};

static int g_nChatOpSecLvl;
static int g_nNumActions;
constexpr int MAX_NUM_ACT = 100;
static ch_action* actions[MAX_NUM_ACT];
static ch_type channels[11];

int rip_words(int start_pos, const char* cmsg, char* wd, int size, char lookfor);
int f_action(int start_pos, int end_pos, char* aword);
int main_loop(const char* message, char* from_message, char* color_string, char* messageSent,
              bool& bActionMode, int loc, int num_actions);
std::vector<int> who_online(int loc);
void intro(int loc);
void ch_direct(const std::string& message, int loc, char* color_string, int node);
void ch_whisper(const std::string&, char* color_string, int node);
int wusrinst(char* n);
void secure_ch(int ch);
void cleanup_chat();
void page_user(int loc);
void moving(bool bOnline, int loc);
void load_actions(IniFile* pIniFile);
void add_action(ch_action act);
void free_actions();
void exec_action(const char* message, char* color_string, int loc, int nact);
void action_help(int num);
void ga(const char* message, char* color_string, int loc, int type);
void list_channels();
int change_channels(int loc);
bool check_ch(int ch);
void load_channels(IniFile& pIniFile);
int userinst(char* user);

static int grabname(const std::string& orig, int channel) {
  auto node = 0;
  User u;
  if (orig.empty() || orig.front() == ' ') {
    return 0;
  }

  const auto space = orig.find(' ', 1);
  const auto message = orig.substr(0, space);

  if (auto n = to_number<int>(message)) {
    if (n < 1 || n > num_instances()) {
      bout.printf("%s%d|#1]\r\n", "|#1[|#9There is no user on instance ", n);
      return 0;
    }
    if (const auto ir = a()->instances().at(n); ir.online() && (!ir.invisible() || so())) {
      if (channel && ir.loc_code() != channel) {
        bout.outstr("|#1[|#9That user is not in this chat channel|#1]\r\n");
        return 0;
      }
      return n;
    }
    bout.printf("%s%d|#1]\r\n", "|#1[|#9There is no user on instance ", n);
    return 0;
  }
  {
    node = 0;
    const auto name = ToStringUpperCase(message);
    for (auto i = 1; i <= num_instances(); i++) {
      if (auto ir = a()->instances().at(i); ir.online() && (!ir.invisible() || so())) {
        if (channel && (ir.loc_code() != channel)) {
          continue;
        }
        a()->users()->readuser(&u, ir.user_number());
        if (name == u.name()) {
          node = i;
          break;
        }
      }
    }
  }
  if (!node) {
    if (channel) {
      bout.outstr("|#1[|#9That user is not in this chat channel|#1]\r\n");
    } else {
      bout.outstr("|#1[|#9Specified user is not online|#1]\r\n");
    }
  }
  return node;
}

static std::string StripName(const std::string& in) {
  if (in.empty() || in.front() == ' ') {
    return in;
  }

  const auto space = in.find(' ', 1);
  return in.substr(space);
}

// Sets color_string string for current node
static void get_colors(char* color_string, const IniFile* pIniFile) {
  const auto s = pIniFile->value<std::string>(StrCat("C", a()->sess().instance_number()));
  strcpy(color_string, s.c_str());
}

void chat_room() {
  char szColorString[15];
  bool bShowPrompt;

  g_nNumActions = -1;
  g_nChatOpSecLvl = 256;

  char szMessageSent[80], szFromMessage[50];
  strcpy(szMessageSent, "|#1[|#9Message Sent|#1]\r\n");
  strcpy(szFromMessage, "|#9From %.12s|#1: %s%s");
  IniFile ini(FilePath(a()->bbspath(), CHAT_INI), "CHAT");
  if (ini.IsOpen()) {
    g_nChatOpSecLvl = ini.value<int>("CHATOP_SL");
    bShowPrompt = ini.value<bool>("CH_PROMPT");
    load_channels(ini);
    load_actions(&ini);
    get_colors(szColorString, &ini);
    ini.Close();
  } else {
    bout.outstr("|#6[CHAT] SECTION MISSING IN CHAT.INI - ALERT THE SYSOP IMMEDIATELY!\r\n");
    bout.pausescr();
    return;
  }
  cleanup_chat();
  bout.outstr("\r\n|#2Welcome to the WWIV Chatroom\n\r\n");
  auto loc = 0;
  if (bShowPrompt) {
    while (!loc) {
      bout.nl();
      bout.outstr("|#1Select a chat channel to enter:\r\n");
      loc = change_channels(-1);
      if (loc == -1) {
        return;
      }
    }
  } else {
    if (a()->user()->restrict_iichat() || !check_ch(1)) {
      bout.outstr("\r\n|#6You may not access inter-instance chat facilities.\r\n");
      bout.pausescr();
      return;
    }
    loc = INST_LOC_CH1;
    write_inst(loc, 0, INST_FLAGS_NONE);
    moving(true, loc);
    intro(loc);
    bout.nl();
  }

  TempDisablePause disable_pause(bout);
  a()->sess().chatline(false);
  a()->sess().in_chatroom(true);
  auto oiia = setiia(std::chrono::milliseconds(500));

  bool bActionMode = true;
  while (!a()->sess().hangup()) {
    setiia(std::chrono::milliseconds(500));
    a()->CheckForHangup();
    if (inst_msg_waiting()) {
      process_inst_msgs(); 
    }
    bout.ansic(1);
    bout.outstr(szColorString);
    a()->tleft(true);
    a()->sess().chatline(false);
    auto message = bin.input_text("", false, 255);
    if (message.empty()) {
      intro(loc);
    } else {
      int c = main_loop(message.c_str(), szFromMessage, szColorString, szMessageSent, bActionMode, loc,
                        g_nNumActions);
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
  a()->sess().in_chatroom(false);
}

int rip_words(int start_pos, const char* message, char* wd, int size, char lookfor) {
  unsigned int nPos;
  int nSpacePos = -1;

  for (nPos = start_pos; nPos <= strlen(message); nPos++) {
    if (nSpacePos == -1 && (message[nPos] == ' ')) {
      continue;
    }
    if (message[nPos] != lookfor) {
      nSpacePos++;
      if (nSpacePos > size) {
        break;
      }
      wd[nSpacePos] = message[nPos];
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

int f_action(int start_pos, int end_pos, char* aword) {
  int test = ((end_pos - start_pos) / 2) + start_pos;
  if (!((end_pos - start_pos) / 2)) {
    test++;
    if (iequals(aword, actions[test]->aword)) {
      return test;
    }
    if (iequals(aword, actions[test - 1]->aword)) {
      return test - 1;
    } else {
      return -1;
    }
  }
  if (StringCompareIgnoreCase(aword, actions[test]->aword) < 0) {
    end_pos = test;
  } else if (StringCompareIgnoreCase(aword, actions[test]->aword) > 0) {
    start_pos = test;
  } else {
    return test;
  }
  if (start_pos != end_pos) {
    test = f_action(start_pos, end_pos, aword);
  }
  return test;
}

// Sends out a raw_message to everyone in channel LOC
static void out_msg(const std::string& message, int loc) {
  for (auto i = 1; i <= num_instances(); i++) {
    auto ir = a()->instances().at(i);
    if (ir.loc_code() == loc && i != a()->sess().instance_number()) {
      send_inst_str(i, message);
    }
  }
}

// Determines if a word is an action, if so, executes it
bool check_action(const char* message, char* color_string, int loc) {
  char s[12];

  unsigned int x = rip_words(0, message, s, 12, ' ');
  if (iequals("GA", s)) {
    ga(message + x, color_string, loc, 0);
    return true;
  }
  if (iequals("GA's", s)) {
    ga(message + x, color_string, loc, 1);
    return true;
  }
  int p = f_action(0, g_nNumActions, s);
  if (p != -1) {
    if (strlen(message) <= x) {
      exec_action("\0", color_string, loc, p);
    } else {
      exec_action(message + x, color_string, loc, p);
    }
    return true;
  }
  return false;
}

int main_loop(const char* raw_message, char* from_message, char* color_string, char* messageSent,
              bool& bActionMode, int loc, int num_actions) {
  User u;

  bool bActionHandled = true;
  if (bActionMode) {
    bActionHandled = !check_action(raw_message, color_string, loc);
  }
  if (iequals(raw_message, "/w")) {
    bActionHandled = 0;
    multi_instance();
    bout.nl();
  } else if (iequals(raw_message, "list")) {
    bout.nl();
    for (int i2 = 0; i2 <= num_actions; i2++) {
      bout.printf("%-16.16s", actions[i2]->aword);
    }
    bout.nl();
    bActionHandled = 0;
  } else if (iequals(raw_message, "/q") || iequals(raw_message, "x")) {
    bActionHandled = 0;
    bout.outstr("\r\n|#2Exiting Chatroom\r\n");
    return 0;
  } else if (iequals(raw_message, "/a")) {
    bActionHandled = 0;
    if (bActionMode) {
      bout.outstr("|#1[|#9Action mode disabled|#1]\r\n");
      bActionMode = false;
    } else {
      bout.outstr("|#1[|#9Action mode enabled|#1]\r\n");
      bActionMode = true;
    }
  } else if (iequals(raw_message, "/s")) {
    bActionHandled = 0;
    secure_ch(loc);
  } else if (iequals(raw_message, "/u")) {
    bActionHandled = 0;
    const auto fn = fmt::format("CHANNEL.{}", (loc + 1 - INST_LOC_CH1));
    if (File::Exists(fn)) {
      File::Remove(fn);
      const auto m = fmt::format("\r\n|#1[|#9{} has unsecured the channel|#1]", a()->user()->name());
      out_msg(m, loc);
      bout.outstr("|#1[|#9Channel Unsecured|#1]\r\n");
    } else {
      bout.outstr("|#1[|#9Channel not secured!|#1]\r\n");
    }
  } else if (iequals(raw_message, "/p")) {
    bActionHandled = 0;
    page_user(loc);
  } else if (iequals(raw_message, "/c")) {
    int nChannel = change_channels(loc);
    loc = nChannel;
    bActionHandled = 0;
  } else if (iequals(raw_message, "?") || iequals(raw_message, "/?")) {
    bActionHandled = 0;
    bout.print_help_file(CHAT_NOEXT);
  } else if (bActionHandled && raw_message[0] == '>') {
    bActionHandled = 0;
    int nUserNum = grabname(raw_message + 1, loc);
    if (nUserNum) {
      auto message = StripName(raw_message);
      ch_direct(message, loc, color_string, nUserNum);
    }
  } else if (bActionHandled && raw_message[0] == '/') {
    int nUserNum = grabname(raw_message + 1, 0);
    if (nUserNum) {
      auto message = StripName(raw_message);
      ch_whisper(message, color_string, nUserNum);
    }
    bActionHandled = 0;
  } else {
    if (bActionHandled) {
      bout.outstr(messageSent);
    }
    if (!raw_message[0]) {
      return loc;
    }
  }
  if (bActionHandled) {
    const auto t = fmt::sprintf(from_message, a()->user()->name(), color_string, raw_message);
    out_msg(t, loc);
  }
  return loc;
}

// Fills an array with information of who's online.
std::vector<int> who_online(int loc) {
  std::vector<int> r{};
  for (auto i = 1; i <= wwiv::stl::size_int(a()->instances()); i++) {
    const auto ir = a()->instances().at(i);
    if (!ir.invisible() || so()) {
      if (ir.loc_code() == loc && i != a()->sess().instance_number()) {
        r.emplace_back(i);
      }
    }
  }
  return r;
}

// Displays which channel the user is in, who's in the channel with them,
// whether or not the channel is secured, and tells the user how to obtain
// help
void intro(int loc) {

  bout.print("|#7You are in {} with: \r\n", channels[loc - INST_LOC_CH1 + 1].name);
  auto users = who_online(loc);
  if (!users.empty()) {
    auto first = true;
    for (const auto& usernum : users) {
      User u;
      a()->users()->readuser(&u, usernum);
      if (!first) {
        bout.outstr("|#7and ");
      }
      bout.print("|#1{} ", u.name());
      first = false;
    }
  } else {
    bout.outstr("|#7You are the only one here.\r\n");
  }
  const auto fn = fmt::format("CHANNEL.{}", (loc + 1 - INST_LOC_CH1));
  if (loc != INST_LOC_CH1 && File::Exists(fn)) {
    bout.outstr("|#7This channel is |#1secured|#7.\r\n");
  }
  bout.outstr("|#7Type ? for help.\r\n");
}

// This function is called when a > sign is encountered at the beginning of
//   a line, it's used for directing messages

void ch_direct(const std::string& message, int loc, char* color_string, int node) {
  if (message.empty()) {
    bout.outstr("|#1[|#9Message required after using a / or > command.|#1]\r\n");
    return;
  }

  auto ir = a()->instances().at(node);
  if (ir.loc_code() == loc) {
    User u;
    a()->users()->readuser(&u, ir.user_number());
    const auto s = fmt::sprintf("|#9From %.12s|#6 [to %s]|#1: %s%s", a()->user()->name(),
                                u.name(), color_string, message);
    for (auto i = 1; i <= num_instances(); i++) {
      ir = a()->instances().at(i);
      if (ir.loc_code() == loc && i != a()->sess().instance_number()) {
        send_inst_str(i, s);
      }
    }
    bout.print("|#1[|#9Message directed to {}|#1\r\n", u.name());
  } else {
    bout.outstr(message);
    bout.nl();
  }
}

// This function is called when a / sign is encountered at the beginning of
//   a raw_message, used for whispering
void ch_whisper(const std::string& message, char* color_string, int node) {
  if (message.empty()) {
    bout.outstr("|#1[|#9Message required after using a / or > command.|#1]\r\n");
    return;
  }
  if (!node) {
    return;
  }

  auto text = message;
  const auto ir = a()->instances().at(node);
  if (ir.in_channel()) {
    text = fmt::sprintf("|#9From %.12s|#6 [WHISPERED]|#2|#1:%s%s", a()->user()->name(),
                        color_string, message);
  }
  send_inst_str(node, text);
  if (auto ou = a()->users()->readuser(ir.user_number())) {
    bout.print("|#1[|#9Message sent only to {}|#1]\r\n", ou.value().name());
  }
}

// This function determines whether or not user N is online

int wusrinst(char* n) {

  for (auto i = 0; i <= num_instances(); i++) {
    auto ir = a()->instances().at(i);
    if (ir.online()) {
      if (auto u = a()->users()->readuser(ir.user_number())) {
        if (iequals(u.value().name(), n)) {
          return i;
        }
      }
    }
  }
  return 0;
}

// Secures a channel

void secure_ch(int ch) {
  if (ch == INST_LOC_CH1) {
    bout.outstr("|#1[|#9Cannot secure channel 1|#1]\r\n");
    return;
  }
  const auto fn = fmt::format("CHANNEL.{}", (ch + 1 - INST_LOC_CH1));
  if (File::Exists(fn)) {
    bout.outstr("|#1[|#9Channel already secured!|#1]\r\n");
  } else {
    {
      File file(fn);
      file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeText);
      file.Write(a()->user()->name());
    }
    bout.outstr("|#1[|#9Channel Secured|#1]\r\n");
    const auto msg = fmt::format("\r\n|#1[|#9{} has secured the channel|#1]", a()->user()->name());
    out_msg(msg, ch);
  }
}

// Eliminates unnecessary lockfiles

void cleanup_chat() {
  for (auto x = INST_LOC_CH2; x <= INST_LOC_CH10; x++) {
    const auto fn = fmt::format("CHANNEL.%d", (x + 1 - INST_LOC_CH1));
    if (File::Exists(fn) && who_online(x).empty()) {
      File::Remove(fn);
    }
  }
}

// Pages a user

void page_user(int loc) {
  int i = 0;

  loc = loc + 1 - INST_LOC_CH1;
  bout.nl();
  multi_instance();
  bout.nl();
  while ((i < 1 || i > num_instances()) && !a()->sess().hangup()) {
    a()->CheckForHangup();
    bout.print("|#2Which instance would you like to page? (1-{}, Q): ", num_instances());
    auto s = bin.input(2);
    if (!s.empty() && s.front() == 'Q') {
      return;
    }
    i = to_number<int>(s);
  }
  if (i == a()->sess().instance_number()) {
    bout.outstr("|#1[|#9Cannot page the instance you are on|#1]\r\n");
    return;
  }
  const auto ir = a()->instances().at(i);
  if (!ir.online() || (!so() && ir.invisible())) {
    bout.print("|#1[|#9There is no user on instance {}|#1]\r\n", i);
    return;
  }
  if (!ir.available() && !so()) {
    bout.outstr("|#1[|#9That user is not available for chat!|#1]");
    return;
  }
  const auto s = fmt::format(
      "{} is paging you from Chatroom channel {}.  Type /C from the MAIN MENU to enter the "
      "Chatroom.",
      a()->user()->name(), loc);
  send_inst_str(i, s);
  bout.outstr("|#1[|#9Page Sent|#1]\r\n");
}

// Announces when a user has left a channel

void moving(bool bOnline, int loc) {
  if (is_chat_invis()) {
    return;
  }
  const auto s = fmt::format("|#6{} {}", a()->user()->name(),
          (bOnline ? "is on the air." : "has signed off."));
  out_msg(s, loc);
}


// Loads the actions into memory

void load_actions(IniFile* pIniFile) {
  int to_read = pIniFile->value<int>("NUM_ACTIONS");
  if (!to_read) {
    return;
  }
  for (int cn = 1; cn <= to_read; cn++) {
    ch_action act;
    memset(&act, 0, sizeof(ch_action));
    for (int ca = 0; ca <= 5; ca++) {
      char rstr[10];
      sprintf(rstr, "%d%c", cn, 65 + ca);
      std::string s = pIniFile->value<std::string>(rstr);
      const char* ini_value = s.c_str();
      switch (ca) {
      case 0:
        act.r = to_number<int>((ini_value != nullptr) ? ini_value : "0");
        break;
      case 1:
        strcpy(act.aword, (ini_value != nullptr) ? ini_value : "");
        break;
      case 2:
        strcpy(act.toprint, (ini_value != nullptr) ? ini_value : "");
        break;
      case 3:
        strcpy(act.toperson, (ini_value != nullptr) ? ini_value : "");
        break;
      case 4:
        strcpy(act.toall, (ini_value != nullptr) ? ini_value : "");
        break;
      case 5:
        strcpy(act.singular, (ini_value != nullptr) ? ini_value : "");
        break;
      default:
        // TODO Should an error be displayed here?
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
  ch_action* addact = static_cast<ch_action*>(calloc(sizeof(ch_action) + 1, 1));
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

// "Executes" an action

void exec_action(const char* message, char* color_string, int loc, int nact) {
  char tmsg[150], final[170];

  bool ok = (strlen(message) == 0) ? false : true;
  if (iequals(message, "?")) {
    action_help(nact);
    return;
  }

  int p = 0;
  if (ok) {
    p = grabname(message, loc);
    if (!p) {
      return;
    }
  }
  if (ok) {
    sprintf(tmsg, actions[nact]->toperson, a()->user()->GetName());
  } else if (actions[nact]->r) {
    bout.outstr("This action requires a recipient.\r\n");
    return;
  } else {
    sprintf(tmsg, actions[nact]->singular, a()->user()->GetName());
  }
  bout.pl(actions[nact]->toprint);
  sprintf(final, "%s%s", color_string, tmsg);
  if (!ok) {
    out_msg(final, loc);
  } else {
    send_inst_str(p, final);
    const auto oa = a()->users()->readuser(a()->instances().at(p).user_number());
    if (!oa) {
      return;
    }
    sprintf(tmsg, actions[nact]->toall, a()->user()->GetName(), oa.value().GetName());
    sprintf(final, "%s%s", color_string, tmsg);
    for (int c = 1; c <= num_instances(); c++) {
      const auto ir = a()->instances().at(c);
      if (ir.loc_code() == loc && c != a()->sess().instance_number() && c != p) {
        send_inst_str(c, final);
      }
    }
  }
}

// Displays help on an action
void action_help(int num) {
  char buffer[150];
  char ac[13], rec[18];

  strcpy(ac, "|#6[USER]|#1");
  strcpy(rec, "|#6[RECIPIENT]|#1");
  bout.print("\r\n|#5Word:|#1 {}\r\n", actions[num]->aword);
  bout.print("|#5To user:|#1 {}\r\n", actions[num]->toprint);
  sprintf(buffer, actions[num]->toperson, ac);
  bout.print("|#5To recipient:|#1 {}\r\n", buffer);
  sprintf(buffer, actions[num]->toall, ac, rec);
  bout.print("|#5To everyone else:|#1 {}\r\n", buffer);
  if (actions[num]->r == 1) {
    bout.outstr("|#5This action requires a recipient.\n\r\n");
    return;
  }
  sprintf(buffer, actions[num]->singular, ac);
  bout.print("|#5Used singular:|#1 {}\r\n", buffer);
  bout.outstr("|#5This action does not require a recipient.\n\r\n");
}

// Executes a GA command

void ga(const char* message, char* color_string, int loc, int type) {
  if (!strlen(message) || message[0] == '\0') {
    bout.outstr("|#1[|#9A message is required after the GA command|#1]\r\n");
    return;
  }
  char buffer[500];
  sprintf(buffer, "%s%s%s %s", color_string, a()->user()->GetName(), (type ? "'s" : ""), message);
  bout.outstr("|#1[|#9Generic Action Sent|#1]\r\n");
  out_msg(buffer, loc);
}

// Lists the chat channels
void list_channels() {
  int tl = 0, secure[11], check[11];
  char s[12];

  for (auto i = 1; i <= 10; i++) {
    sprintf(s, "CHANNEL.%d", i);
    secure[i] = (File::Exists(s)) ? 1 : 0;
    check[i] = 0;
  }

  for (auto i1 = 1; i1 <= num_instances(); i1++) {
    const auto ir = a()->instances().at(i1);
    if (!ir.invisible() || so()) {
      if (ir.in_channel()) {
        check[ir.loc_code() - INST_LOC_CH1 + 1] = 1;
      }
    }
  }

  for (tl = 1; tl <= 10; tl++) {
    bout.print("|#1{} |#7-|#9 {}\r\n", tl, channels[tl].name);
    if (!check[tl]) {
      continue;
    }
    auto users = who_online(tl + INST_LOC_CH1 - 1);
    if (users.empty()) {
      continue;
    }
    if (tl == 10) {
      bout.outchr(SPACE);
    }
    bout.outstr("    |#9Users in channel: ");
    auto first = true;
    for (const auto& usernum : users) {
      if (auto u = a()->users()->readuser(usernum)) {
        if (!first) {
          bout.outstr("|#7and ");
        }
        bout.print("|#1{} ", u.value().name());
        first = false;
      }
    }
    bout.outstr(" ");
    if (secure[tl]) {
      bout.outstr("|#6[SECURED]");
    }
    bout.nl();
  }
}

// Calls list_channels() then prompts for a channel to change to.
int change_channels(int loc) {
  auto ch_ok = 0, temploc = 0;
  char szMessage[80];

  cleanup_chat();
  bout.nl();
  list_channels();
  bout.nl();
  while ((temploc < 1 || temploc > 10) && !a()->sess().hangup()) {
    a()->CheckForHangup();
    bout.outstr("|#1Enter a channel number, 1 to 10, Q to quit: ");
    bin.input(szMessage, 2);
    if (to_upper_case_char(szMessage[0]) == 'Q') {
      return loc;
    }
    temploc = to_number<int>(szMessage);
  }
  if (check_ch(temploc)) {
    sprintf(szMessage, "CHANNEL.%d", temploc);
    if (!File::Exists(szMessage)) {
      ch_ok = 1;
    } else {
      if (a()->user()->sl() >= g_nChatOpSecLvl || so()) {
        bout.outstr("|#9This channel is secured.  Are you |#1SURE|#9 you wish to enter? ");
        if (bin.yesno()) {
          ch_ok = 1;
        }
      } else {
        bout.outstr("|#1Channel is secured.  You cannot enter it.\r\n");
      }
    }
    if (ch_ok) {
      if (loc != -1) {
        moving(false, loc);
      }
      loc = temploc + (INST_LOC_CH1 - 1);
      write_inst(loc, 0, INST_FLAGS_NONE);
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
  uint16_t c_ar;
  char szMessage[80];

  if (static_cast<int>(a()->user()->sl()) < channels[ch].sl && !so()) {
    bout.print("\r\n|#9A security level of |#1{}"
               "|#9 is required to access this channel.\r\n",
               channels[ch].sl);
    bout.print("|#9Your security level is |#1{}|#9.\r\n", a()->user()->sl());
    return false;
  }
  if (channels[ch].ar != '0') {
    c_ar = static_cast<uint16_t>(1 << (channels[ch].ar - 65));
  } else {
    c_ar = 0;
  }
  if (c_ar && !a()->user()->has_ar(c_ar)) {
    sprintf(szMessage, "\r\n|#9The \"|#1%c|#9\" AR is required to access this chat channel.\r\n",
            channels[ch].ar);
    bout.outstr(szMessage);
    return false;
  }
  char gender = channels[ch].sex;
  if (gender != 65 && a()->user()->gender() != gender &&
      a()->user()->sl() < g_nChatOpSecLvl) {
    if (gender == 77) {
      bout.outstr("\r\n|#9Only |#1males|#9 are allowed in this channel.\r\n");
    } else if (gender == 70) {
      bout.outstr("\r\n|#9Only |#1females|#9 are allowed in this channel.\r\n");
    } else {
      bout.outstr("\r\n|#6The sysop has configured this channel improperly!\r\n");
    }
    return false;
  }
  if (a()->user()->age() < channels[ch].min_age && a()->user()->sl() < g_nChatOpSecLvl) {
    bout.print("\r\n|#9You must be |#1{}|#9 or older to enter this channel.\r\n", channels[ch].min_age);
    return false;
  }
  if (a()->user()->age() > channels[ch].max_age && a()->user()->sl() < g_nChatOpSecLvl) {
    bout.print("\r\n|#9You must be |#1{}|#9 or younger to enter this channel.\r\n", channels[ch].max_age);
    return false;
  }
  return true;
}

// Loads channel information into memory
void load_channels(IniFile& ini) {
  char buffer[6];

  for (int cn = 1; cn <= 10; cn++) {
    for (int ca = 0; ca <= 5; ca++) {
      sprintf(buffer, "CH%d%c", cn, 65 + ca);
      switch (ca) {
      case 0:
        to_char_array(channels[cn].name, ini.value<std::string>(buffer));
        break;
      case 1:
        channels[cn].sl = ini.value<int>(buffer);
        break;
      case 2: {
        std::string temp = ini.value<std::string>(buffer);
        if (temp.empty() || temp.front() == '0') {
          channels[cn].ar = 0;
        } else {
          channels[cn].ar = temp.front();
        }
      } break;
      case 3: {
        std::string temp = ini.value<std::string>(buffer);
        if (!temp.empty()) {
          channels[cn].sex = temp.front();
        }
      } break;
      case 4:
        channels[cn].min_age = ini.value<uint8_t>(buffer);
        break;
      case 5:
        channels[cn].max_age = ini.value<uint8_t>(buffer);
        break;
      }
    }
  }
}

// Determines the node number a user is on
int userinst(char* user) {
  if (strlen(user) == 0) {
    return 0;
  }
  auto p = wusrinst(user);
  if (p) {
    auto ir = a()->instances().at(p);
    if (!ir.invisible() || so()) {
      return p;
    }
  }
  p = to_number<int>(user);
  if (p > 0 && p <= num_instances()) {
    const auto ir = a()->instances().at(p);
    if (!ir.online() && (so() || !ir.invisible())) {
      return p;
    }
  }
  return 0;
}
