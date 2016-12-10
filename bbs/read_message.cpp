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
#include "bbs/read_message.h"

#include <iostream>
#include <iterator>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/message_file.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/subacc.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/net.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_utils_wwiv.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::stl;
using namespace wwiv::strings;


/**
* Sets the global variables pszOutOriginStr and pszOutOriginStr2.
* Note: This is a private function
*/
static void SetMessageOriginInfo(int system_number, int user_number, string* outNetworkName,
  string* outLocation) {
  string netName;

  if (a()->max_net_num() > 1) {
    netName = StrCat(a()->current_net().name, "- ");
  }

  outNetworkName->clear();
  outLocation->clear();

  if (a()->current_net().type == network_type_t::internet) {
    outNetworkName->assign("Internet Mail and Newsgroups");
    return;
  }

  if (system_number && a()->current_net().type == network_type_t::wwivnet) {
    net_system_list_rec *csne = next_system(system_number);
    if (csne) {
      string netstatus;
      if (user_number == 1) {
        if (csne->other & other_net_coord) {
          netstatus = "{NC}";
        } else if (csne->other & other_group_coord) {
          netstatus = StringPrintf("{GC%d}", csne->group);
        } else if (csne->other & other_area_coord) {
          netstatus = "{AC}";
        }
      }
      const string filename = StringPrintf(
        "%s%s%c%s.%-3u",
        a()->config()->datadir().c_str(),
        REGIONS_DIR,
        File::pathSeparatorChar,
        REGIONS_DIR,
        atoi(csne->phone));

      string description;
      if (File::Exists(filename)) {
        // Try to use the town first.
        const string phone_prefix = StringPrintf("%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
        description = describe_area_code_prefix(atoi(csne->phone), atoi(phone_prefix.c_str()));
      }
      if (description.empty()) {
        // Try area code if we still don't have a description.
        description = describe_area_code(atoi(csne->phone));
      }

      *outNetworkName = StrCat(netName, csne->name, " [", csne->phone, "] ", netstatus.c_str());
      *outLocation = (!description.empty()) ? description : "Unknown Area";
    } else {
      *outNetworkName = StrCat(netName, "Unknown System");
      *outLocation = "Unknown Area";
    }
  }
}

void display_message_text(const std::string& text, bool *next) {
  int nNumCharsPtr = 0;
  int nLineLenPtr = 0;
  int ctrld = 0;
  bool done = false;
  bool printit = false;
  bool ctrla = false; 
  bool centre = false;
  bool abort = false;
  bool ansi = false;
  char s[205];

  bout.nl();
  for (char ch : text) {
    if (done || abort) {
      break;
    }
    if (ch == CZ) {
      done = true;
    } else {
      if (ch != SOFTRETURN) {
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
                if (10 - (a()->user()->GetOptionalVal()) < static_cast<unsigned>(ch - '0')) {
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
          if (g_flags & g_flag_ansi_movement) {
            g_flags &= ~g_flag_ansi_movement;
            bout.clear_lines_listed();
            if (a()->localIO()->GetTopLine() && a()->localIO()->GetScreenBottom() == 24) {
              a()->ClearTopScreenProtection();
            }
          }
          s[nNumCharsPtr++] = ch;
          if (ch == CC || ch == BACKSPACE) {
            --nLineLenPtr;
          } else {
            ++nLineLenPtr;
          }
        }

        if (printit || ansi || nLineLenPtr >= 80) {
          if (centre && (ctrld != -1)) {
            int nSpacesToCenter = 
              (a()->user()->GetScreenChars() - bout.wherex() - nLineLenPtr) / 2;
            osan(charstr(nSpacesToCenter, ' '), &abort, next);
          }
          if (nNumCharsPtr) {
            if (ctrld != -1) {
              if ((bout.wherex() + nLineLenPtr >= a()->user()->GetScreenChars())
                && !centre && !ansi) {
                bout.nl();
              }
              s[nNumCharsPtr] = '\0';
              osan(s, &abort, next);
              if (ctrla && s[nNumCharsPtr - 1] != SPACE && !ansi) {
                if (bout.wherex() < a()->user()->GetScreenChars() - 1) {
                  bout.bputch(SPACE);
                  bout.nl();
                } else {
                  bout.nl();
                }
                checka(&abort, next);
              }
            }
            nLineLenPtr = 0;
            nNumCharsPtr = 0;
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
      } else {
        ctrld = 0;
      }
    }

  }

  if (!abort && nNumCharsPtr) {
    s[nNumCharsPtr] = '\0';
    bout << s;
    bout.nl();
  }
  bout.Color(0);
  bout.nl();
  if (express && abort && !*next) {
    expressabort = true;
  }
  if (ansi && a()->topdata && a()->IsUserOnline()) {
    a()->UpdateTopScreen();
  }
  g_flags &= ~g_flag_disable_mci;
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

  Type2MessageData data;
  if (!readfile(msg, file_name, &data.message_text)) {
    return{};
  }

  // TODO(rushfan): Use get_control_line from networking code here.

  size_t ptr = 0;
  for (ptr = 0; 
    ptr <data.message_text.size() && data.message_text[ptr] != RETURN && ptr <= 200;
    ptr++) {
    data.from_user_name.push_back(data.message_text[ptr]);
  }
  if (ptr < data.message_text.size() && data.message_text[++ptr] == SOFTRETURN) {
    ++ptr;
  }
  for (size_t start = ptr; ptr < data.message_text.size() && data.message_text[ptr] != RETURN && ptr - start <= 60; ptr++) {
    data.date.push_back(data.message_text[ptr]);
  }
  if (ptr + 1 < data.message_text.size()) {
    // skip trailing \r\n
    while (ptr + 1 < data.message_text.size() && (data.message_text[ptr] == '\r' || data.message_text[ptr] == '\n')) {
      ptr++;
    }
    data.message_text = data.message_text.substr(ptr);
    ptr = 0;
  }
  
  auto lines = SplitString(data.message_text, "\r");
  for (auto line : lines) {
    StringTrim(&line);
    if (line.empty()) continue;
    if (starts_with(line, "\004""0FidoAddr:")) {
      string cl = line.substr(12);
      if (!cl.empty()) {
        data.to_user_name = cl;
      }
    }
  }

  if (!data.message_text.empty() && data.message_text.back() == CZ) {
    data.message_text.pop_back();
  }

  // Also remove any BY: line if we have a To user name.
  irt_name[0] = '\0';

  UpdateHeaderInformation(an, readit, data.from_user_name, &data.from_user_name, &data.date);
  if (an == 0) {
    g_flags &= ~g_flag_disable_mci;
    SetMessageOriginInfo(from_sys_num, from_user, &data.from_sys_name, &data.from_sys_loc);
    to_char_array(irt_name, data.from_user_name);
  }

  return data;
}

struct MessageHeaderInfo {
  int num_lines = 0;
};

static MessageHeaderInfo display_type2_message_header(Type2MessageData& msg) {
  static constexpr int COLUMN2 = 42;
  MessageHeaderInfo info{};

  if (msg.message_number > 0 && msg.total_messages > 0 && !msg.message_area.empty()) {
    string msgarea = msg.message_area;
    if (msgarea.size() > 35) { msgarea = msgarea.substr(0, 35); }
    bout << "|#9 Sub|#7: |#" << a()->GetMessageColor() << msgarea;
    if (a()->user()->GetScreenChars() >= 78) {
      int used = 6 + msgarea.size();
      auto pad = COLUMN2 - used;
      bout << string(pad, ' ');
    }
    else {
      bout.nl();
      info.num_lines++;
    }
    bout << "|#9Msg#|#7: ";
    if (msg.message_number > 0 && msg.total_messages > 0) {
      bout << "[|#" << a()->GetMessageColor() << msg.message_number
        << "|#7 of |#" << a()->GetMessageColor()
        << msg.total_messages << "|#7]";
    }
    bout.nl();
    info.num_lines++;
  }

  string from = msg.from_user_name;
  if (from.size() > 35) {
    from = from.substr(0, 35);
  }
  bout << "|#9From|#7: |#1" << from;
  if (a()->user()->GetScreenChars() >= 78) {
    int used = 6 + from.size();
    auto pad = COLUMN2 - used;
    bout << string(pad, ' ');
  }
  else {
    bout.nl();
    info.num_lines++;
  }
  bout << "|#9Date|#7: |#1" << msg.date << wwiv::endl;
  info.num_lines++;
  if (!msg.to_user_name.empty()) {
    bout << "  |#9To|#7: |#1" << msg.to_user_name << wwiv::endl;
    info.num_lines++;
  }
  bout << "|#9Subj|#7: |#" << a()->GetMessageColor() << msg.title << wwiv::endl;
  info.num_lines++;

  auto sysname = msg.from_sys_name;
  if (!msg.from_sys_name.empty()) {
    if (sysname.size() > 35) {
      sysname = sysname.substr(0, 35);
    }
    bout << "|#9 Sys|#7: |#1" << sysname;
    if (a()->user()->GetScreenChars() >= 78) {
      int used = 6 + sysname.size();
      auto pad = COLUMN2 - used;
      bout << string(pad, ' ');
    }
    else {
      bout.nl();
      info.num_lines++;
    }
    if (!msg.from_sys_loc.empty()) {
      auto loc = msg.from_sys_loc;
      int maxlen = a()->user()->GetScreenChars() - 7 - COLUMN2;
      if (size_int(loc) > maxlen) {
        loc = loc.substr(0, maxlen);
      }
      bout << "|#9Loc|#7:  |#1" << loc << wwiv::endl;
      info.num_lines++;
    }
  }

  if (!msg.flags.empty()) {
    bout << "|#9Info|#7: |#1";
    for (const auto& f : msg.flags) {
      switch (f) {
      case MessageFlags::FORCED: bout << "|13[FORCED] "; break;
      case MessageFlags::NOT_NETWORK_VALIDATED: bout << "|12[Not Network Validated] "; break;
      case MessageFlags::NOT_VALIDATED: bout << "|12<<< NOT VALIDATED >>>"; break;
      case MessageFlags::PERMANENT: bout << "|13[Permanent] "; break;
      case MessageFlags::LOCAL: bout << "|10[Local] "; break;
      case MessageFlags::FTN: bout << "|10[Fido] "; break;
      case MessageFlags::PRIVATE: bout << "|10[Pvt] "; break;
      case MessageFlags::WWIVNET: bout << "|10[WWIVnet] "; break;
      }
    }
    bout.nl();
    info.num_lines++;
  }

  return info;
}

static std::string pad(int screen_width, std::string::size_type line_len) {
  if (static_cast<int>(line_len) >= screen_width) {
    return{};
  }
  return std::string(screen_width - line_len, ' ');
}

static std::vector<std::string> split_wwiv_message(const std::string& text) {
  std::vector<std::string> orig_lines = SplitString(text, "\n");
  std::vector<std::string> lines;
  for (auto line : orig_lines) {
    StringTrim(&line);
    //if (line.empty()) { continue; }
    if (contains(line, '\r')) {
      for (auto l : SplitString(line, "\r")) {
        StringTrim(&l);
        if (line.front() == '\x04') { continue; }
        lines.emplace_back(l);
      }
    }
    else {
      if (!line.empty() && line.front() == '\x04') {
        // skip contril lines.
        continue;
      }
      lines.emplace_back(line);
    }
  }
  return lines;
}

static void display_message_text_new(const std::vector<std::string>& lines, int start, 
  int message_height, int screen_width, int lines_start) {
  for (int i = start; i < start + message_height; i++) {
    if (i >= size_int(lines)) {
      break;
    }
    bout.GotoXY(1, i - start + lines_start);
    auto l = lines.at(i);
    if (!l.empty()) {
      if (l.back() == CA) {
        // A line ending in ^A means it soft-wrapped.
        l.pop_back();
      }
      if (l.front() == CB) {
        // Line starting with ^B is centered.
        l = StrCat(std::string((screen_width - l.size()) / 2, ' '), l);
      }
    }
    bout << "|#0" << l << pad(screen_width, l.size());
  }

}

static ReadMessageResult display_type2_message_new(Type2MessageData& msg, char an, bool* next) {
  g_flags &= ~g_flag_ansi_movement;
  *next = false;
  g_flags |= g_flag_disable_mci;
  if (an == 0) {
    g_flags &= ~g_flag_disable_mci;
  }

  bout.cls();
  const auto info = display_type2_message_header(msg);
  const int screen_width = a()->user()->GetScreenChars();
  const int screen_length = a()->user()->GetScreenLines() - 1;
  const int message_height = screen_length - info.num_lines - 2 - 1;
  const int lines_start = info.num_lines + 2;
  const int lines_end = lines_start + message_height;
  const int command_line = screen_length;

  auto lines = split_wwiv_message(msg.message_text);

  bout.GotoXY(1, info.num_lines + 1);
  bout << "|#7" << static_cast<char>(198) << string(screen_width - 3, static_cast<char>(205)) << static_cast<char>(181);
  bout.GotoXY(1, screen_length - 1);
  bout << "|#7" << static_cast<char>(198) << string(screen_width - 3, static_cast<char>(205)) << static_cast<char>(181);

  bout.GotoXY(1, info.num_lines + 2);
  const int first = 0;
  const int last = std::max<int>(0, lines.size() - message_height);

  int start = first;
  bool done = false;
  bout.Color(0);
  ReadMessageResult result{};
  result.lines_start = lines_start;
  result.lines_end = lines_end;
  while (!done) {
    CheckForHangup();
    
    display_message_text_new(lines, start, message_height, screen_width, lines_start);
    bout.GotoXY(1, command_line);
    bout << "|#9(|#2Q|#9=Quit, |#2?|#9=Help): ";

    int key = bgetch_event(numlock_status_t::NOTNUMBERS);
    switch (key) {
    case COMMAND_UP: {
      if (start > first) {
        --start;
      }
    } break;
    case COMMAND_PAGEUP: {
      if (start - message_height > first) {
        start -= message_height;
      } else {
        start = first;
      }
    } break;
    case COMMAND_HOME: {
      start = first;
    } break;
    case COMMAND_DOWN: {
      if (start < last) {
        ++start;
      }
    } break;
    case COMMAND_PAGEDN: {
      if (start + message_height < last) {
        start += message_height;
      } else {
        start = last;
      }
    } break;
    case COMMAND_END: { 
      start = last;
    } break;
    case COMMAND_RIGHT: {
      result.option = ReadMessageOption::NEXT_MSG;
      return result;
    } break;
    case COMMAND_LEFT: {
      result.option = ReadMessageOption::PREV_MSG;
      return result;
    } break;
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
      else if (start + message_height < last) {
        start += message_height;
      }
      else {
        start = last;
      }
    } break;
    default: {
      if ((key & 0xff) == key) {
        key = toupper(key & 0xff);
        if (key == ']') {
          result.option = ReadMessageOption::NEXT_MSG;
        }
        else if (key == '[') {
          result.option = ReadMessageOption::PREV_MSG;
        }
        else if (key == 'J') {
          result.option = ReadMessageOption::JUMP_TO_MSG;
        } 
        else if (key == 'T') {
          result.option = ReadMessageOption::LIST_TITLES;
        } else if (key == '?') {
          for (int y = lines_start; y < lines_end; y++) {
            bout.GotoXY(1, y);
            bout.clreol();
          }
          bout.GotoXY(1, lines_start);
          if (!printfile(MBFSED_NOEXT)) {
            bout.GotoXY(1, command_line);
            bout.clreol();
            bout << "|#6Unable to find file: " << MBFSED_NOEXT;
          }
          else {
            bout.GotoXY(1, command_line);
            bout.clreol();
          }
          if (lcs()) {
            pausescr();
            if (!printfile(MBFSED_SYSOP_NOEXT)) {
              bout.GotoXY(1, command_line);
              bout.clreol();
              bout << "|#6Unable to find file: " << MBFSED_SYSOP_NOEXT;
            }
          }
          pausescr();
          bout.GotoXY(1, command_line);
          bout.clreol();
        } else {
          result.option = ReadMessageOption::COMMAND;
          result.command = static_cast<char>(key);
        }
        bout.GotoXY(1, command_line);
        bout.clreol();
        return result;
      }
    }
    }
  }

  bout.GotoXY(1, command_line);
  bout.clreol();
  return result;
}

ReadMessageResult display_type2_message(Type2MessageData& msg, char an, bool* next) {
  if (a()->experimental_read_prompt() && a()->user()->HasStatusFlag(User::fullScreenReader)) {
    return display_type2_message_new(msg, an, next);
  }

  g_flags &= ~g_flag_ansi_movement;
  *next = false;
  g_flags |= g_flag_disable_mci;
  if (an == 0) {
    g_flags &= ~g_flag_disable_mci;
  }


  auto info = display_type2_message_header(msg);
  bout.lines_listed_ = info.num_lines;

  display_message_text(msg.message_text, next);
  g_flags &= ~g_flag_disable_mci;

  ReadMessageResult result{};
  result.lines_start = 1;
  result.lines_end = a()->user()->GetScreenLines() - 1;
  result.option = ReadMessageOption::NONE;
  return result;
}

static void update_qscan(uint32_t qscan) {
  if (qscan > qsc_p[a()->GetCurrentReadMessageArea()]) {
    qsc_p[a()->GetCurrentReadMessageArea()] = qscan;
  }

  uint32_t current_qscan_pointer = 0;
  {
    std::unique_ptr<WStatus> wwiv_status(a()->status_manager()->GetStatus());
    // not sure why we check this twice...
    // maybe we need a getCachedQScanPointer?
    current_qscan_pointer = wwiv_status->GetQScanPointer();
  }
  if (qscan >= current_qscan_pointer) {
    a()->status_manager()->Run([&](WStatus& s) {
      if (qscan >= s.GetQScanPointer()) {
        s.SetQScanPointer(qscan + 1);
      }
    });
  }
}

ReadMessageResult read_post(int n, bool *next, int *val) {
  if (a()->user()->IsUseClearScreen()) {
    bout.cls();
  } else {
    bout.nl();
  }
  bool abort = false;
  *next = false;

  postrec p = *get_post(n);
  bool bReadit = (lcs() || (getslrec(a()->GetEffectiveSl()).ability & ability_read_post_anony)) ? true : false;
  auto m = read_type2_message(&(p.msg), static_cast<char>(p.anony & 0x0f), bReadit,
    a()->current_sub().filename.c_str(), p.ownersys, p.owneruser);

  if (forcescansub) {
    m.flags.insert(MessageFlags::FORCED);
  }

  m.message_number = n;
  m.total_messages = a()->GetNumMessagesInCurrentMessageArea();
  m.message_area = a()->current_sub().name;

  if (a()->current_sub().nets.empty()) {
    m.flags.insert(MessageFlags::LOCAL);
  }
  for (const auto& nets : a()->current_sub().nets) {
    const auto& net = a()->net_networks[nets.net_num];
    if (net.type == network_type_t::ftn) {
      m.flags.insert(MessageFlags::FTN);
    }
    else if (net.type == network_type_t::wwivnet) {
      m.flags.insert(MessageFlags::WWIVNET);
    }
  }

  if (p.status & (status_unvalidated | status_delete)) {
    if (!lcs()) {
      return{};
    }
    m.flags.insert(MessageFlags::NOT_VALIDATED);
    *val |= 1;
  }
  m.title = p.title;
  strncpy(irt, p.title, 60);
  irt_name[0] = '\0';

  if ((p.status & status_no_delete) && lcs()) {
    m.flags.insert(MessageFlags::PERMANENT);
  }
  if ((p.status & status_pending_net) && a()->user()->GetSl() > syscfg.newusersl) {
    *val |= 2;
    m.flags.insert(MessageFlags::NOT_NETWORK_VALIDATED);
  }
  ReadMessageResult result{};
  if (!abort) {
    int saved_net_num = a()->net_num();

    if (p.status & status_post_new_net) {
      set_net_num(network_number_from(&p));
    }
    result = display_type2_message(m, static_cast<char>(p.anony & 0x0f), next);

    if (saved_net_num != a()->net_num()) {
      set_net_num(saved_net_num);
    }

    a()->user()->SetNumMessagesRead(a()->user()->GetNumMessagesRead() + 1);
    a()->SetNumMessagesReadThisLogon(a()->GetNumMessagesReadThisLogon() + 1);
  } else if (express && !*next) {
    expressabort = true;
  }

  update_qscan(p.qscan);
  return result;
}
