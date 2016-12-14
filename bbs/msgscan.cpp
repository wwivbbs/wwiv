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
#include <algorithm>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include "bbs/bgetch.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/email.h"
#include "bbs/extract.h"
#include "bbs/full_screen.h"
#include "bbs/instmsg.h"
#include "bbs/input.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/msgbase1.h"
#include "bbs/quote.h"
#include "bbs/read_message.h"
#include "sdk/subxtr.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "bbs/pause.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/keycodes.h"
#include "bbs/workspace.h"
#include "sdk/status.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::endl;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::stl;
using namespace wwiv::strings;

static char s_szFindString[21];

static string GetScanReadPrompts(int nMessageNumber) {
  if (forcescansub) {
    if (nMessageNumber < a()->GetNumMessagesInCurrentMessageArea()) {
      return "|#1Press |#7[|#2ENTER|#7]|#1 to go to the next message...";
    } else {
      return "|#1Press |#7[|#2ENTER|#7]|#1 to continue...";
    }
  }

  string local_network_name = "Local";
  if (!a()->current_sub().nets.empty()) {
    local_network_name = a()->network_name();
  } else {
    set_net_num(0);
  }
  const string sub_name_prompt = StringPrintf("|#7[|#1%s|#7] [|#2%s|#7]",
      local_network_name.c_str(), a()->current_sub().name.c_str());
  return StringPrintf("%s |#7(|#1Read |#2%d |#1of |#2%d|#1|#7) : ",
        sub_name_prompt.c_str(), nMessageNumber,
        a()->GetNumMessagesInCurrentMessageArea());
}

static void HandleScanReadAutoReply(int &nMessageNumber, const char *pszUserInput, MsgScanOption& nScanOptionType) {
  if (!lcs() && get_post(nMessageNumber)->status & (status_unvalidated | status_delete)) {
    return;
  }
  if (get_post(nMessageNumber)->ownersys && !get_post(nMessageNumber)->owneruser) {
    grab_user_name(&(get_post(nMessageNumber)->msg), a()->current_sub().filename, a()->net_num());
  }
  grab_quotes(&(get_post(nMessageNumber)->msg), a()->current_sub().filename.c_str());

  if (okfsed() && a()->user()->IsUseAutoQuote() && nMessageNumber > 0 &&
      nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea() && pszUserInput[0] != 'O') {
    string b;
    readfile(&(get_post(nMessageNumber)->msg),
             (a()->current_sub().filename), &b);
    if (pszUserInput[0] == '@') {
      auto_quote(&b[0], b.size(), 1, get_post(nMessageNumber)->daten);
    } else {
      auto_quote(&b[0], b.size(), 3, get_post(nMessageNumber)->daten);
    }
  }

  if (get_post(nMessageNumber)->status & status_post_new_net) {
    set_net_num(get_post(nMessageNumber)->network.network_msg.net_number);
    if (get_post(nMessageNumber)->network.network_msg.net_number == -1) {
      bout << "|#6Deleted network.\r\n";
      return;
    }
  }

  if (pszUserInput[0] == 'O' && (so() || lcs())) {
    irt_sub[0] = 0;
    show_files("*.frm", a()->config()->gfilesdir().c_str());
    bout << "|#2Which form letter: ";
    char szFileName[MAX_PATH];
    input(szFileName, 8, true);
    if (!szFileName[0]) {
      return;
    }
    string full_pathname = StrCat(a()->config()->gfilesdir(), szFileName, ".frm");
    if (!File::Exists(full_pathname)) {
      full_pathname = StrCat(a()->config()->gfilesdir(), "form", szFileName, ".msg");
    }
    if (File::Exists(full_pathname)) {
      LoadFileIntoWorkspace(full_pathname, true);
      email(irt, get_post(nMessageNumber)->owneruser, get_post(nMessageNumber)->ownersys, false, get_post(nMessageNumber)->anony);
      grab_quotes(nullptr, nullptr);
    }
  } else if (pszUserInput[0] == '@') {
    bout.nl();
    bout << "|#21|#7]|#9 Reply to Different Address\r\n";
    bout << "|#22|#7]|#9 Forward Message\r\n";
    bout.nl();
    bout << "|#7(Q=Quit) Select [1,2] : ";
    char chReply = onek("Q12\r", true);
    switch (chReply) {
    case '\r':
    case 'Q':
      nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      break;
    case '1': {
      bout.nl();
      bout << "|#9Enter user name or number:\r\n:";
      char szUserNameOrNumber[ 81 ];
      input(szUserNameOrNumber, 75, true);
      size_t nAtPos = strcspn(szUserNameOrNumber, "@");
      if (nAtPos != strlen(szUserNameOrNumber) && isalpha(szUserNameOrNumber[ nAtPos + 1 ])) {
        if (strstr(szUserNameOrNumber, "@32767") == nullptr) {
          strlwr(szUserNameOrNumber);
          strcat(szUserNameOrNumber, " @32767");
        }
      }
      int user_number, system_number;
      parse_email_info(szUserNameOrNumber, &user_number, &system_number);
      if (user_number || system_number) {
        email("", user_number, system_number, false, 0);
      }
      nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    }
    break;
    case '2': {
      if (nMessageNumber > 0 && nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea()) {
        string b;
        readfile(&(get_post(nMessageNumber)->msg), (a()->current_sub().filename), &b);
        string filename = "EXTRACT.TMP";
        if (File::Exists(filename)) {
          File::Remove(filename);
        }
        File fileExtract(filename);
        if (!fileExtract.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
          bout << "|#6Could not open file for writing.\r\n";
        } else {
          if (fileExtract.GetLength() > 0) {
            fileExtract.Seek(-1L, File::Whence::end);
            char chLastChar = CZ;
            fileExtract.Read(&chLastChar, 1);
            if (chLastChar == CZ) {
              fileExtract.Seek(-1L, File::Whence::end);
            }
          }
          string buffer = StringPrintf("ON: %s", a()->current_sub().name.c_str());
          fileExtract.Write(buffer);
          fileExtract.Write("\r\n\r\n", 4);
          fileExtract.Write(get_post(nMessageNumber)->title, strlen(get_post(nMessageNumber)->title));
          fileExtract.Write("\r\n", 2);
          fileExtract.Write(b);
          fileExtract.Close();
        }
        bout.nl();
        bout << "|#5Allow editing? ";
        if (yesno()) {
          bout.nl();
          LoadFileIntoWorkspace(filename, false);
        } else {
          bout.nl();
          LoadFileIntoWorkspace(filename, true);
        }
        send_email();
        filename = StrCat(a()->temp_directory(), INPUT_MSG);
        if (File::Exists(filename)) {
          File::Remove(filename);
        }
      }
      nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    }
    break;
    }
  } else {
    if (lcs() || (getslrec(a()->GetEffectiveSl()).ability & ability_read_post_anony)
        || get_post(nMessageNumber)->anony == 0) {
      email("", get_post(nMessageNumber)->owneruser, get_post(nMessageNumber)->ownersys, false, 0);
    } else {
      email("", get_post(nMessageNumber)->owneruser, get_post(nMessageNumber)->ownersys, false, get_post(nMessageNumber)->anony);
    }
    irt_sub[0] = 0;
    grab_quotes(nullptr, nullptr);
  }
}

static void HandleScanReadFind(int &nMessageNumber, MsgScanOption& scan_option) {
  bool abort = false;
  char *pszTempFindString = nullptr;
  if (!(g_flags & g_flag_made_find_str)) {
    pszTempFindString = strupr(stripcolors(get_post(nMessageNumber)->title));
    strncpy(s_szFindString, pszTempFindString, sizeof(s_szFindString) - 1);
    g_flags |= g_flag_made_find_str;
  } else {
    pszTempFindString = &s_szFindString[0];
  }
  while (strncmp(pszTempFindString, "RE:", 3) == 0 || *pszTempFindString == ' ') {
    if (*pszTempFindString == ' ') {
      pszTempFindString++;
    } else {
      pszTempFindString += 3;
    }
  }
  if (strlen(pszTempFindString) >= 20) {
    pszTempFindString[20] = 0;
  }
  strncpy(s_szFindString, pszTempFindString, sizeof(s_szFindString) - 1);
  bout.nl();
  bout << "|#7Find what? (CR=\"" << pszTempFindString << "\")|#1";
  char szFindString[ 81 ];
  input(szFindString, 20);
  if (!*szFindString) {
    strncpy(szFindString, pszTempFindString, sizeof(szFindString) - 1);
  } else {
    strncpy(s_szFindString, szFindString, sizeof(s_szFindString) - 1);
  }
  bout.nl();
  bout << "|#1Backwards or Forwards? ";
  char szBuffer[10];
  szBuffer[0] = 'Q';
  szBuffer[1] = upcase(*"Backwards");
  szBuffer[2] = upcase(*"Forwards");
  strcpy(&szBuffer[3], "+-");
  char ch = onek(szBuffer);
  if (ch == 'Q') {
    return;
  }
  bool fnd = false;
  int nTempMsgNum = nMessageNumber;
  bout.nl();
  bout << "|#1Searching -> |#2";

  // Store search direction and limit
  bool bSearchForwards = true;
  int nMsgNumLimit;
  if (ch == '-' || ch == upcase(*"Backwards")) {
    bSearchForwards = false;
    nMsgNumLimit = 1;
  } else {
    bSearchForwards = true;
    nMsgNumLimit = a()->GetNumMessagesInCurrentMessageArea();
  }

  while (nTempMsgNum != nMsgNumLimit && !abort && !fnd) {
    if (bSearchForwards) {
      nTempMsgNum++;
    } else {
      nTempMsgNum--;
    }
    checka(&abort);
    if (!(nTempMsgNum % 5)) {
      bout.bprintf("%5.5d", nTempMsgNum);
      for (int i1 = 0; i1 < 5; i1++) {
        bout << "\b";
      }
      if (!(nTempMsgNum % 100)) {
        a()->tleft(true);
        CheckForHangup();
      }
    }
    string b;
    if (readfile(&(get_post(nTempMsgNum)->msg), a()->current_sub().filename, &b)) {
      StringUpperCase(&b);
      fnd = (strstr(strupr(stripcolors(get_post(nTempMsgNum)->title)), szFindString)
             || strstr(b.c_str(), szFindString)) ? true : false;
    }
  }
  if (fnd) {
    bout << "Found!\r\n";
    nMessageNumber = nTempMsgNum;
    scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
  } else {
    bout << "|#6Not found!\r\n";
    scan_option = MsgScanOption::SCAN_OPTION_READ_PROMPT;
  }
}

static FullScreenView CreateFullScreenListTitlesView() {
  auto screen_width = a()->user()->GetScreenChars();
  auto screen_length = a()->user()->GetScreenLines() - 1;
  int num_header_lines = 1;
  bout << "|14      Num" << " " << std::left << std::setw(43) 
       << "Title" << std::left << "From\r\n";
  bout.clear_lines_listed();
  return FullScreenView(num_header_lines, screen_width, screen_length);
}

static std::string CreateLine(std::unique_ptr<wwiv::sdk::msgapi::Message>&& msg, const int msgnum) {
  if (!msg) {
    return "";
  }
  char szPrompt[255];
  char szTempBuffer[255];
  const auto h = msg->header();
  if (h->is_local() && h->from_usernum() == a()->usernum) {
    sprintf(szTempBuffer, "|09[|11%d|09]", msgnum);
  }
  else if (!h->is_local()) {
    sprintf(szTempBuffer, "|09<|11%d|09>", msgnum);
  }
  else {
    sprintf(szTempBuffer, "|09(|11%d|09)", msgnum);
  }
  for (int i1 = 0; i1 < 7; i1++) {
    szPrompt[i1] = SPACE;
  }
  // HACK: Need to undo this before supporting JAM
  WWIVMessageHeader* wh = reinterpret_cast<WWIVMessageHeader*>(h);
  if (wh->data().qscan > qsc_p[a()->GetCurrentReadMessageArea()]) {
    szPrompt[0] = '*';
  }
  if (h->status() & (status_pending_net | status_unvalidated)) {
    szPrompt[0] = '+';
  }
  strcpy(&szPrompt[9 - strlen(stripcolors(szTempBuffer))], szTempBuffer);
  strcat(szPrompt, "|11 ");
  if ((h->status() & (status_unvalidated | status_delete)) && (!lcs())) {
    strcat(szPrompt, "<<< NOT VALIDATED YET >>>");
  }
  else {
    // Need the StringTrim on post title since some FSEDs
    // added \r in the title string, also gets rid of extra spaces
    auto title = h->title();
    StringTrim(&title);
    strncat(szPrompt, stripcolors(title).c_str(), 60);
  }

  if (a()->user()->GetScreenChars() >= 80) {
    if (strlen(stripcolors(szPrompt)) > 50) {
      while (strlen(stripcolors(szPrompt)) > 50) {
        szPrompt[strlen(szPrompt) - 1] = 0;
      }
    }
    strcat(szPrompt, charstr(51 - strlen(stripcolors(szPrompt)), ' '));
    if (okansi()) {
      strcat(szPrompt, "|09\xB3|10 ");
    }
    else {
      strcat(szPrompt, "| ");
    }
    if ((h->anony() & 0x0f) &&
      ((getslrec(a()->GetEffectiveSl()).ability & ability_read_post_anony) == 0)) {
      strcat(szPrompt, ">UNKNOWN<");
    }
    else {
      char from[81];
      to_char_array(from, h->from());
      strcat(szPrompt, from);
    }
  }
  return szPrompt;
}

static std::vector<std::string> CreateMessageTitleVector(MessageArea* area, int start, int num) {
  vector<string> lines;
  for (int i = start; i < (start + num); i++) {
    auto line = CreateLine(unique_ptr<Message>(area->ReadMessage(i)), i);
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}


static void display_titles_new(const std::vector<std::string>& lines, const FullScreenView& fs,
  int start, int selected) {
  for (int i = 0; i < fs.message_height(); i++) {
    if (i >= size_int(lines)) {
      break;
    }
    bout.GotoXY(1, i + fs.lines_start());
    const auto l = lines.at(i);
    if (i == (selected - start)) {
      bout << "|17|12>";
    }
    else {
      bout << "|16|#0 ";
    }
    bout << l << pad(fs.screen_width() - 1, stripcolors(l).size()) << "|#0";
  }
}

static ReadMessageResult HandleListTitlesFullScreen(int &msgnum, MsgScanOption& scan_option_type) {
  bout.cls();
  auto api = a()->msgapi();
  unique_ptr<MessageArea> area(api->Open(a()->current_sub().filename));
  if (!area) {
    ReadMessageResult result;
    result.command = 0;
    return result;
  }
  // Want to start the list after the one we just read.
  ++msgnum;
  scan_option_type = MsgScanOption::SCAN_OPTION_READ_PROMPT;

  auto num_msgs_in_area = area->number_of_messages();
  msgnum = std::min(msgnum, num_msgs_in_area);
  if (msgnum > num_msgs_in_area) {
    ReadMessageResult result;
    result.command = 0;
    return result;
  }

  auto fs = CreateFullScreenListTitlesView();
  fs.DrawTopBar();
  fs.DrawBottomBar("");
  fs.GotoContentAreaTop();

  const int window_top_min = 1;
  const int first = 1;
  const int last = std::max<int>(first, area->number_of_messages() - fs.message_height());
  const int height = std::min<int>(num_msgs_in_area, fs.message_height());

  int selected = msgnum;
  int window_top = std::min(msgnum, last);
  int window_bottom = window_top + height - window_top_min - 1;
  // When starting mid-range, sometimes the selected is past the bottom.
  if (selected > window_bottom) selected = window_bottom;

  bool done = false;
  while (!done) {
    CheckForHangup();
    auto lines = CreateMessageTitleVector(area.get(), window_top, height);
    display_titles_new(lines, fs, window_top, selected);

    bout.GotoXY(1, fs.lines_start() + selected - (window_top - window_top_min) + window_top_min);
    fs.DrawBottomBar(StrCat("Selected: ", selected));

    fs.ClearCommandLine();
    bout.GotoXY(1, fs.command_line_y());
    bout << "|#9(|#2Q|#9=Quit, |#2?|#9=Help): ";
    int key = bgetch_event(numlock_status_t::NOTNUMBERS, [&](bgetch_timeout_status_t status, int s)  { 
      if (status == bgetch_timeout_status_t::WARNING) {
        fs.PrintTimeoutWarning(s);
      }
      else if (status == bgetch_timeout_status_t::CLEAR) {
        fs.ClearCommandLine();
      }
    });
    switch (key) {
    case COMMAND_UP: {
      if (selected + window_top_min - 1 == window_top) {
        // At the top of the window, move the window up one.
        if (window_top > window_top_min) {
          window_top--;
          selected--;
        }
      }
      else {
        selected--;
      }
    } break;
    case COMMAND_PAGEUP: {
      window_top -= height;
      selected -= height;
      window_top = std::max<int>(window_top, window_top_min);
      selected = std::max<int>(selected, 1);
    } break;
    case COMMAND_HOME: {
      selected = 1;
      window_top = window_top_min;
    } break;
    case COMMAND_DOWN: {
      //int window_bottom = window_top + height - window_top_min - 1;
      if (selected <= window_bottom) {
        selected++;
      }
      else if (window_top < num_msgs_in_area - height + window_top_min) {
        selected++;
        window_top++;
      }
    } break;
    case COMMAND_PAGEDN: {
      window_top += height;
      selected += height;
      window_top = std::min<int>(window_top, num_msgs_in_area - height + window_top_min);
      selected = std::min<int>(selected, num_msgs_in_area);
    } break;
    case COMMAND_END: {
      window_top = num_msgs_in_area - height + window_top_min;
      selected = window_top;
    } break;
    case SOFTRETURN: {
      // Do nothing. SyncTerm sends CRLF on enter, not just CR
      // like we get from the local terminal. So we'll ignore the
      // SOFTRETURN (10, aka LF).
    } break;
    case RETURN: {
      ReadMessageResult result;
      result.option = ReadMessageOption::READ_MESSAGE;
      msgnum = selected;
      fs.ClearCommandLine();
      return result;
    } break;
    default: {
      if ((key & 0xff) == key) {
        key = toupper(key & 0xff);
        switch (key) {
        case 'J': {
          fs.ClearCommandLine();
          bout << "Enter Message Number (1-" << num_msgs_in_area << ") :";
          msgnum = input_number(msgnum, 1, num_msgs_in_area, false);

          ReadMessageResult result;
          result.option = ReadMessageOption::READ_MESSAGE;
          fs.ClearCommandLine();
          return result;
        } break;
        case 'Q': {
          ReadMessageResult result;
          result.option = ReadMessageOption::COMMAND;
          result.command = 'Q';
          fs.ClearCommandLine();
          return result;
        } break;
        case '?': {
          fs.ClearMessageArea();
          if (!printfile(TITLE_FSED_NOEXT)) {
            fs.ClearCommandLine();
            bout << "|#6Unable to find file: " << TITLE_FSED_NOEXT;
            pausescr();
            fs.ClearCommandLine();
          }
          else {
            fs.ClearCommandLine();
            pausescr();
            fs.ClearCommandLine();
          }
        } break;
        } // default switch
      }
    } break;
    }  // switch
  }
  return{};
}

static void HandleListTitles(int &msgnum, MsgScanOption& scan_option_type) {
  bout.cls();
  auto api = a()->msgapi();
  unique_ptr<MessageArea> area(api->Open(a()->current_sub().filename));
  if (!area) {
    return;
  }
  scan_option_type = MsgScanOption::SCAN_OPTION_READ_PROMPT;
  auto fs = CreateFullScreenListTitlesView();

  auto num_msgs_in_area = area->number_of_messages();
  bool abort = false;
  if (msgnum >= num_msgs_in_area) {
    return;
  }

  bout << "|#7" << static_cast<unsigned char>(198)
    << string(a()->user()->GetScreenChars() - 2, static_cast<unsigned char>(205))
    << static_cast<unsigned char>(181) << "\r\n";
  int nNumTitleLines = std::max<int>(a()->screenlinest - 6, 1);
  int i = 0;
  while (!abort && ++i <= nNumTitleLines) {
    ++msgnum;
    const string line = CreateLine(unique_ptr<Message>(area->ReadMessage(msgnum)), msgnum);
    pla(line, &abort);
    if (msgnum >= num_msgs_in_area) {
      abort = true;
    }
  }
}

static void HandleMessageDownload(int nMessageNumber) {
  if (nMessageNumber > 0 && nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea()) {
    string b;
    readfile(&(get_post(nMessageNumber)->msg), (a()->current_sub().filename), &b);
    bout << "|#1Message Download -\r\n\n";
    bout << "|#2Filename to use? ";
    string filename = input(12);
    if (!okfn(filename)) {
      return;
    }
    File fileTemp(a()->temp_directory(), filename);
    fileTemp.Delete();
    fileTemp.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    fileTemp.Write(b);
    fileTemp.Close();

    bool bFileAbortStatus;
    bool bStatus;
    send_file(fileTemp.full_pathname().c_str(), &bStatus, &bFileAbortStatus, fileTemp.full_pathname().c_str(), -1,
              b.length());
    bout << "|#1Message download... |#2" << (bStatus ? "successful" : "unsuccessful");
    if (bStatus) {
      sysoplog() << "Downloaded message";
    }
  }
}

void HandleMessageMove(int &nMessageNumber) {
  if ((lcs()) && (nMessageNumber > 0) && (nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea())) {
    char *ss1 = nullptr;
    tmp_disable_conf(true);
    bout.nl();
    do {
      bout << "|#5(|#2Q|#5=|#2Quit|#5) Move to which sub? ";
      ss1 = mmkey(0);
      if (ss1[0] == '?') {
        old_sublist();
      }
    } while (ss1[0] == '?');
    int nTempSubNum = -1;
    if (ss1[0] == 0) {
      tmp_disable_conf(false);
      return;
    }
    bool ok = false;
    for (size_t i1 = 0; (i1 < a()->subs().subs().size() && a()->usub[i1].subnum != -1 && !ok); i1++) {
      if (IsEquals(a()->usub[i1].keys, ss1)) {
        nTempSubNum = i1;
        bout.nl();
        bout << "|#9Copying to " << a()->subs().sub(a()->usub[nTempSubNum].subnum).name << endl;
        ok = true;
      }
    }
    if (nTempSubNum != -1) {
      if (a()->GetEffectiveSl() < a()->subs().sub(a()->usub[nTempSubNum].subnum).postsl) {
        bout.nl();
        bout << "Sorry, you don't have post access on that sub.\r\n\n";
        nTempSubNum = -1;
      }
    }
    if (nTempSubNum != -1) {
      open_sub(true);
      resynch(&nMessageNumber, nullptr);
      postrec p2 = *get_post(nMessageNumber);
      postrec p1 = p2;
      string b;
      readfile(&(p2.msg), (a()->current_sub().filename), &b);
      bout.nl();
      bout << "|#5Delete original post? ";
      if (yesno()) {
        delete_message(nMessageNumber);
        if (nMessageNumber > 1) {
          nMessageNumber--;
        }
      }
      close_sub();
      iscan(nTempSubNum);
      open_sub(true);
      p2.msg.storage_type = static_cast<unsigned char>(a()->current_sub().storage_type);
      savefile(b, &(p2.msg), (a()->current_sub().filename));
      a()->status_manager()->Run([&](WStatus& s) {
        p2.qscan = s.IncrementQScanPointer();
      });
      if (a()->GetNumMessagesInCurrentMessageArea() >=
        a()->current_sub().maxmsgs) {
        int nTempMsgNum = 1;
        int nMsgToDelete = 0;
        while (nMsgToDelete == 0 && nTempMsgNum <= a()->GetNumMessagesInCurrentMessageArea()) {
          if ((get_post(nTempMsgNum)->status & status_no_delete) == 0) {
            nMsgToDelete = nTempMsgNum;
          }
          ++nTempMsgNum;
        }
        if (nMsgToDelete == 0) {
          nMsgToDelete = 1;
        }
        delete_message(nMsgToDelete);
      }
      if ((!(a()->current_sub().anony & anony_val_net)) ||
          (a()->current_sub().nets.empty())) {
        p2.status &= ~status_pending_net;
      }
      if (!a()->current_sub().nets.empty()) {
        p2.status |= status_pending_net;
        a()->user()->SetNumNetPosts(a()->user()->GetNumNetPosts() + 1);
        send_net_post(&p2, a()->current_sub());
      }
      add_post(&p2);
      close_sub();
      tmp_disable_conf(false);
      iscan(a()->current_user_sub_num());
      bout.nl();
      bout << "|#9Message moved.\r\n\n";
      resynch(&nMessageNumber, &p1);
    } else {
      tmp_disable_conf(false);
    }
  }
}

static void HandleMessageLoad() {
  if (!so()) {
    return;
  }
  bout.nl();
  bout << "|#2Filename: ";
  char szFileName[MAX_PATH];
  input(szFileName, 50);
  if (szFileName[0]) {
    bout.nl();
    bout << "|#5Allow editing? ";
    if (yesno()) {
      bout.nl();
      LoadFileIntoWorkspace(szFileName, false);
    } else {
      bout.nl();
      LoadFileIntoWorkspace(szFileName, true);
    }
  }
}

void HandleMessageReply(int &nMessageNumber) {
  irt_sub[0] = 0;
  if ((!lcs()) && (get_post(nMessageNumber)->status & (status_unvalidated | status_delete))) {
    return;
  }
  postrec p2 = *get_post(nMessageNumber);
  grab_quotes(&(p2.msg), a()->current_sub().filename.c_str());

  if (okfsed() && a()->user()->IsUseAutoQuote() &&
      nMessageNumber > 0 && nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea()) {
    string b;
    if (readfile(&(get_post(nMessageNumber)->msg),
      a()->current_sub().filename, &b)) {
      auto_quote(&b[0], b.size(), 1, get_post(nMessageNumber)->daten);
    }
  }

  post();
  resynch(&nMessageNumber, &p2);
  grab_quotes(nullptr, nullptr);
}

static void HandleMessageDelete(int &nMessageNumber) {
  if (!lcs()) { 
    return; 
  }
  if (!nMessageNumber) { 
    return; 
  }

  bout << "|#5Delete message #" << nMessageNumber << ". Are you sure?";
  if (!noyes()) {
    return;
  }

  open_sub(true);
  resynch(&nMessageNumber, nullptr);
  postrec p2 = *get_post(nMessageNumber);
  delete_message(nMessageNumber);
  close_sub();
  if (p2.ownersys == 0) {
    User tu;
    a()->users()->ReadUser(&tu, p2.owneruser);
    if (!tu.IsUserDeleted()) {
      if (date_to_daten(tu.GetFirstOn()) < static_cast<time_t>(p2.daten)) {
        bout.nl();
        bout << "|#2Remove how many posts credit? ";
        char szNumCredits[ 10 ];
        input(szNumCredits, 3, true);
        int nNumCredits = 1;
        if (szNumCredits[0]) {
          nNumCredits = (atoi(szNumCredits));
        }
        nNumCredits = std::min<int>(nNumCredits, tu.GetNumMessagesPosted());
        if (nNumCredits) {
          tu.SetNumMessagesPosted(tu.GetNumMessagesPosted() - static_cast<uint16_t>(nNumCredits));
        }
        bout.nl();
        bout << "|#7Post credit removed = " << nNumCredits << endl;
        tu.SetNumDeletedPosts(tu.GetNumDeletedPosts() - 1);
        a()->users()->WriteUser(&tu, p2.owneruser);
        a()->UpdateTopScreen();
      }
    }
  }
  resynch(&nMessageNumber, &p2);
}

static void HandleMessageExtract(int &nMessageNumber) {
  if (!so()) {
    return;
  }
  if ((nMessageNumber > 0) && (nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea())) {
    string b;
    if (readfile(&(get_post(nMessageNumber)->msg), (a()->current_sub().filename), &b)) {
      extract_out(&b[0], b.size(), get_post(nMessageNumber)->title, get_post(nMessageNumber)->daten);
    }
  }
}

static void HandleMessageHelp() {
  if (forcescansub) {
    printfile(MUSTREAD_NOEXT);
  } else {
    if (lcs()) {
      printfile(SMBMAIN_NOEXT);
    } else {
      printfile(MBMAIN_NOEXT);
    }
  }
}

static void HandleValUser(int nMessageNumber) {
  if (cs() && (get_post(nMessageNumber)->ownersys == 0) && (nMessageNumber > 0)
    && (nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea())) {
    valuser(get_post(nMessageNumber)->owneruser);
  }
  else if (cs() && (nMessageNumber > 0) && (nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea())) {
    bout.nl();
    bout << "|#6Post from another system.\r\n\n";
  }
}

static void HandleToggleAutoPurge(int nMessageNumber) {
  if ((lcs()) && (nMessageNumber > 0) && (nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea())) {
    wwiv::bbs::OpenSub opened_sub(true);
    resynch(&nMessageNumber, nullptr);
    postrec *p3 = get_post(nMessageNumber);
    p3->status ^= status_no_delete;
    write_post(nMessageNumber, p3);
    bout.nl();
    if (p3->status & status_no_delete) {
      bout << "|#9Message will |#6NOT|#9 be auto-purged.\r\n";
    }
    else {
      bout << "|#9Message |#2CAN|#9 now be auto-purged.\r\n";
    }
    bout.nl();
  }
}


static void HandleTogglePendingNet(int nMessageNumber, int& val) {
  if (lcs() && nMessageNumber > 0 && nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea() &&
    a()->current_sub().anony & anony_val_net &&
    !a()->current_sub().nets.empty()) {
    wwiv::bbs::OpenSub opened_sub(true);
    resynch(&nMessageNumber, nullptr);
    postrec *p3 = get_post(nMessageNumber);
    p3->status ^= status_pending_net;
    write_post(nMessageNumber, p3);
    close_sub();
    bout.nl();
    if (p3->status & status_pending_net) {
      val |= 2;
      bout << "|#9Will be sent out on net now.\r\n";
    }
    else {
      bout << "|#9Not set for net pending now.\r\n";
    }
    bout.nl();
  }
}

static void HandleToggleUnAnonymous(int nMessageNumber) {
  if (lcs() && nMessageNumber > 0 && nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea()) {
    wwiv::bbs::OpenSub opened_sub(true);
    resynch(&nMessageNumber, nullptr);
    postrec *p3 = get_post(nMessageNumber);
    p3->anony = 0;
    write_post(nMessageNumber, p3);
    bout.nl();
    bout << "|#9Message is not anonymous now.\r\n";
  }
}

static void HandleScanReadPrompt(int &nMessageNumber, MsgScanOption& nScanOptionType, int *nextsub, bool &bTitleScan, bool &done,
  bool &quit, int &val) {
  resetnsp();
  string read_prompt = GetScanReadPrompts(nMessageNumber);
  bout.nl();
  char szUserInput[81];
  bout << read_prompt;
  input(szUserInput, 5, true);
  resynch(&nMessageNumber, nullptr);
  while (szUserInput[0] == 32) {
    char szTempBuffer[255];
    strcpy(szTempBuffer, &(szUserInput[1]));
    strcpy(szUserInput, szTempBuffer);
  }
  if (bTitleScan && szUserInput[0] == 0 && nMessageNumber < a()->GetNumMessagesInCurrentMessageArea()) {
    nScanOptionType = MsgScanOption::SCAN_OPTION_LIST_TITLES;
    szUserInput[0] = 'T';
    szUserInput[1] = '\0';
  } else {
    bTitleScan = false;
    nScanOptionType = MsgScanOption::SCAN_OPTION_READ_PROMPT;
  }
  int nUserInput = atoi(szUserInput);
  if (szUserInput[0] == '\0') {
    nUserInput = nMessageNumber + 1;
    if (nUserInput >= a()->GetNumMessagesInCurrentMessageArea() + 1) {
      done = true;
    }
  }

  if (nUserInput != 0 && nUserInput <= a()->GetNumMessagesInCurrentMessageArea() && nUserInput >= 1) {
    nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    nMessageNumber = nUserInput;
  } else if (szUserInput[1] == '\0') {
    if (forcescansub) {
      return;
    }
    switch (szUserInput[0]) {
    case '$':
    { // Last message in area.
      nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      nMessageNumber = a()->GetNumMessagesInCurrentMessageArea();
    }
    break;
    case 'F':
    { // Find addition
      HandleScanReadFind(nMessageNumber, nScanOptionType);
    }
    break;
    // End of find addition

    case 'Q':
      quit = true;
      done = true;
      *nextsub = 0;
      break;
    case 'B':
      if (*nextsub != 0) {
        bout.nl();
        bout << "|#5Remove this sub from your Q-Scan? ";
        if (yesno()) {
          qsc_q[a()->current_user_sub().subnum / 32] ^= (1L <<
            (a()->current_user_sub().subnum % 32));
        } else {
          bout.nl();
          bout << "|#9Mark messages in "
            << a()->subs().sub(a()->current_user_sub().subnum).name
            << " as read? ";
          if (yesno()) {
            unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
            qsc_p[a()->current_user_sub().subnum] = pStatus->GetQScanPointer() - 1L;
          }
        }

        *nextsub = 1;
        done = true;
        quit = true;
      }
      break;
    case 'T':
      bTitleScan = true;
      nScanOptionType = MsgScanOption::SCAN_OPTION_LIST_TITLES;
      break;
    case 'R':
      nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      break;
    case '@':
      to_char_array(irt_sub, a()->subs().sub(a()->current_user_sub().subnum).name);
    case 'O':
    case 'A':
      HandleScanReadAutoReply(nMessageNumber, szUserInput, nScanOptionType);
    break;
    case 'P':
      irt[0] = '\0';
      irt_name[0] = '\0';
      post();
      break;
    case 'W':
      HandleMessageReply(nMessageNumber);
      break;
    case 'Y':
      HandleMessageDownload(nMessageNumber);
      break;
    case '?':
      HandleMessageHelp();
      break;
    case '-':
      if (nMessageNumber > 1 && (nMessageNumber - 1 < a()->GetNumMessagesInCurrentMessageArea())) {
        --nMessageNumber;
        nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      }
      break;
    case '*':
      // This used to be threaded code.
      break;
    case '[':
      // This used to be threaded code.
      break;
    case ']':
      // This used to be threaded code.
      break;
    case '>':
      // This used to be threaded code.
      nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      break;

    case 'V': HandleValUser(nMessageNumber); break;
    case 'N': HandleToggleAutoPurge(nMessageNumber); break;
    case 'X': HandleTogglePendingNet(nMessageNumber, val); break;
    case 'U': HandleToggleUnAnonymous(nMessageNumber); break;
    case 'D':
      HandleMessageDelete(nMessageNumber);
      break;
    case 'E':
      HandleMessageExtract(nMessageNumber);
      break;
    case 'M':
      HandleMessageMove(nMessageNumber);
      break;
    case 'L':
      HandleMessageLoad();
      break;
    }
  } else if (IsEquals(szUserInput, "cls")) {
    bout.bputch('\x0c');
  }
}

static void validate() {
  bout.nl();
  bout << "|#5Validate messages here? ";
  if (noyes()) {
    wwiv::bbs::OpenSub opened_sub(true);
    for (int i = 1; i <= a()->GetNumMessagesInCurrentMessageArea(); i++) {
      postrec* p3 = get_post(i);
      if (p3->status & (status_unvalidated | status_delete)) {
        p3->status &= (~(status_unvalidated | status_delete));
      }
      write_post(i, p3);
    }
  }
}

static void network_validate() {
  bout.nl();
  bout << "|#5Network validate here? ";
  if (yesno()) {
    int nNumMsgsSent = 0;
    vector<postrec> to_validate;
    {
      wwiv::bbs::OpenSub opened_sub(true);
      for (int i = 1; i <= a()->GetNumMessagesInCurrentMessageArea(); i++) {
        postrec *p4 = get_post(i);
        if (p4->status & status_pending_net) {
          to_validate.push_back(*p4);
          p4->status &= ~status_pending_net;
          write_post(i, p4);
        }
      }
    }
    for (auto p : to_validate) {
      send_net_post(&p, a()->current_sub());
      nNumMsgsSent++;
    }

    bout.nl();
    bout << nNumMsgsSent << " messages sent.";
    bout.nl(2);
  }
}

static void query_post() {
  if (!a()->user()->IsRestrictionPost() &&
     (a()->user()->GetNumPostsToday() < getslrec(a()->GetEffectiveSl()).posts) &&
     (a()->GetEffectiveSl() >= a()->current_sub().postsl)) {
    bout << "|#5Post on " << a()->current_sub().name << "? ";
    irt[0] = '\0';
    irt_name[0] = '\0';
    grab_quotes(nullptr, nullptr);
    if (yesno()) {
      post();
    }
  }
}

static void scan_new(int msgnum, MsgScanOption scan_option, int *nextsub, bool title_scan) {
  bool done = false;
  int val = 0;
  while (!done) {
    CheckForHangup();
    ReadMessageResult result{};
    if (scan_option == MsgScanOption::SCAN_OPTION_READ_MESSAGE) {
      bool next = true;
      result = read_post(msgnum, &next, &val);
    }
    else if (scan_option == MsgScanOption::SCAN_OPTION_LIST_TITLES) {
      result = HandleListTitlesFullScreen(msgnum, scan_option);
    } else if (scan_option == MsgScanOption::SCAN_OPTION_READ_PROMPT) {
      bool quit = false;
      HandleScanReadPrompt(msgnum, scan_option, nextsub, title_scan, done, quit, val);
      if (quit) { done = true; }
    }

    switch (result.option) {
    case ReadMessageOption::READ_MESSAGE: {
      if (msgnum > a()->GetNumMessagesInCurrentMessageArea()) {
        done = true;
      }
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    } break;
    case ReadMessageOption::NEXT_MSG: {
      if (++msgnum > a()->GetNumMessagesInCurrentMessageArea()) {
        done = true;
      }
    } break;
    case ReadMessageOption::PREV_MSG: {
      if (--msgnum <= 0) {
        done = true;
      }
    } break;
    case ReadMessageOption::JUMP_TO_MSG: {
      const auto maxnum = a()->GetNumMessagesInCurrentMessageArea();
      bout << "Enter Message Number (1-" << maxnum << ") :";
      msgnum = input_number(msgnum, 1, maxnum, false);
    } break;
    case ReadMessageOption::LIST_TITLES: {
      scan_option = MsgScanOption::SCAN_OPTION_LIST_TITLES;
    } break;
    case ReadMessageOption::COMMAND: {
      switch (result.command) {
      case 'Q': done = true; *nextsub = 0; break;
      case 'A': HandleScanReadAutoReply(msgnum, "A", scan_option); break;
      case 'D': HandleMessageDelete(msgnum); break;
      case 'E': HandleMessageExtract(msgnum); break;
      case 'L': HandleMessageLoad(); break;
      case 'M': HandleMessageMove(msgnum); break;
      case 'N': HandleToggleAutoPurge(msgnum); break;
      case 'X': HandleTogglePendingNet(msgnum, val); break;
      case 'U': HandleToggleUnAnonymous(msgnum); break;
      case 'V': HandleValUser(msgnum); break;
      case 'P': {
        irt[0] = '\0';
        irt_name[0] = '\0';
        post();
      } break;
      case 'W': HandleMessageReply(msgnum); break;
      }
    } break;
    }
  }
  if ((val & 1) && lcs()) {
    validate();
  }
  if ((val & 2) && lcs()) {
    network_validate();
  }
  query_post();
}

void scan(int nMessageNumber, MsgScanOption nScanOptionType, int *nextsub, bool bTitleScan) {
  irt[0] = '\0';
  irt_name[0] = '\0';

  int val = 0;
  iscan(a()->current_user_sub_num());
  if (a()->GetCurrentReadMessageArea() < 0) {
    bout.nl();
    bout << "No subs available.\r\n\n";
    return;
  }

  if (a()->experimental_read_prompt() && a()->user()->HasStatusFlag(User::fullScreenReader)) {
    scan_new(nMessageNumber, nScanOptionType, nextsub, bTitleScan);
    return;
  }

  bool done = false;
  bool quit = false;
  do {
    a()->tleft(true);
    CheckForHangup();
    set_net_num((a()->current_sub().nets.empty()) ? 0 :
      a()->current_sub().nets[0].net_num);
    if (nScanOptionType != MsgScanOption::SCAN_OPTION_READ_PROMPT) {
      resynch(&nMessageNumber, nullptr);
    }
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);

    switch (nScanOptionType) {
    case MsgScanOption::SCAN_OPTION_READ_PROMPT:
    { // Read Prompt
      HandleScanReadPrompt(nMessageNumber, nScanOptionType, nextsub, bTitleScan, done, quit, val);
    }
    break;
    case MsgScanOption::SCAN_OPTION_LIST_TITLES:
    { // List Titles
      HandleListTitles(nMessageNumber, nScanOptionType);
    }
    break;
    case MsgScanOption::SCAN_OPTION_READ_MESSAGE:
    { // Read Message
      bool next = false;
      if (nMessageNumber > 0 && nMessageNumber <= a()->GetNumMessagesInCurrentMessageArea()) {
        bool old_incom = incom;
        if (forcescansub) {
          incom = false;
        }
        read_post(nMessageNumber, &next, &val);
        if (forcescansub) {
          incom = old_incom;
        }
      }
      bout.Color(0);
      bout.nl();
      if (next) {
        ++nMessageNumber;
        if (nMessageNumber > a()->GetNumMessagesInCurrentMessageArea()) {
          done = true;
        }
        nScanOptionType = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      } else {
        nScanOptionType = MsgScanOption::SCAN_OPTION_READ_PROMPT;
      }
    }
    break;
    }
  } while (!done);

  if ((val & 1) && lcs()) {
    validate();
  }
  if ((val & 2) && lcs()) {
    network_validate();
  }
  bout.nl();
  if (quit) {
    return;
  }
  query_post();
  bout.nl();
}
