/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "bbs/inmsg.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <sstream>
#include <vector>

#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/crc.h"
#include "bbs/input.h"
#include "sdk/subxtr.h"
#include "bbs/printfile.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/instmsg.h"
#include "bbs/external_edit.h"
#include "bbs/keycodes.h"
#include "bbs/message_file.h"
#include "bbs/quote.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "core/wwivassert.h"
#include "sdk/datetime.h"
#include "sdk/names.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::bbs::InputMode;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static const int LEN = 161;
static const char crlf[] = "\r\n";

bool MessageEditorData::is_email() const {
  return iequals(aux, "email");
}


static bool GetMessageToName(bool is_email) {
  // If a()->GetCurrentReadMessageArea() is -1, then it hasn't been set by reading a sub,
  // also, if we are using e-mail, this is definately NOT a FidoNet
  // post so there's no reason in wasting everyone's time in the loop...
  if (a()->GetCurrentReadMessageArea() == -1 || is_email) {
    return false;
  }

  if (a()->current_sub().nets.empty()) {
    return false;
  }

  bool has_address = false;
  bool newlsave = bout.newline;
  for (const auto& xnp : a()->current_sub().nets) {
    if (a()->net_networks[xnp.net_num].type == network_type_t::ftn && !is_email) {
      bout << "|#2To   : ";
      bout.newline = false;
      auto to_name = Input1("All", 40, true, InputMode::MIXED);
      bout.newline = newlsave;
      if (to_name.empty()) {
        to_char_array(irt_name, "All");
        bout << "|#4All\r\n";
        bout.Color(0);
      } else {
        to_char_array(irt_name, to_name);
      }
      return true;
      // WTF??? strcpy(irt, "\xAB");
    }
  }

  return false;
}

static void GetMessageTitle(MessageEditorData& data) {
  auto force_title = !data.title.empty();
  if (okansi()) {
    if (!data.silent_mode) {
      bout << "|#2Title: ";
      bout.mpl(60);
    }
    if (!data.title.empty()) {
      bout << data.title;
      return;
    }
    if (irt[0] != '\xAB' && irt[0]) {
      string s1;
      char ch = '\0';
      StringTrim(irt);
      if (strncasecmp(stripcolors(irt), "re:", 3) != 0) {
        if (data.silent_mode) {
          s1 = irt;
          irt[0] = '\0';
        } else {
          s1 = StrCat("Re: ", irt);
        }
      } else {
        s1 = irt;
      }
      if (s1.length() > 60) {
        s1.resize(60);
      }
      if (!data.silent_mode && !force_title) {
        bout << s1;
        ch = bout.getkey();
        if (ch == 10) {
          ch = bout.getkey();
        }
      } else {
        data.title.assign(s1);
        ch = RETURN;
      }
      force_title = false;
      if (ch != RETURN) {
        bout << "\r";
        if (ch == SPACE || ch == ESC) {
          ch = '\0';
        }
        bout << "|#2Title: ";
        bout.mpl(60);
        auto rollover_line = StringPrintf("%c", ch);
        inli(&s1, &rollover_line, 60, true, false);
        data.title.assign(s1);
      } else {
        bout.nl();
        data.title.assign(s1);
      }
    } else {
      data.title = inputl(60);
    }
  } else {
    if (data.silent_mode || force_title) {
      data.title.assign(irt);
    } else {
      bout << "       (---=----=----=----=----=----=----=----=----=----=----=----)\r\n";
      bout << "Title: ";
      data.title = inputl(60);
    }
  }
}

static bool InternalMessageEditor(vector<string>& lin, int maxli, int* setanon, string *title) {
  bool abort = false, next = false;
  int curli = 0;

  bout.nl(2);
  bout << "|#9Enter message now, you can use |#2" << maxli << "|#9 lines.\r\n";
  bout << "|#9Colors: ^P-0\003""11\003""22\003""33\003""44\003""55\003""66\003""77\003""88\003""99\003""0";
  bout.nl();
  bout << "|#9Enter |#2/Q|#9 to quote previous message, |#2/HELP|#9 for other editor commands.\r\n";

  bout.Color(7);
  string header = "[---=----=----=----=----=----=----=----]----=----=----=----=----=----=----=----]";
  if (a()->user()->GetScreenChars() < 80) {
    header.resize(a()->user()->GetScreenChars());
  }
  bout << header;
  bout.nl();

  string current_line;
  bool check_message_size = true;
  bool save_message = false;
  bool done = false;
  string rollover_line;
  while (!done && !hangup) {
    while (inli(&current_line, &rollover_line, 160, true, (curli > 0))) {
      // returning true means we back spaced past the current line.
      // intuitive eh?
      curli--;
      if (curli >= 0) {
        // Don't keep retreating past line 0.
        rollover_line = lin.at(curli);
        if (rollover_line.length() > a()->user()->GetScreenChars() - 1) {
          rollover_line.resize(a()->user()->GetScreenChars() - 2);
        }
      } else {
        curli = 0;
      }
    }
    if (hangup) {
      done = true;
    }
    check_message_size = true;
    if (current_line[0] == '/') {
      auto cmd = current_line;
      StringUpperCase(&cmd);
      if (((cmd == "/HELP")) ||
          ((cmd == "/H")) ||
          ((cmd == "/?"))) {
        check_message_size = false;
        print_help_file(EDITOR_NOEXT);
      } else if ((cmd == "/QUOTE") ||
                 (cmd == "/Q")) {
        check_message_size = false;
        if (quotes_ind != nullptr) {
          get_quote(irt_name);
        }
      } else if ((cmd == "/LI")) {
        check_message_size = false;
        abort = false;
        for (int i = 0; (i < curli) && !abort; i++) {
          auto line = lin.at(i);
          if (!line.empty() && line.back() == CA) {
            line.pop_back();
          }
          if (!line.empty() && line.front() == CB) {
            line = line.substr(1);
            int i5 = 0;
            // TODO(rusnfan): Make a utility function to do this in strings.h
            for (size_t i4 = 0; i4 < line.size(); i4++) {
              if ((line[i4] == 8) || (line[i4] == 3)) {
                --i5;
              } else {
                ++i5;
              }
            }
            for (size_t i4 = 0; (i4 < (a()->user()->GetScreenChars() - i5) / 2) && (!abort); i4++) {
              bout.bputs(" ", &abort, &next);
            }
          }
          bout.bpla(line, &abort);
        }
        if (!okansi() || next) {
          bout.nl();
          bout << "Continue...\r\n";
        }
      } else if ((cmd == "/ES") ||
                 (cmd == "/S")) {
        save_message = true;
        done = true;
        check_message_size = false;
      } else if ((cmd == "/ESY") ||
                 (cmd == "/SY")) {
        save_message = true;
        done = true;
        check_message_size = false;
        *setanon = 1;
      } else if ((cmd == "/ESN") ||
                 (cmd == "/SN")) {
        save_message = true;
        done = true;
        check_message_size = false;
        *setanon = -1;
      } else if ((cmd == "/ABT") ||
                 (cmd == "/A")) {
        done = true;
        check_message_size = false;
      } else if ((cmd == "/CLR")) {
        check_message_size = false;
        curli = 0;
        bout << "Message cleared... Start over...\r\n\n";
      } else if (cmd == "/RL") {
        check_message_size = false;
        if (curli) {
          curli--;
          bout << "Replace:\r\n";
        } else {
          bout << "Nothing to replace.\r\n";
        }
      } else if (cmd == "/TI") {
        check_message_size = false;
        bout << "|#1Subj|#7: |#2" ;
        *title = inputl(60, true);
        bout << "Continue...\r\n\n";
      }
      if (cmd.length() > 3) {
        cmd.resize(3);
      }
      if (cmd == "/C:") {
        const auto centered_text = current_line.substr(3);
        current_line = StringPrintf("%c%s", CB, centered_text.c_str());
      } else if (cmd == "/SU" && current_line[3] == '/' && curli > 0) {
        auto old_string = current_line.substr(4);
        auto slash = old_string.find('/');
        if (slash != string::npos) {
          auto new_string = old_string.substr(slash + 1);
          old_string.resize(slash);
          // We've already advanced curli to point to the new row, so we need
          // to back up one.
          auto last_line = curli - 1;
          auto temp = lin.at(last_line);
          wwiv::strings::StringReplace(&temp, old_string, new_string);
          lin[last_line] = temp;
          bout << "Last line:\r\n" << lin.at(last_line) << "\r\nContinue...\r\n";
        }
        check_message_size = false;
      }
    }

    if (check_message_size) {
      if (curli == size_int(lin)) {
        // we're inserting a new line at the end.
        lin.emplace_back(current_line);
      } else if (curli < size_int(lin)) {
        // replacing an older line.
        lin.at(curli).assign(current_line);
      }
      // Since we added the line, let's move forward.
      curli++;
      if (curli == (maxli + 1)) {
        bout << "\r\n-= No more lines, last line lost =-\r\n/S to save\r\n\n";
        curli--;
      } else if (curli == maxli) {
        bout << "-= Message limit reached, /S to save =-\r\n";
      } else if ((curli + 5) == maxli) {
        bout << "-= 5 lines left =-\r\n";
      }
    }
  }
  if (curli == 0) {
    save_message = false;
  }
  lin.resize(curli);
  return save_message;
}


static void UpdateMessageBufferInReplyToInfo(std::ostringstream& ss, bool is_email, const string& to_name) {
  if (!to_name.empty() && !is_email && !a()->current_sub().nets.empty()) {
    for (const auto& xnp : a()->current_sub().nets) {
      if (a()->net_networks[xnp.net_num].type == network_type_t::ftn) {
        const auto buf = StringPrintf("%c0FidoAddr: %s", CD, to_name.c_str());
        ss << buf << crlf;
        break;
      }
    }
  }
  if (a()->current_net().type == network_type_t::internet) {
    if (a()->usenetReferencesLine.length() > 0) {
      const auto buf = StringPrintf("%c0RReferences: %s", CD, a()->usenetReferencesLine.c_str());
      ss << buf << crlf;
      a()->usenetReferencesLine = "";
    }
  }

  // WTF is \xAB. FidoAddr sets it, but we don't want
  // to add the RE: line when it's \xAB, so let's skip it.
  if (irt[0] != '"' && irt[0] != '\xAB') {
    ss << "RE: " << irt << crlf;
  }

  if (!to_name.empty() && !is_email) {
    ss << "BY: " << to_name << crlf;
  }
  ss << crlf;
}

static string FindTagFileName() {
  for (const auto& xnp : a()->current_sub().nets) {
    auto nd = a()->net_networks[xnp.net_num].dir;
    auto filename = StrCat(nd, xnp.stype, ".tag");
    if (File::Exists(filename)) {
      return filename;
    }
    filename = StrCat(nd, GENERAL_TAG);
    if (File::Exists(filename)) {
      return filename;
    }
    filename = StrCat(a()->config()->datadir(), xnp.stype, ".tag");
    if (File::Exists(filename)) {
      return filename;
    }
    filename = StrCat(a()->config()->datadir(), GENERAL_TAG);
    if (File::Exists(filename)) {
      return filename;
    }
  }
  return "";
}

static void UpdateMessageBufferTagLine(std::ostringstream& ss, bool is_email, const string& title, const string& to_name) {
  if (a()->subs().subs().size() <= 0 && a()->GetCurrentReadMessageArea() <= 0) {
    return;
  }
  if (is_email) {
    return;
  }
  if (a()->current_sub().nets.empty()) {
    return;
  }
  if (a()->current_sub().anony & anony_no_tag) {
    return;
  }
  if (iequals(to_name, "Multi-Mail")) {
    return;
  }

  const auto filename = FindTagFileName();
  if (filename.empty()) {
    // FindTagFileName returns an empty string if no tagname exists, so
    // just exit here since there is no tag.
  }
  TextFile file(filename, "rb");
  if (file.IsOpen()) {
    int j = 0;
    string s;
    do {
      s.clear();
      file.ReadLine(&s);
      if (s.length() > 1 && s[s.length() - 2] == RETURN) {
        // remove last 2 characters.
        s.pop_back();
        s.pop_back();
      }
      string s1 = s;
      if (s[0] != CD) {
        s1 = StringPrintf("%c%c%s", CD, j + '2', s.c_str());
      }
      if (!j) {
        ss << StringPrintf("%c1", CD) << crlf;
      }
      ss << s1 << crlf;
      if (j < 7) {
        j++;
      }
    } while (!file.IsEndOfFile());
    file.Close();
  }
}

static void UpdateMessageBufferQuotesCtrlLines(std::ostringstream& ss) {
  const auto quotes_filename = StrCat(a()->temp_directory(), QUOTES_TXT);
  TextFile file(quotes_filename, "rt");
  if (file.IsOpen()) {
    string quote_text;
    while (file.ReadLine(&quote_text)) {
      auto slash_n = quote_text.find('\n');
      if (slash_n != string::npos) {
        quote_text.resize(slash_n);
      }
      if (starts_with(quote_text, "\004""0U")) {
        ss << quote_text << crlf;
      }
    }
    file.Close();
  }

  const auto msginf_filename = StrCat(a()->temp_directory(), "msginf");
  copyfile(quotes_filename, msginf_filename, false);
}

static void GetMessageAnonStatus(bool *real_name, uint8_t *anony, int setanon) {
  // Changed *anony to anony
  switch (*anony) {
  case 0:
    *anony = 0;
    break;
  case anony_enable_anony:
    if (setanon) {
      if (setanon == 1) {
        *anony = anony_sender;
      } else {
        *anony = 0;
      }
    } else {
      bout << "|#5Anonymous? ";
      if (yesno()) {
        *anony = anony_sender;
      } else {
        *anony = 0;
      }
    }
    break;
  case anony_enable_dear_abby: {
    bout.nl();
    bout << "1. " << a()->names()->UserName(a()->usernum) << wwiv::endl;
    bout << "2. Abby\r\n";
    bout << "3. Problemed Person\r\n\n";
    bout << "|#5Which? ";
    char chx = onek("\r123");
    switch (chx) {
    case '\r':
    case '1':
      *anony = 0;
      break;
    case '2':
      *anony = anony_sender_da;
      break;
    case '3':
      *anony = anony_sender_pp;
    }
  }
  break;
  case anony_force_anony:
    *anony = anony_sender;
    break;
  case anony_real_name:
    *real_name = true;
    *anony = 0;
    break;
  }
}

bool inmsg(MessageEditorData& data) {

  auto oiia = setiia(std::chrono::milliseconds(0));
  if (data.fsed_flags != FsedFlags::NOFSED && !okfsed()) {
    data.fsed_flags = FsedFlags::NOFSED;
  }

  const auto exted_filename = StrCat(a()->temp_directory(), INPUT_MSG);
  if (data.fsed_flags != FsedFlags::NOFSED) {
    data.fsed_flags = FsedFlags::FSED;
  }
  if (use_workspace) {
    if (!File::Exists(exted_filename)) {
      use_workspace = false;
    } else {
      data.fsed_flags = FsedFlags::WORKSPACE;
    }
  }

  wwiv::core::ScopeExit at_exit([=]() {
    setiia(oiia);
    a()->charbufferpointer_ = 0;
    charbuffer[0] = '\0';
    grab_quotes(nullptr, nullptr);
    if (data.fsed_flags != FsedFlags::NOFSED) {
      File::Remove(exted_filename);
    }
  });

  bout.nl();

  if (data.to_name.empty()) {
    if (GetMessageToName(data.is_email())) {
      bout.nl();
    }
  }
  else {
    bout << "|#2To   : ";
    bout.mpl(40);
    bout << data.to_name << "\r\n";
  }

  GetMessageTitle(data);
  if (data.title.empty() && data.need_title) {
    bout << "|#6Aborted.\r\n";
    return false;
  }

  vector<string> lin;
  int setanon = 0;
  auto save_message = false;
  auto maxli = GetMaxMessageLinesAllowed();
  if (data.fsed_flags == FsedFlags::NOFSED) {   // Use Internal Message Editor
    save_message = InternalMessageEditor(lin, maxli, &setanon, &data.title);
  } else if (data.fsed_flags == FsedFlags::FSED) {   // Use Full Screen Editor
    save_message = ExternalMessageEditor(maxli, &setanon, &data.title, data.to_name, data.msged_flags, data.is_email());
  } else if (data.fsed_flags == FsedFlags::WORKSPACE) {   // "auto-send mail message"
    save_message = File::Exists(exted_filename);
    if (save_message && !data.silent_mode) {
      bout << "Reading in file...\r\n";
    }
    use_workspace = false;
  }

  if (!save_message) {
    bout << "|#6Aborted.\r\n";
    return false;
  }
  bout.backline();
  if (!data.silent_mode) {
    SpinPuts("Saving...", 2);
  }
  std::ostringstream b;

  auto real_name = false;
  GetMessageAnonStatus(&real_name, &data.anonymous_flag, setanon);
  // Add author name
  if (real_name) {
    b << a()->user()->GetRealName() << crlf;
  } else if (data.silent_mode) {
    b << a()->config()->config()->sysopname << " #1" << crlf;
  } else {
    b << a()->names()->UserName(a()->usernum, a()->current_net().sysnum) << crlf;
  }

  // Add date to message body
  b << daten_to_wwivnet_time(daten_t_now()) << crlf;
  UpdateMessageBufferQuotesCtrlLines(b);

  if (irt[0]) {
    UpdateMessageBufferInReplyToInfo(b, data.is_email(), irt_name);
  }

  // TODO(rushfan): This and the date above, etc. Will need to be transformed
  // into structured data when moving to a proper message API.
  //
  // New in 5.x. Add PID to all message types. This will be for WWIV messages
  // as well as FTN messages so we know the Program ID (WWIV) that created
  // the message.
  const auto control_d_zero = StringPrintf("%c0", CD);
  b << control_d_zero << "PID: WWIV " << wwiv_version << beta_version << crlf;

  if (data.fsed_flags != FsedFlags::NOFSED) {
    // Read the file produced by the external editor and add it to the message.
    TextFile editor_file(exted_filename, "r");
    string line;
    while (editor_file.ReadLine(&line)) {
      b << line << crlf;
    }
  } else {
    // iterate through the lines in "lin" and append them to 'b'
    for (const auto& l : lin) {
      b << l << crlf;
    }
  }

  if (a()->HasConfigFlag(OP_FLAGS_MSG_TAG)) {
    UpdateMessageBufferTagLine(b, data.is_email(), data.title, data.to_name);
  }

  auto text = b.str();
  if (text.back() != CZ) {
    text.push_back(CZ);
  }
  data.text = std::move(text);
  return true;
}
