/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/read_message.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/full_screen.h"
#include "bbs/message_file.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/subacc.h"
#include "bbs/utility.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_utils_wwiv.h"
#include "sdk/net.h"
#include "sdk/subxtr.h"
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::ansi;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::stl;
using namespace wwiv::strings;

// N.B: I can't imagine any good reason for this, but I ifdef'ed
// out the old behavior that's been here since forever
// with this guard.  I'll probably remove it later unless
// I hear some good reason to leave it.  Also note that the
// Message SDK doesn't have this logic.
// #define UPDATE_SYSTEM_QSCAN_PTR_ON_ADVANCED_POST_POINTER

/**
 * Sets the global variables pszOutOriginStr and pszOutOriginStr2.
 * Note: This is a private function
 */
static void SetMessageOriginInfo(int system_number, int user_number, string* outNetworkName,
                                 string* outLocation) {
  string netName;

  if (wwiv::stl::ssize(a()->nets()) > 1) {
    netName = StrCat(a()->current_net().name, "- ");
  }

  outNetworkName->clear();
  outLocation->clear();

  if (a()->current_net().type == network_type_t::internet ||
      a()->current_net().type == network_type_t::news) {
    outNetworkName->assign("Internet Mail and Newsgroups");
    return;
  }

  if (system_number && a()->current_net().type == network_type_t::wwivnet) {
    auto* csne = next_system(system_number);
    if (csne) {
      string netstatus;
      if (user_number == 1) {
        if (csne->other & other_net_coord) {
          netstatus = "{NC}";
        } else if (csne->other & other_group_coord) {
          netstatus = fmt::format("{GC{}}", csne->group);
        } else if (csne->other & other_area_coord) {
          netstatus = "{AC}";
        }
      }
      const auto phone_fn =
          fmt::sprintf("%s.%-3u", REGIONS_DIR, to_number<unsigned int>(csne->phone));
      const auto regions_dir = FilePath(a()->config()->datadir(), REGIONS_DIR);
      const auto filename = FilePath(regions_dir, phone_fn);

      string description;
      if (File::Exists(filename)) {
        // Try to use the town first.
        const auto phone_prefix =
            fmt::sprintf("%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
        description = describe_area_code_prefix(to_number<int>(csne->phone),
                                                to_number<int>(phone_prefix));
      }
      if (description.empty()) {
        // Try area code if we still don't have a description.
        description = describe_area_code(to_number<int>(csne->phone));
      }

      *outNetworkName = StrCat(netName, csne->name, " [", csne->phone, "] ", netstatus);
      *outLocation = (!description.empty()) ? description : "Unknown Area";
    } else {
      *outNetworkName = StrCat(netName, "Unknown System");
      *outLocation = "Unknown Area";
    }
  }
}

void display_message_text(const std::string& text, bool* next) {
  auto line_len_ptr = 0;
  auto ctrld = 0;
  auto printit = false;
  auto ctrla = false;
  auto centre = false;
  auto abort = false;
  auto ansi = false;
  std::string s;
  s.reserve(160);

  bout.nl();
  for (auto ch : text) {
    if (ch == CZ || abort) {
      break;
    }
    if (ch == SOFTRETURN) {
      ctrld = 0;
      continue;
    }
    if (ch == RETURN || !ch) {
      printit = true;
    } else if (ch == CA) {
      ctrla = true;
    } else if (ch == CB) {
      centre = true;
    } else if (ch == CD) {
      ctrld = 1;
    } else if (ctrld == 1) {
      if (ch >= '0' && ch <= '9') {
        if (ch == '0') {
          ctrld = -1; // don't display
        } else {
          if (a()->user()->GetOptionalVal() == 0) {
            ctrld = 0; // display
          } else {
            if (10 - a()->user()->GetOptionalVal() < static_cast<int>(ch - '0')) {
              ctrld = -1; // don't display
            } else {
              ctrld = 0; // display
            }
          }
        }
      } else {
        ctrld = 0; // ctrl-d and non-numeric
      }
    } else {
      if (ch == ESC) {
        ansi = true;
      }
      if (bout.ansi_movement_occurred()) {
        bout.clear_ansi_movement_occurred();
        bout.clear_lines_listed();
        if (a()->localIO()->GetTopLine() && a()->localIO()->GetScreenBottom() == 24) {
          a()->ClearTopScreenProtection();
        }
      }
      s.push_back(ch);
      if (ch == CC || ch == BACKSPACE) {
        --line_len_ptr;
      } else {
        ++line_len_ptr;
      }
    }

    if (printit || ansi || line_len_ptr >= 80) {
      if (centre && ctrld != -1) {
        const auto spaces_to_center = (a()->user()->GetScreenChars() - bout.wherex() - line_len_ptr) / 2;
        bout.bputs(std::string(spaces_to_center, ' '), &abort, next);
      }
      if (!s.empty()) {
        if (ctrld != -1) {
          if (bout.wherex() + line_len_ptr >= static_cast<int>(a()->user()->GetScreenChars()) &&
              !centre && !ansi) {
            bout.nl();
          }
          bout.bputs(s, &abort, next);
          if (ctrla && !s.empty() && s.back() != SPACE && !ansi) {
            if (bout.wherex() < static_cast<int>(a()->user()->GetScreenChars()) - 1) {
              bout.bputch(SPACE);
            } 
            bout.nl();
            checka(&abort, next);
          }
        }
        line_len_ptr = 0;
        s.clear();
      }
      centre = false;
    }
    if (ch == RETURN) {
      if (ctrla == false) {
        if (ctrld != -1) {
          bout.nl();
          checka(&abort, next);
        }
      } else {
        ctrla = false;
      }
      if (printit) {
        ctrld = 0;
      }
    }
    printit = false;
  }

  //
  // After the for loop processing character by character
  //

  if (!abort && !s.empty()) {
    bout << s;
    bout.nl();
  }
  bout.Color(0);
  bout.nl();
  if (ansi && a()->topdata && a()->IsUserOnline()) {
    a()->UpdateTopScreen();
  }
  a()->mci_enabled_ = false;
}

static void UpdateHeaderInformation(int8_t anon_type, bool readit, const string default_name,
                                    string* name, string* date) {
  switch (anon_type) {
  case anony_sender:
    if (readit) {
      *name = StrCat("<<< ", default_name, " >>>");
    } else {
      *name = ">UNKNOWN<";
    }
    break;
  case anony_sender_da:
  case anony_sender_pp:
    *date = ">UNKNOWN<";
    if (anon_type == anony_sender_da) {
      *name = "Abby";
    } else {
      *name = "Problemed Person";
    }
    if (readit) {
      *name = StrCat("<<< ", default_name, " >>>");
    }
    break;
  default:
    *name = default_name;
    break;
  }
}

Type2MessageData read_type2_message(messagerec* msg, char an, bool readit, const char* file_name,
                                    int from_sys_num, int from_user) {

  Type2MessageData data{};
  data.email = iequals("email", file_name);
  if (!readfile(msg, file_name, &data.message_text)) {
    return {};
  }
  // Make a copy of the raw message text.
  data.raw_message_text = data.message_text;

  // TODO(rushfan): Use get_control_line from networking code here.

  size_t ptr;
  for (ptr = 0; ptr < data.message_text.size() && data.message_text[ptr] != RETURN && ptr <= 200;
       ptr++) {
    data.from_user_name.push_back(data.message_text[ptr]);
  }
  if (ptr < data.message_text.size() && data.message_text[++ptr] == SOFTRETURN) {
    ++ptr;
  }
  for (const auto start = ptr;
       ptr < data.message_text.size() && data.message_text[ptr] != RETURN && ptr - start <= 60;
       ptr++) {
    data.date.push_back(data.message_text[ptr]);
  }
  if (ptr + 1 < data.message_text.size()) {
    // skip trailing \r\n
    while (ptr + 1 < data.message_text.size() &&
           (data.message_text[ptr] == '\r' || data.message_text[ptr] == '\n')) {
      ptr++;
    }
    try {
      data.message_text = data.message_text.substr(ptr);
    } catch (const std::out_of_range& e) {
      LOG(ERROR) << "Error getting message_text: " << e.what();
    }
    // ptr = 0;
  }

  auto lines = SplitString(data.message_text, "\r");
  for (auto line : lines) {
    StringTrim(&line);
    if (line.empty()) {
      continue;
    }
    if (starts_with(line, "\004" "0FidoAddr: ") && line.size() > 12) {
      auto cl = line.substr(12);
      if (!cl.empty()) {
        data.to_user_name = cl;
        break;
      }
    }
  }

  if (!data.message_text.empty() && data.message_text.back() == CZ) {
    data.message_text.pop_back();
  }

  UpdateHeaderInformation(an, readit, data.from_user_name, &data.from_user_name, &data.date);
  if (an == 0) {
    a()->mci_enabled_ = false;
    SetMessageOriginInfo(from_sys_num, from_user, &data.from_sys_name, &data.from_sys_loc);
  }

  data.message_anony = an;
  return data;
}

static FullScreenView display_type2_message_header(Type2MessageData& msg) {
  const auto oldcuratr = bout.curatr();
  static constexpr const auto COLUMN2 = 42;
  auto num_header_lines = 0;

  if (msg.message_number > 0 && msg.total_messages > 0 && !msg.message_area.empty()) {
    auto msgarea = msg.message_area;
    if (msgarea.size() > 35) {
      msgarea = msgarea.substr(0, 35);
    }
    bout << "|#9 Sub|#7: ";
    bout.Color(a()->GetMessageColor());
    bout << msgarea;
    if (a()->user()->GetScreenChars() >= 78) {
      const auto pad = COLUMN2 - (6 + msgarea.size());
      bout << string(pad, ' ');
    } else {
      bout.nl();
      num_header_lines++;
    }
    bout << "|#9Msg#|#7: ";
    if (msg.message_number > 0 && msg.total_messages > 0) {
      bout << "[";
      bout.Color(a()->GetMessageColor());
      bout << msg.message_number << "|#7 of ";
      bout.Color(a()->GetMessageColor());
      bout << msg.total_messages << "|#7]";
    }
    bout.nl();
    num_header_lines++;
  }

  auto from = msg.from_user_name;
  if (from.size() > 35) {
    from = from.substr(0, 35);
  }
  bout << "|#9From|#7: |#1" << from;
  if (a()->user()->GetScreenChars() >= 78) {
    const int used = ssize(from) + 6;
    const auto pad = COLUMN2 - used;
    bout << string(pad, ' ');
  } else {
    bout.nl();
    num_header_lines++;
  }
  bout << "|#9Date|#7: |#1" << msg.date << wwiv::endl;
  num_header_lines++;
  if (!msg.to_user_name.empty()) {
    bout << "  |#9To|#7: |#1" << msg.to_user_name << wwiv::endl;
    num_header_lines++;
  }
  bout << "|#9Subj|#7: ";
  bout.Color(a()->GetMessageColor());
  bout << msg.title << wwiv::endl;
  num_header_lines++;

  auto sysname = msg.from_sys_name;
  if (!msg.from_sys_name.empty()) {
    if (sysname.size() > 35) {
      sysname = sysname.substr(0, 35);
    }
    bout << "|#9 Sys|#7: |#1" << sysname;
    if (a()->user()->GetScreenChars() >= 78) {
      const auto used = 6 + ssize(sysname);
      const auto pad = COLUMN2 - used;
      bout << string(pad, ' ');
    } else {
      bout.nl();
      num_header_lines++;
    }
    if (!msg.from_sys_loc.empty()) {
      auto loc = msg.from_sys_loc;
      const int maxlen = a()->user()->GetScreenChars() - 7 - COLUMN2;
      if (ssize(loc) > maxlen) {
        loc = loc.substr(0, maxlen);
      }
      bout << "|#9Loc|#7:  |#1" << loc << wwiv::endl;
      num_header_lines++;
    }
  }

  if (!msg.flags.empty()) {
    bout << "|#9Info|#7: |#1";
    for (const auto& f : msg.flags) {
      switch (f) {
      case MessageFlags::FORCED:
        bout << "|13[FORCED] ";
        break;
      case MessageFlags::NOT_NETWORK_VALIDATED:
        bout << "|12[Not Network Validated] ";
        break;
      case MessageFlags::NOT_VALIDATED:
        bout << "|12<<< NOT VALIDATED >>>";
        break;
      case MessageFlags::PERMANENT:
        bout << "|13[Permanent] ";
        break;
      case MessageFlags::LOCAL:
        bout << "|10[Local] ";
        break;
      case MessageFlags::FTN:
        bout << "|10[Fido] ";
        break;
      case MessageFlags::PRIVATE:
        bout << "|10[Pvt] ";
        break;
      case MessageFlags::WWIVNET:
        bout << "|10[WWIVnet] ";
        break;
      }
    }
    bout.nl();
    num_header_lines++;
  }

  const auto screen_width = a()->user()->GetScreenChars();
  const auto screen_length = a()->user()->GetScreenLines() - 1;

  bout.curatr(oldcuratr);
  return FullScreenView(bout, num_header_lines, screen_width, screen_length);
}

static std::vector<std::string> split_long_lines(std::string& orig_text, int width, bool add_wrapping_marker) {
  auto orig = SplitString(orig_text, "\r");
  std::vector<std::string> out;
  for (auto& l : orig) {
    StringTrimCRLF(&l);
    l.erase(std::remove(l.begin(), l.end(), 10), l.end());

    do {
      const auto size_wc = size_without_colors(l);
      if (size_wc <= width) {
        out.push_back(l);
        break;
      }
      // We have a long line
      auto pos = width;
      while (pos > 0 && l[pos] > 32) {
        pos--;
      }
      if (pos == 0) {
        pos = width;
      }
      auto subset_of_l = l.substr(0, pos);
      l = l.substr(pos + 1);
      if (add_wrapping_marker) {
        // A ^A at the end of the line means it was soft wrapped.
        out.push_back(fmt::sprintf("%s%c", subset_of_l, 0x01));
      } else {
        out.push_back(subset_of_l);
      }
    } while (true);
    
  }
  return out;
}

static std::vector<std::string> split_wwiv_message(const std::string& orig_text, bool controlcodes) {
  auto text(orig_text);
  const auto& user = *a()->user();
  const auto cz_pos = text.find(CZ);
  if (cz_pos != string::npos) {
    // We stop the message at control-Z if it exists.
    text = text.substr(0, cz_pos);
  }

  // split text into lines of appropriate length.
  auto orig_lines = split_long_lines(text, user.GetScreenChars(), false);

  // Now handle control chars, and optional lines.
  std::vector<std::string> lines;
  std::string overflow;
  for (auto line : orig_lines) {
    if (line.empty()) {
      lines.emplace_back("");
      continue;
    }
    const auto optional_lines = user.GetOptionalVal();
    if (line.front() == CD) {
      const auto level = (line.size() > 1) ? static_cast<int>(line.at(1) - '0') : 0;
      if (level == 0) {
        // ^D0 lines are always skipped unless explicitly requested.
        if (!controlcodes) {
          continue;
        }
        line = StrCat("@", line.substr(2));
      } else {
        line = line.substr(2);
      }
      if (optional_lines != 0 && line.size() >= 2 &&
          static_cast<int>(10 - optional_lines) < level) {
        // This is too high of a level, so skip it.
        continue;
      }
    } else if (line.back() == CA) {
      // This meant we wrapped here.
      line.pop_back();
    }
    // Not CD or CA.  Check overflow.
    lines.emplace_back(line);
  }

  // Finally render it to the frame buffer to interpret
  // heart codes, ansi, etc
  FrameBuffer b{80};
  Ansi ansi(&b, {}, 0x07);
  HeartCodeFilter heart(&ansi, user.colors());
  for (auto& l : lines) {
    for (const auto c  : l) {
      heart.write(c);
    }
    ansi.write("\n");
  }
  b.close();
  return b.to_screen_as_lines();
}

static void display_message_text_new(const std::vector<std::string>& lines, int start,
                                     int message_height, int screen_width, int lines_start) {
  auto had_ansi = false;
  for (auto i = start; i < start + message_height; i++) {
    // Do this so we don't pop up a pause for sure.
    bout.clear_lines_listed();

    if (i >= ssize(lines)) {
      break;
    }
    bout.GotoXY(1, i - start + lines_start);
    auto l = lines.at(i);
    if (l.find("\x1b[") != string::npos) {
      had_ansi = true;
    }
    if (!l.empty() && l.back() == CA) {
      // A line ending in ^A means it soft-wrapped.
      l.pop_back();
    }
    if (!l.empty() && l.front() == CB) {
      // Line starting with ^B is centered.
      if (ssize(stripcolors(l)) >= screen_width) {
        // TODO(rushfan): This should be stripped size
        l = l.substr(1, screen_width);
      } else {
        l = StrCat(std::string((screen_width - stripcolors(l).size()) / 2, ' '), l.substr(1));
      }
    }
    bout << (had_ansi ? "|16" : "|#0") << l;
    bout.clreol();
  }
}

static ReadMessageResult display_type2_message_new(Type2MessageData& msg, char an, bool* next) {
  // Reset the color before displaying a message.
  bout.curatr(7);
  bout.clear_ansi_movement_occurred();
  *next = false;
  a()->mci_enabled_ = an != 0;

  bout.cls();
  auto fs = display_type2_message_header(msg);

  const auto controlcodes = a()->user()->HasStatusFlag(User::msg_show_controlcodes);
  const auto lines = split_wwiv_message(msg.message_text, controlcodes);

  fs.DrawTopBar();
  fs.DrawBottomBar("");

  fs.GotoContentAreaTop();
  const auto first = 0;
  const auto last = std::max<int>(0, lines.size() - fs.message_height());

  auto start = first;
  auto dirty = true;
  ReadMessageResult result{};
  result.lines_start = fs.lines_start();
  result.lines_end = fs.lines_end();
  for ( ;; ) {
    CheckForHangup();

    if (dirty) {
      bout.Color(0);
      display_message_text_new(lines, start, fs.message_height(), fs.screen_width(),
                               fs.lines_start());
      dirty = false;
      fs.DrawBottomBar(start == last ? "END" : "");
      fs.ClearCommandLine();
      bout.PutsXY(1, fs.command_line_y(), "|#9(|#2Q|#9=Quit, |#2?|#9=Help): ");
    }
    if (!msg.use_msg_command_handler) {
      result.option = ReadMessageOption::NEXT_MSG;
      return result;
    }
    auto key = bgetch_event(numlock_status_t::NOTNUMBERS, [&](bgetch_timeout_status_t st, int s) {
      if (st == bgetch_timeout_status_t::WARNING) {
        fs.PrintTimeoutWarning(s);
      } else if (st == bgetch_timeout_status_t::CLEAR) {
        fs.ClearCommandLine();
      }
    });
    switch (key) {
    case COMMAND_UP: {
      if (start > first) {
        --start;
        dirty = true;
      }
    } break;
    case COMMAND_PAGEUP: {
      if (start - fs.message_height() > first) {
        start -= fs.message_height();
        dirty = true;
      } else {
        dirty = true;
        start = first;
      }
    } break;
    case COMMAND_HOME: {
      start = first;
    } break;
    case COMMAND_DOWN: {
      if (start < last) {
        dirty = true;
        ++start;
      }
    } break;
    case COMMAND_PAGEDN: {
      if (start + fs.message_height() < last) {
        dirty = true;
        start += fs.message_height();
      } else if (start != last) {
        // If start already == last, don't dirty the screen.
        dirty = true;
        start = last;
      }
    } break;
    case COMMAND_END: {
      if (start != last) {
        start = last;
        dirty = true;
      }
    } break;
    case COMMAND_RIGHT: {
      result.option = ReadMessageOption::NEXT_MSG;
      return result;
    }
    case COMMAND_LEFT: {
      result.option = ReadMessageOption::PREV_MSG;
      return result;
    }
    case SOFTRETURN: {
      // Do nothing. SyncTerm sends CRLF on enter, not just CR
      // like we get from the local terminal. So we'll ignore the
      // SOFTRETURN (10, aka LF).
    } break;
    case RETURN: {
      if (start == last) {
        result.option = ReadMessageOption::NEXT_MSG;
        return result;
      }
      if (start + fs.message_height() < last) {
        dirty = true;
        start += fs.message_height();
      } else if (start != last) {
        dirty = true;
        start = last;
      }
    } break;
    default: {
      if ((key & 0xff) == key) {
        key = toupper(key & 0xff);
        if (key == ']') {
          result.option = ReadMessageOption::NEXT_MSG;
        } else if (key == '[') {
          result.option = ReadMessageOption::PREV_MSG;
        } else if (key == 'J') {
          result.option = ReadMessageOption::JUMP_TO_MSG;
        } else if (key == 'T') {
          result.option = ReadMessageOption::LIST_TITLES;
        } else if (key == 'K') {
          result.option = ReadMessageOption::NONE;
          a()->user()->ToggleStatusFlag(User::msg_show_controlcodes);
        } else if (key == '?') {
          result.option = ReadMessageOption::NONE;
          fs.ClearMessageArea();
          if (!print_help_file(MBFSED_NOEXT)) {
            fs.ClearCommandLine();
            bout << "|#6Unable to find file: " << MBFSED_NOEXT;
          } else {
            fs.ClearCommandLine();
          }
          if (lcs()) {
            pausescr();
            fs.ClearMessageArea();
            if (!print_help_file(MBFSED_SYSOP_NOEXT)) {
              fs.ClearCommandLine();
              bout << "|#6Unable to find file: " << MBFSED_SYSOP_NOEXT;
            }
          }
          fs.ClearCommandLine();
          pausescr();
          fs.ClearCommandLine();
        } else {
          result.option = ReadMessageOption::COMMAND;
          result.command = static_cast<char>(key);
        }
        fs.ClearCommandLine();
        return result;
      }
    }
    }
  }
}

void display_type2_message_old_impl(Type2MessageData& msg, bool* next) {
  const auto info = display_type2_message_header(msg);
  bout.lines_listed_ = info.num_header_lines();
  bout.clear_ansi_movement_occurred();
  *next = false;
  a()->mci_enabled_ = true;
  if (msg.message_anony == 0) {
    a()->mci_enabled_ = false;
  }
  display_message_text(msg.message_text, next);
  a()->mci_enabled_ = false;
}

ReadMessageResult display_type2_message(Type2MessageData& msg, bool* next) {
  const auto fsreader_enabled =
      a()->fullscreen_read_prompt() && a()->user()->HasStatusFlag(User::fullScreenReader);
  const auto skip_fs_reader_per_sub = (msg.subboard_flags & anony_no_fullscreen) != 0;
  if (fsreader_enabled && !skip_fs_reader_per_sub && !msg.email) {
    // N.B.: We don't use the full screen reader for email yet since
    // It does not work.  Need to figure out how to rearrange email
    // reading so it does work.
    return display_type2_message_new(msg, static_cast<char>(msg.message_anony), next);
  }

  display_type2_message_old_impl(msg, next);

  ReadMessageResult result{};
  result.lines_start = 1;
  result.lines_end = a()->user()->GetScreenLines() - 1;
  result.option = ReadMessageOption::NONE;
  return result;
}

static void update_qscan(uint32_t qscan) {
  if (qscan > a()->context().qsc_p[a()->GetCurrentReadMessageArea()]) {
    a()->context().qsc_p[a()->GetCurrentReadMessageArea()] = qscan;
  }

#ifdef UPDATE_SYSTEM_QSCAN_PTR_ON_ADVANCED_POST_POINTER
  uint32_t current_qscan_pointer = 0;
  {
    auto status = a()->status_manager()->GetStatus();
    // not sure why we check this twice...
    // maybe we need a getCachedQScanPointer?
    current_qscan_pointer = status->GetQScanPointer();
  }
  if (qscan >= current_qscan_pointer) {
    a()->status_manager()->Run([&](WStatus& s) {
      if (qscan >= s.GetQScanPointer()) {
        s.SetQScanPointer(qscan + 1);
      }
    });
  }
#endif // UPDATE_SYSTEM_QSCAN_PTR_ON_ADVANCED_POST_POINTER
}

ReadMessageResult read_post(int n, bool* next, int* val) {
  if (a()->user()->IsUseClearScreen()) {
    bout.cls();
  } else {
    bout.nl();
  }
  const bool abort = false;
  *next = false;

  auto p = *get_post(n);
  const auto read_it =
      (lcs() || (a()->effective_slrec().ability & ability_read_post_anony)) ? true : false;
  const auto& cs = a()->current_sub();
  auto m = read_type2_message(&(p.msg), static_cast<char>(p.anony & 0x0f), read_it,
                              cs.filename.c_str(), p.ownersys, p.owneruser);
  m.subboard_flags = cs.anony;
  if (a()->context().forcescansub()) {
    m.flags.insert(MessageFlags::FORCED);
  }

  m.message_number = n;
  m.total_messages = a()->GetNumMessagesInCurrentMessageArea();
  m.message_area = cs.name;

  if (cs.nets.empty()) {
    m.flags.insert(MessageFlags::LOCAL);
  }
  for (const auto& nets : cs.nets) {
    const auto& net = a()->nets()[nets.net_num];
    if (net.type == network_type_t::ftn) {
      m.flags.insert(MessageFlags::FTN);
    } else if (net.type == network_type_t::wwivnet) {
      m.flags.insert(MessageFlags::WWIVNET);
    }
  }

  if (p.status & (status_unvalidated | status_delete)) {
    if (!lcs()) {
      return {};
    }
    m.flags.insert(MessageFlags::NOT_VALIDATED);
    *val |= 1;
  }
  m.title = p.title;
  strncpy(a()->context().irt_, p.title, 60);

  if ((p.status & status_no_delete) && lcs()) {
    m.flags.insert(MessageFlags::PERMANENT);
  }
  if ((p.status & status_pending_net) &&
      a()->user()->GetSl() > a()->config()->newuser_sl()) {
    *val |= 2;
    m.flags.insert(MessageFlags::NOT_NETWORK_VALIDATED);
  }
  ReadMessageResult result{};
  if (!abort) {
    const auto saved_net_num = a()->net_num();

    if (p.status & status_post_new_net) {
      set_net_num(network_number_from(&p));
    }
    result = display_type2_message(m, next);

    if (saved_net_num != a()->net_num()) {
      set_net_num(saved_net_num);
    }

    a()->user()->SetNumMessagesRead(a()->user()->GetNumMessagesRead() + 1);
    a()->SetNumMessagesReadThisLogon(a()->GetNumMessagesReadThisLogon() + 1);
  }

  update_qscan(p.qscan);
  return result;
}
