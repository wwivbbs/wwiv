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
#include "bbs/read_message.h"

#include "message_find.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/connect1.h"
#include "bbs/message_file.h"
#include "bbs/subacc.h"
#include "bbs/utility.h"
#include "common/full_screen.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk/fido/fido_util.h"
#include "sdk/msgapi/message_utils_wwiv.h"
#include "sdk/msgapi/parsed_message.h"
#include "sdk/net/net.h"
#include "sdk/net/networks.h"

#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
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
 * Returns (network name, network location) the outNetworkName and outLocation.
 */
static void UpdateMessageOriginInfo(int system_number, int user_number, Type2MessageData& data) {
  data.from_sys_loc.clear();
  data.from_sys_name.clear();

  auto& net = a()->mutable_current_net();
  if (net.type == network_type_t::internet ||
      net.type == network_type_t::news) {
    data.from_sys_name = "Internet Mail and Newsgroups";
  }

  if (net.type == network_type_t::ftn) {
    // TODO(rushfan): here's where we should try to get it from the bbslist.
    if (try_load_nodelist(net)) {
      auto& nl = *net.nodelist;
      auto addr =
          system_number == 0
              ? fido::try_parse_fidoaddr(net.fido.fido_address).value_or(fido::FidoAddress())
              : fido::get_address_from_origin(data.message_text);
      if (addr.zone() == -1) {
        addr = fido::get_address_from_single_line(data.from_user_name);
        if (addr.zone() == -1) {
          addr = fido::get_address_from_single_line(data.from_user_name);
        }
      }
      if (addr.point() != 0) {
        // Hack to drop points and report parent.
        if (auto o = fido::try_parse_fidoaddr(to_zone_net_node(addr))) {
          addr = o.value();
        }
      }
      if (nl.contains(addr)) {
        const auto& e = nl.entry(addr);
        data.from_sys_name = e.name_;
        data.from_sys_loc = e.location_;
        return;
      }
    }
    data.from_sys_name = net.name;
    data.from_sys_loc = "Unknown FTN Location";
    return;
  }

  if (net.type == network_type_t::wwivnet) {
    if (system_number == 0) {
      system_number = net.sysnum;
    }
    if (auto csne = next_system(system_number)) {
      std::string netstatus;
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

      std::string description;
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
      data.from_sys_name = fmt::format("{} {}", csne->name, netstatus);
      data.from_sys_loc = description;
      return;
    }
    data.from_sys_name = "Unknown System";
    data.from_sys_loc = "";
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
          if (a()->user()->optional_val() == 0) {
            ctrld = 0; // display
          } else {
            if (10 - a()->user()->optional_val() < static_cast<int>(ch - '0')) {
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
        const auto spaces_to_center = (a()->user()->screen_width() - bout.wherex() - line_len_ptr) / 2;
        bout.bputs(std::string(spaces_to_center, ' '), &abort, next);
      }
      if (!s.empty()) {
        if (ctrld != -1) {
          if (bout.wherex() + line_len_ptr >= static_cast<int>(a()->user()->screen_width()) &&
              !centre && !ansi) {
            bout.nl();
          }
          bout.bputs(s, &abort, next);
          if (ctrla && !s.empty() && s.back() != SPACE && !ansi) {
            if (bout.wherex() < static_cast<int>(a()->user()->screen_width()) - 1) {
              bout.bputch(SPACE);
            } 
            bout.nl();
            bin.checka(&abort, next);
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
          bin.checka(&abort, next);
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
  if (ansi && a()->localIO()->topdata() != LocalIO::topdata_t::none &&
      a()->sess().IsUserOnline()) {
    a()->UpdateTopScreen();
  }
  bout.disable_mci();
}

static void UpdateHeaderInformation(int8_t anon_type, bool readit, const std::string default_name,
                                    std::string* name, std::string* date) {
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

Type2MessageData read_type2_message(messagerec* msg, uint8_t an, bool readit, const std::string& file_name,
                                    int from_sys_num, int from_user) {

  Type2MessageData data{};
  data.email = iequals("email", file_name);
  auto o = readfile(msg, file_name);
  if (!o) {
    return {};
  }
  // Make a copy of the raw message text.
  data.message_text = o.value();
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
      if (auto cl = line.substr(12); !cl.empty()) {
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
    bout.disable_mci();
    UpdateMessageOriginInfo(from_sys_num, from_user, data);
  }

  data.message_anony = an;
  return data;
}

static int legacy_display_header_text(Type2MessageData& msg) {
  auto num_header_lines = 0;
  if (msg.message_number > 0 && msg.total_messages > 0 && !msg.message_area.empty()) {
    bout.format("|#9 Sub|#7: |#{}{:<35.35}", a()->GetMessageColor(), msg.message_area);
    if (a()->user()->screen_width() >= 78) {
      bout << " ";
    } else {
      bout.nl();
      num_header_lines++;
    }
    if (msg.message_number > 0 && msg.total_messages > 0) {
      bout.format("|#9Msg#|#7: [|#{}{}|#7 of |#{}{}|#7]", a()->GetMessageColor(),
                  msg.message_number, a()->GetMessageColor(), msg.total_messages);
    }
    bout.nl();
    num_header_lines++;
  }

  bout.format("|#9From|#7: |#1{:<35.35}", msg.from_user_name);
  if (a()->user()->screen_width() >= 78) {
    bout << " ";
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
  bout.format("|#9Subj|#7: |#{}{}\r\n", a()->GetMessageColor(), msg.title);
  num_header_lines++;

  if (!msg.from_sys_name.empty()) {
    bout.format("|#9 Sys|#7: |#1{:<35.35}", msg.from_sys_name);
    if (a()->user()->screen_width() >= 78) {
      bout << " ";
    } else {
      bout.nl();
      num_header_lines++;
    }
    if (!msg.from_sys_loc.empty()) {
      bout.format(" |#9Loc|#7: |#1{:<30}\r\n", msg.from_sys_loc);
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
  return num_header_lines;
}

static std::tuple<bool, int> display_header_file(Type2MessageData& msg) {
  std::map<std::string, std::string> m;
  m["message_area"] = trim_to_size(msg.message_area, 35);
  m["message_number"] = std::to_string(msg.message_number);
  m["total_messages"] = std::to_string(msg.total_messages);
  m["from"] =trim_to_size(msg.from_user_name, 35);
  m["date"] = msg.date;
  m["to"] = (!msg.to_user_name.empty()) ? msg.to_user_name : "All";
  m["title"] = msg.title;
  m["sys"] = trim_to_size(msg.from_sys_name, 35);
  m["loc"] = trim_to_size(msg.from_sys_loc, 35);
  a()->context().add_context_variable("msg", m);

  std::map<std::string, std::string> flags;
  // Set defaults.
  flags["forced"] = "false";
  flags["not_net_val"] = "false";
  flags["not_val"] = "false";
  flags["permanent"] = "false";
  flags["local"] = "false";
  flags["ftn"] = "false";
  flags["private"] = "false";
  flags["wwivnet"] = "false";
  for (const auto& f : msg.flags) {
    switch (f) {
    case MessageFlags::FORCED:
      flags["forced"] = "true";
      break;
    case MessageFlags::NOT_NETWORK_VALIDATED:
      flags["not_net_val"] = "true";
      break;
    case MessageFlags::NOT_VALIDATED:
      flags["not_val"] = "true";
      break;
    case MessageFlags::PERMANENT:
      flags["permanent"] = "true";
      break;
    case MessageFlags::LOCAL:
      flags["local"] = "true";
      break;
    case MessageFlags::FTN:
      flags["ftn"] = "true";
      break;
    case MessageFlags::PRIVATE:
      flags["private"] = "true";
      break;
    case MessageFlags::WWIVNET:
      flags["wwivnet"] = "true";
      break;
    }
  }
  a()->context().add_context_variable("msg.flags", flags);

  // TODO(rushfan): Make these always be available like user?
  const auto& net = a()->current_net();
  std::map<std::string, std::string> n;
  n["name"] = net.name;
  if (net.type == network_type_t::ftn) {
    n["node"] = net.fido.fido_address;
  }
  n["node"] = std::to_string(net.sysnum);
  a()->context().add_context_variable("net", n);

  const auto saved_mci_enabled = bout.mci_enabled();
  ScopeExit at_exit([=] {
    a()->context().clear_context_variables();
    bout.set_mci_enabled(saved_mci_enabled);
  });
  bout.set_mci_enabled(true);
  if (bout.printfile("fs_msgread")) {
    auto num_header_lines = 5;
    if (const auto iter = a()->context().return_values().find("num_header_lines");
        iter != std::end(a()->context().return_values())) {
      num_header_lines = to_number<int>(iter->second);
    }
    return std::make_tuple(true, num_header_lines);
  }
  return std::make_tuple(false, 5);
}

static FullScreenView display_type2_message_header(Type2MessageData& msg, bool allow_header_file) {
  const auto oldcuratr = bout.curatr();

  int num_header_lines = 5;
  if (allow_header_file) {
    auto [displayed, nh] = display_header_file(msg);
    if (!displayed) {
      nh = legacy_display_header_text(msg);
    }
    num_header_lines = nh;
  } else {
    num_header_lines = legacy_display_header_text(msg);
  }

  const auto screen_width = a()->user()->screen_width();
  const auto screen_length = a()->user()->screen_lines() - 1;

  bout.SystemColor(oldcuratr);
  return FullScreenView(bout, bin, num_header_lines, screen_width, screen_length);
}

static std::vector<std::string> split_wwiv_message(const std::string& orig_text, bool controlcodes) {
  const auto& user = *a()->user();
  WWIVParsedMessageText pmt(orig_text);
  parsed_message_lines_style_t style{};
  style.ctrl_lines = control_lines_t::control_lines;
  style.add_wrapping_marker = false;
  style.line_length = user.screen_width() - 1;
  const auto orig_lines = pmt.to_lines(style);

  // Now handle control chars, and optional lines.
  std::vector<std::string> lines;
  std::string overflow;
  for (auto line : orig_lines) {
    if (line.empty()) {
      lines.emplace_back("");
      continue;
    }
    const auto optional_lines = user.optional_val();
    if (line.front() == CD) {
      const auto level = line.size() > 1 ? line.at(1) - '0' : 0;
      if (level == 0) {
        // ^D0 lines are always skipped unless explicitly requested.
        if (!controlcodes) {
          continue;
        }
        line = StrCat("|08@", line.substr(2));
      } else {
        line = line.substr(2);
      }
      if (optional_lines != 0 && line.size() >= 2 && 10 - optional_lines < level) {
        // This is too high of a level, so skip it.
        continue;
      }
    }
    // Not CD.  Check overflow.
    lines.emplace_back(line);
  }

  // Finally render it to the frame buffer to interpret
  // heart codes, ansi, etc
  FrameBuffer b{80};
  Ansi ansi(&b, {}, 0x07);
  HeartAndPipeCodeFilter heart(&ansi, user.colors());
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
    if (l.find("\x1b[") != std::string::npos) {
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
    bout.Color(0);
    bout << (had_ansi ? "|16" : "|#0") << l;
    bout.clreol();
  }
}

static std::string percent_read(int start, int end) {
  const auto d = static_cast<float>(start) / static_cast<float>(end);
  return fmt::format("{:0.0f}%", d * 100);
}

static ReadMessageResult display_type2_message_new(int& msgno, Type2MessageData& msg, char an, bool* next) {
  // Reset the color before displaying a message.
  bout.SystemColor(7);
  bout.clear_ansi_movement_occurred();
  *next = false;
  bout.set_mci_enabled(an != 0);

  bout.cls();
  auto fs = display_type2_message_header(msg, true);

  const auto controlcodes = a()->user()->has_flag(User::msg_show_controlcodes);
  const auto lines = split_wwiv_message(msg.message_text, controlcodes);

  fs.DrawTopBar();
  fs.DrawBottomBar("");

  fs.GotoContentAreaTop();
  const auto first = 0;
  const auto last = std::max<int>(0, size_int(lines) - fs.message_height());

  auto start = first;
  auto dirty = true;
  ReadMessageResult result{};
  result.lines_start = fs.lines_start();
  result.lines_end = fs.lines_end();
  for ( ;; ) {
    a()->CheckForHangup();

    if (dirty) {
      bout.Color(0);
      display_message_text_new(lines, start, fs.message_height(), fs.screen_width(),
                               fs.lines_start());
      dirty = false;
      fs.DrawBottomBar(start == last ? "END" : percent_read(start, last));
      fs.ClearCommandLine();
      fs.PutsCommandLine("|#9(|#2Q|#9=Quit, |#2?|#9=Help): ");
    }
    if (!msg.use_msg_command_handler) {
      result.option = ReadMessageOption::NEXT_MSG;
      return result;
    }
    auto key = bin.bgetch_event(wwiv::common::Input::numlock_status_t::NOTNUMBERS,
                         [&](wwiv::common::Input::bgetch_timeout_status_t st, int s) {
                              if (st == wwiv::common::Input::bgetch_timeout_status_t::WARNING) {
        fs.PrintTimeoutWarning(s);
                              } else if (st ==
                                         wwiv::common::Input::bgetch_timeout_status_t::CLEAR) {
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
        } else if (key == 'F' && msgno != -1) {
          const auto r = wwiv::bbs::FindNextMessageFS(fs,msgno);
          if (r.found) {
            msgno = r.msgnum;
          }
          result.option = ReadMessageOption::READ_MESSAGE;
        } else if (key == 'J') {
          result.option = ReadMessageOption::JUMP_TO_MSG;
        } else if (key == 'T') {
          result.option = ReadMessageOption::LIST_TITLES;
        } else if (key == 'K') {
          result.option = ReadMessageOption::NONE;
          a()->user()->toggle_flag(User::msg_show_controlcodes);
        } else if (key == '?') {
          result.option = ReadMessageOption::NONE;
          fs.ClearMessageArea();
          if (!bout.print_help_file(MBFSED_NOEXT)) {
            fs.ClearCommandLine();
            bout << "|#6Unable to find file: " << MBFSED_NOEXT;
          } else {
            fs.ClearCommandLine();
          }
          if (lcs()) {
            bout.pausescr();
            fs.ClearMessageArea();
            if (!bout.print_help_file(MBFSED_SYSOP_NOEXT)) {
              fs.ClearCommandLine();
              bout << "|#6Unable to find file: " << MBFSED_SYSOP_NOEXT;
            }
          }
          fs.ClearCommandLine();
          bout.pausescr();
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
  const auto info = display_type2_message_header(msg, false);
  bout.lines_listed(info.num_header_lines());
  bout.clear_ansi_movement_occurred();
  *next = false;
  bout.enable_mci();
  if (msg.message_anony == 0) {
    bout.disable_mci();
  }
  display_message_text(msg.message_text, next);
  bout.disable_mci();
}

ReadMessageResult display_type2_message(int& msgno, Type2MessageData& msg, bool* next) {
  const auto fsreader_enabled =
      a()->fullscreen_read_prompt() && a()->user()->has_flag(User::fullScreenReader);
  const auto skip_fs_reader_per_sub = (msg.subboard_flags & anony_no_fullscreen) != 0;
  if (fsreader_enabled && !skip_fs_reader_per_sub && !msg.email) {
    // N.B.: We don't use the full screen reader for email yet since
    // It does not work.  Need to figure out how to rearrange email
    // reading so it does work.
    return display_type2_message_new(msgno, msg, static_cast<char>(msg.message_anony), next);
  }

  display_type2_message_old_impl(msg, next);

  ReadMessageResult result{};
  result.lines_start = 1;
  result.lines_end = a()->user()->screen_lines() - 1;
  result.option = ReadMessageOption::NONE;
  return result;
}

static void update_qscan(uint32_t qscan) {
  if (qscan > a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()]) {
    a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()] = qscan;
  }

#ifdef UPDATE_SYSTEM_QSCAN_PTR_ON_ADVANCED_POST_POINTER
  uint32_t current_qscan_pointer = 0;
  {
    auto status = a()->status_manager()->get_status();
    // not sure why we check this twice...
    // maybe we need a getCachedQScanPointer?
    current_qscan_pointer = status->qscanptr();
  }
  if (qscan >= current_qscan_pointer) {
    a()->status_manager()->Run([&](Status& s) {
      if (qscan >= s.qscanptr()) {
        s.qscanptr(qscan + 1);
      }
    });
  }
#endif // UPDATE_SYSTEM_QSCAN_PTR_ON_ADVANCED_POST_POINTER
}

ReadMessageResult read_post(int& msgnum, bool* next, int* val) {
  if (a()->user()->clear_screen()) {
    bout.cls();
  } else {
    bout.nl();
  }
  const auto abort = false;
  *next = false;

  auto p = *get_post(msgnum);
  const auto read_it =
      (lcs() || (a()->config()->sl(a()->sess().effective_sl()).ability & ability_read_post_anony)) ? true : false;
  const auto& cs = a()->current_sub();
  auto m = read_type2_message(&p.msg, p.anony & 0x0f, read_it,
                              cs.filename.c_str(), p.ownersys, p.owneruser);
  m.subboard_flags = cs.anony;
  if (a()->sess().forcescansub()) {
    m.flags.insert(MessageFlags::FORCED);
  }

  m.message_number = msgnum;
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

  ReadMessageResult result{};
  if (p.status & (status_unvalidated | status_delete)) {
    if (!lcs()) {
      result.option = ReadMessageOption::NEXT_MSG;
      return result;
    }
    m.flags.insert(MessageFlags::NOT_VALIDATED);
    *val |= 1;
  }
  m.title = p.title;
  strncpy(a()->sess().irt_, p.title, 60);

  if ((p.status & status_no_delete) && lcs()) {
    m.flags.insert(MessageFlags::PERMANENT);
  }
  if ((p.status & status_pending_net) &&
      a()->user()->sl() >= a()->config()->validated_sl()) {
    *val |= 2;
    m.flags.insert(MessageFlags::NOT_NETWORK_VALIDATED);
  }
  if (!abort) {
    const auto saved_net_num = a()->net_num();

    if (p.status & status_post_new_net) {
      set_net_num(network_number_from(&p));
    }
    result = display_type2_message(msgnum, m, next);

    if (saved_net_num != a()->net_num()) {
      set_net_num(saved_net_num);
    }

    a()->user()->messages_read(a()->user()->messages_read() + 1);
    a()->SetNumMessagesReadThisLogon(a()->GetNumMessagesReadThisLogon() + 1);
  }

  update_qscan(p.qscan);
  return result;
}
