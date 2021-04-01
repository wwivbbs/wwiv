/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl.h"
#include "bbs/external_edit.h"
#include "bbs/instmsg.h"
#include "bbs/utility.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "common/quote.h"
#include "common/workspace.h"
#include "core/datetime.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "fsed/fsed.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/subxtr.h"
#include "sdk/net/networks.h"
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

using wwiv::common::InputMode;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

static const char crlf[] = "\r\n";

static bool GetMessageToName(MessageEditorData& data) {
  // If a()->sess().GetCurrentReadMessageArea() is -1, then it hasn't been set by reading a sub,
  // also, if we are using e-mail, this is definately NOT a FidoNet
  // post so there's no reason in wasting everyone's time in the loop...
  if (a()->sess().GetCurrentReadMessageArea() == -1 || data.is_email()) {
    return false;
  }

  if (a()->current_sub().nets.empty()) {
    return false;
  }

  const auto saved_newline = bout.newline;
  for (const auto& xnp : a()->current_sub().nets) {
    if (a()->nets()[xnp.net_num].type == network_type_t::ftn && !data.is_email()) {
      bout << "|#2To   : ";
      bout.newline = false;
      const auto to_name = bin.input_text("All", 40);
      bout.newline = saved_newline;
      if (to_name.empty()) {
        data.to_name = "All";
        bout << "|#4All\r\n";
        bout.Color(0);
      } else {
        data.to_name = to_name;
      }
      return true;
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
    auto irt = a()->sess().irt();
    if (!irt.empty()) {
      std::string s1;
      auto ch = '\0';
      irt = stripcolors(StringTrim(a()->sess().irt()));
      if (strncasecmp(irt.c_str(), "re:", 3) != 0) {
        if (data.silent_mode) {
          s1 = irt;
          a()->sess().clear_irt();
        } else {
          s1 = StrCat("Re: ", irt);
        }
      } else {
        s1 = a()->sess().irt();
      }
      if (s1.length() > 60) {
        s1.resize(60);
      }
      if (!data.silent_mode && !force_title) {
        bout << s1;
        ch = bin.getkey();
        if (ch == 10) {
          ch = bin.getkey();
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
        auto rollover_line = fmt::sprintf("%c", ch);
        inli(&s1, &rollover_line, 60, true, false);
        data.title.assign(s1);
      } else {
        bout.nl();
        data.title.assign(s1);
      }
    } else {
      data.title = bin.input_text(60);
    }
  } else {
    if (data.silent_mode || force_title) {
      data.title = a()->sess().irt();
    } else {
      bout << "       (---=----=----=----=----=----=----=----=----=----=----=----)\r\n";
      bout << "Title: ";
      data.title = bin.input_text(60);
    }
  }
}

static bool InternalMessageEditor(std::vector<std::string>& lin, int maxli, int* setanon, MessageEditorData& data) {
  auto abort = false, next = false;
  auto curli = 0;

  bout.nl(2);
  bout << "|#9Enter message now, you can use |#2" << maxli << "|#9 lines.\r\n";
  bout << "|#9Colors: ^P-0\003""11\003""22\003""33\003""44\003""55\003""66\003""77\003""88\003""99\003""0";
  bout.nl();
  bout << "|#9Enter |#2/S|#9 at the start of a blank line to save your email.\r\n";
  bout << "|#9Enter |#2/Q|#9 to quote previous message, |#2/HELP|#9 for other editor commands.\r\n";

  bout.Color(7);
  std::string header = "[---=----=----=----=----=----=----=----]----=----=----=----=----=----=----=----]";
  if (a()->user()->screen_width() < 80) {
    header.resize(a()->user()->screen_width());
  }
  bout << header;
  bout.nl();

  std::string current_line;
  auto check_message_size = true;
  auto save_message = false;
  auto done = false;
  std::string rollover_line;
  std::vector<std::string> quoted_lines;
  while (!done && !a()->sess().hangup()) {
    while (inli(&current_line, &rollover_line, 160, true, (curli > 0))) {
      // returning true means we back spaced past the current line.
      // intuitive eh?
      curli--;
      if (curli >= 0) {
        // Don't keep retreating past line 0.
        rollover_line = lin.at(curli);
        if (ssize(rollover_line) > a()->user()->screen_width() - 1) {
          rollover_line.resize(a()->user()->screen_width() - 2);
        }
      } else {
        curli = 0;
      }
    }
    if (a()->sess().hangup()) {
      done = true;
    }
    check_message_size = true;
    if (current_line[0] == '/') {
      auto cmd = current_line;
      StringUpperCase(&cmd);
      if (cmd == "/HELP" ||
          cmd == "/H" ||
          cmd == "/?") {
        check_message_size = false;
        bout.print_help_file(EDITOR_NOEXT);
      } else if ((cmd == "/QUOTE") ||
                 (cmd == "/Q")) {
        check_message_size = false;
        quoted_lines = query_quote_lines(a()->sess());

        if (quoted_lines.empty()) {
          bout.nl();          
        }
        for (const auto& ql  : quoted_lines) {
          bout.bpla(ql, &abort);
          if (curli == ssize(lin)) {
            // we're inserting a new line at the end.
            lin.emplace_back(ql);
          } else if (curli < ssize(lin)) {
            // replacing an older line.
            lin.at(curli).assign(ql);
          }
          curli++;
        } 
      } else if (cmd == "/LI") {
        check_message_size = false;
        abort = false;
        for (auto i = 0; i < curli && !abort; i++) {
          auto line = lin.at(i);
          if (!line.empty() && line.back() == CA) {
            line.pop_back();
          }
          if (!line.empty() && line.front() == CB) {
            line = line.substr(1);
            auto i5 = 0;
            // TODO(rushfan): Make a utility function to do this in strings.h
            for (auto i4 = 0; i4 < ssize(line); i4++) {
              if (line[i4] == 8 || line[i4] == 3) {
                --i5;
              } else {
                ++i5;
              }
            }
            for (auto i4 = 0; (i4 < (a()->user()->screen_width() - i5) / 2) && (!abort); i4++) {
              bout.bputs(" ", &abort, &next);
            }
          }
          bout.bpla(line, &abort);
        }
        if (!okansi() || next) {
          bout.nl();
          bout << "Continue...\r\n";
        }
      } else if (cmd == "/ES" ||
                 cmd == "/S") {
        save_message = true;
        done = true;
        check_message_size = false;
      } else if (cmd == "/ESY" ||
                 cmd == "/SY") {
        save_message = true;
        done = true;
        check_message_size = false;
        *setanon = 1;
      } else if (cmd == "/ESN" ||
                 cmd == "/SN") {
        save_message = true;
        done = true;
        check_message_size = false;
        *setanon = -1;
      } else if (cmd == "/ABT" ||
                 cmd == "/A") {
        done = true;
        check_message_size = false;
      } else if (cmd == "/CLR") {
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
        data.title = bin.input_text(60);
        bout << "Continue...\r\n\n";
      }
      if (cmd.length() > 3) {
        cmd.resize(3);
      }
      if (cmd == "/C:") {
        const auto centered_text = current_line.substr(3);
        current_line = fmt::sprintf("%c%s", CB, centered_text);
      } else if (cmd == "/SU" && current_line[3] == '/' && curli > 0) {
        auto old_string = current_line.substr(4);
        auto slash = old_string.find('/');
        if (slash != std::string::npos) {
          auto new_string = old_string.substr(slash + 1);
          old_string.resize(slash);
          // We've already advanced curli to point to the new row, so we need
          // to back up one.
          auto last_line = curli - 1;
          auto temp = lin.at(last_line);
          StringReplace(&temp, old_string, new_string);
          lin[last_line] = temp;
          bout << "Last line:\r\n" << lin.at(last_line) << "\r\nContinue...\r\n";
        }
        check_message_size = false;
      }
    }

    if (check_message_size) {
      if (curli == ssize(lin)) {
        // we're inserting a new line at the end.
        lin.emplace_back(current_line);
      } else if (curli < ssize(lin)) {
        // replacing an older line.
        lin.at(curli).assign(current_line);
      }
      // Since we added the line, let's move forward.
      curli++;
      if (curli == maxli + 1) {
        bout << "\r\n-= No more lines, last line lost =-\r\n/S to save\r\n\n";
        curli--;
      } else if (curli == maxli) {
        bout << "-= Message limit reached, /S to save =-\r\n";
      } else if (curli + 5 == maxli) {
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


static void UpdateMessageBufferInReplyToInfo(std::ostringstream& ss, bool is_email, const std::string& to_name, const std::string& title) {
  auto fido_addr_added{false};
  if (!to_name.empty() && !is_email && !a()->current_sub().nets.empty()) {
    for (const auto& xnp : a()->current_sub().nets) {
      if (a()->nets()[xnp.net_num].type == network_type_t::ftn) {
        const auto buf = fmt::sprintf("%c0FidoAddr: %s", CD, to_name);
        ss << buf << crlf;
        fido_addr_added = true;
        break;
      }
    }
  }

  if (!is_email) {
    // I don't think either RE: or BY lines should show up in email messages.
    // Also the message SDK only has support for adding them to subs, not emails too.
    if (!title.empty() && title.front() != '"' && !fido_addr_added) {
      // Don't add RE: line if we have a "^D0FidoAddr" line.
      ss << "RE: " << title << crlf;
    }
    if (!to_name.empty()) {
      ss << "BY: " << to_name << crlf;
    }
    ss << crlf;
  }
}

static std::filesystem::path FindTagFileName() {
  for (const auto& xnp : a()->current_sub().nets) {
    const auto& nd = a()->nets()[xnp.net_num].dir;
    auto filename = FilePath(nd, StrCat(xnp.stype, ".tag"));
    if (File::Exists(filename)) {
      return filename;
    }
    filename = FilePath(nd, GENERAL_TAG);
    if (File::Exists(filename)) {
      return filename;
    }
    filename = FilePath(a()->config()->datadir(), StrCat(xnp.stype, ".tag"));
    if (File::Exists(filename)) {
      return filename;
    }
    filename = FilePath(a()->config()->datadir(), GENERAL_TAG);
    if (File::Exists(filename)) {
      return filename;
    }
  }
  return {};
}

static void UpdateMessageBufferTagLine(std::ostringstream& ss, bool is_email, const std::string& to_name) {
  if (a()->subs().subs().empty() && a()->sess().GetCurrentReadMessageArea() <= 0) {
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
    // FindTagFileName returns an empty string if no tag name exists, so
    // just exit here since there is no tag.
    return;
  }
  TextFile file(filename, "rb");
  if (file.IsOpen()) {
    int j = 0;
    std::string s;
    do {
      s.clear();
      const auto line_read = file.ReadLine(&s);
      if (line_read && s.length() > 1 && s[s.length() - 2] == RETURN) {
        // remove last 2 characters.
        s.pop_back();
        s.pop_back();
      }
      auto s1 = s;
      if (s[0] != CD) {
        s1 = fmt::sprintf("%c%c%s", CD, j + '2', s);
      }
      if (!j) {
        ss << fmt::sprintf("%c1", CD) << crlf;
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
  const auto quotes_filename = FilePath(a()->sess().dirs().temp_directory(), QUOTES_TXT);
  TextFile file(quotes_filename, "rt");
  if (file.IsOpen()) {
    std::string quote_text;
    while (file.ReadLine(&quote_text)) {
      const auto slash_n = quote_text.find('\n');
      if (slash_n != std::string::npos) {
        quote_text.resize(slash_n);
      }
      if (starts_with(quote_text, "\004""0U")) {
        ss << quote_text << crlf;
      }
    }
    file.Close();
  }

  const auto msginf_filename = FilePath(a()->sess().dirs().temp_directory(), "msginf");
  File::Copy(quotes_filename, msginf_filename);
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
      bout.Left(80);
      bout.clreol();
      bout << "|#5Anonymous? ";
      if (bin.yesno()) {
        *anony = anony_sender;
      } else {
        *anony = 0;
      }
    }
    break;
  case anony_enable_dear_abby: {
    bout.nl();
    bout << "1. " << a()->user()->name_and_number() << wwiv::endl;
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

static std::string ostringstream_to_wwivtext(const std::ostringstream& b) {
  auto text = b.str();
  if (text.back() != CZ) {
    text.push_back(CZ);
  }
  return text;
}

bool inmsg(MessageEditorData& data) {

  const auto oiia = setiia(std::chrono::milliseconds(0));
  if (data.fsed_flags != FsedFlags::NOFSED && !okfsed()) {
    data.fsed_flags = FsedFlags::NOFSED;
  }

  const auto exted_filename = FilePath(a()->sess().dirs().temp_directory(), INPUT_MSG);
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

  ScopeExit at_exit([=]() {
    setiia(oiia);
    // Might not need to do this anymore since quoting
    // isn't so convoluted.
    bin.charbufferpointer_ = 0;
    bin.charbuffer[0] = '\0';
    clear_quotes(a()->sess());
    File::Remove(exted_filename);
  });

  bout.nl();

  if (data.to_name.empty()) {
    if (GetMessageToName(data)) {
      bout.nl();
    }
  } else {
    bout << "|#2To   : ";
    bout.mpl(40);
    bout << data.to_name << "\r\n";
  }

  GetMessageTitle(data);
  if (data.title.empty() && data.need_title) {
    bout << "|#6Aborted.\r\n";
    return false;
  }

  std::vector<std::string> lin;
  int setanon = 0;
  auto save_message = false;
  auto maxli = GetMaxMessageLinesAllowed();
  if (data.fsed_flags == FsedFlags::NOFSED) {   // Use Internal Message Editor
    save_message = InternalMessageEditor(lin, maxli, &setanon, data);
  } else if (data.fsed_flags == FsedFlags::FSED) {   // Use Full Screen Editor
    if (ok_internal_fsed()) {
      if (File::Exists(exted_filename)) {
        // We have auto-quote or a file loaded into the workspace.
        TextFile editor_file(exted_filename, "r");
        lin = editor_file.ReadFileIntoVector();
      }
      save_message = wwiv::fsed::fsed(a()->context(), lin, maxli, data, false);
    } else {
      save_message = DoExternalMessageEditor(data, maxli, &setanon);
      TextFile editor_file(exted_filename, "r");
      lin = editor_file.ReadFileIntoVector();
    }
  } else if (data.fsed_flags == FsedFlags::WORKSPACE) { // "auto-send mail message"
    use_workspace = false;
    save_message = File::Exists(exted_filename);
    if (save_message) {
      if (!data.silent_mode) {
        bout << "Reading in file...\r\n";
      }
      TextFile editor_file(exted_filename, "r");
      lin = editor_file.ReadFileIntoVector();
    }
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
    b << a()->user()->real_name() << crlf;
  } else if (data.silent_mode) {
    b << a()->config()->sysop_name() << " #1" << crlf;
  } else {
    b << a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum) << crlf;
  }

  // Add date to message body
  b << daten_to_wwivnet_time(daten_t_now()) << crlf;
  UpdateMessageBufferQuotesCtrlLines(b);

  if (!data.title.empty()) {
    UpdateMessageBufferInReplyToInfo(b, data.is_email(), data.to_name, data.title);
  }

  // TODO(rushfan): This and the date above, etc. Will need to be transformed
  // into structured data when moving to a proper message API.
  //
  // New in 5.x. Add PID to all message types. This will be for WWIV messages
  // as well as FTN messages so we know the Program ID (WWIV) that created
  // the message.
  const auto control_d_zero = fmt::sprintf("%c0", CD);
  b << control_d_zero << "PID: WWIV " << full_version() << crlf;

  // iterate through the lines in "lin" and append them to 'b'
  for (const auto& l : lin) {
    b << l << crlf;
  }

  if (a()->HasConfigFlag(OP_FLAGS_MSG_TAG)) {
    UpdateMessageBufferTagLine(b, data.is_email(), data.to_name);
  }

  data.text = ostringstream_to_wwivtext(b);
  return true;
}
