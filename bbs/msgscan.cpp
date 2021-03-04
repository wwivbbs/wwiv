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
#include "bbs/msgscan.h"


#include "message_find.h"
#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/extract.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/msgbase1.h"
#include "bbs/read_message.h"
#include "bbs/showfiles.h"
#include "bbs/sr.h"
#include "bbs/subacc.h"
#include "bbs/sublist.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/full_screen.h"
#include "common/input.h"
#include "common/output.h"
#include "common/quote.h"
#include "common/workspace.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/message.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::endl;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::stl;
using namespace wwiv::strings;

static string GetScanReadPrompts(int msg_num) {
  if (a()->sess().forcescansub()) {
    if (msg_num < a()->GetNumMessagesInCurrentMessageArea()) {
      return "|#1Press |#7[|#2ENTER|#7]|#1 to go to the next message...";
    }
    return "|#1Press |#7[|#2ENTER|#7]|#1 to continue...";
  }

  string local_network_name = "Local";
  if (!a()->current_sub().nets.empty()) {
    local_network_name = a()->network_name();
  } else {
    set_net_num(0);
  }
  const auto sub_name_prompt = fmt::sprintf(
      "|#7[|#1%s|#7] [|#2%s|#7]", local_network_name, a()->current_sub().name);
  return fmt::sprintf("%s |#7(|#1Read |#2%d |#1of |#2%d|#1|#7) : ", sub_name_prompt,
                      msg_num, a()->GetNumMessagesInCurrentMessageArea());
}

static void HandleScanReadAutoReply(int& msgnum, const char* user_input,
                                    MsgScanOption& scan_option) {
  auto* post = get_post(msgnum);
  if (!post) {
    return;
  }
  if (!lcs() && post->status & (status_unvalidated | status_delete)) {
    return;
  }
  string reply_to_name;
  if (post->ownersys && !post->owneruser) {
    reply_to_name = grab_user_name(&post->msg, a()->current_sub().filename, a()->net_num());
  }

  if (auto o = readfile(&post->msg, a()->current_sub().filename)) {
    auto b = o.value();
    grab_quotes(b, reply_to_name, a()->context());

    if (okfsed() && a()->user()->IsUseAutoQuote() && msgnum > 0 &&
        msgnum <= a()->GetNumMessagesInCurrentMessageArea() && user_input[0] != 'O') {
      const auto type =
          user_input[0] == '@' ? quote_date_format_t::generic : quote_date_format_t::post;
      auto_quote(b, reply_to_name, type, post->daten, a()->context());
    }
  }

  if (post->status & status_post_new_net) {
    set_net_num(post->network.network_msg.net_number);
    if (post->network.network_msg.net_number == static_cast<uint8_t>(-1)) {
      bout << "|#6Deleted network.\r\n";
      return;
    }
  }

  if (user_input[0] == 'O' && (so() || lcs())) {
    show_files("*.frm", a()->config()->gfilesdir().c_str());
    bout << "|#2Which form letter: ";
    auto fn = bin.input_filename("", 8);
    if (fn.empty()) {
      return;
    }
    auto full_pathname = FilePath(a()->config()->gfilesdir(), StrCat(fn, ".frm"));
    if (!File::Exists(full_pathname)) {
      full_pathname = FilePath(a()->config()->gfilesdir(), StrCat("form", fn, ".msg"));
    }
    if (File::Exists(full_pathname)) {
      LoadFileIntoWorkspace(a()->context(), full_pathname, true);
      email(a()->sess().irt(), post->owneruser, post->ownersys, false, post->anony);
      clear_quotes(a()->sess());
    }
  } else if (user_input[0] == '@') {
    bout.nl();
    bout << "|#21|#7]|#9 Reply to Different Address\r\n";
    bout << "|#22|#7]|#9 Forward Message\r\n";
    bout.nl();
    bout << "|#7(Q=Quit) Select [1,2] : ";
    auto ch_reply = onek("Q12\r", true);
    switch (ch_reply) {
    case '\r':
    case 'Q':
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      break;
    case '1': {
      bout.nl();
      bout << "|#9Enter user name or number:\r\n:";
      auto un_nn = fixup_user_entered_email(bin.input(75, true));
      auto [un, sy] = parse_email_info(un_nn);
      if (un || sy) {
        email("", un, sy, false, 0);
      }
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    } break;
    case '2': {
      if (msgnum > 0 && msgnum <= a()->GetNumMessagesInCurrentMessageArea()) {
        auto o = readfile(&post->msg, a()->current_sub().filename);
        if (!o) {
          break;
        }
        auto b = o.value();
        string filename = "EXTRACT.TMP";
        if (File::Exists(filename)) {
          File::Remove(filename);
        }
        File fileExtract(filename);
        if (!fileExtract.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
          bout << "|#6Could not open file for writing.\r\n";
        } else {
          if (fileExtract.length() > 0) {
            fileExtract.Seek(-1L, File::Whence::end);
            char chLastChar = CZ;
            fileExtract.Read(&chLastChar, 1);
            if (chLastChar == CZ) {
              fileExtract.Seek(-1L, File::Whence::end);
            }
          }
          auto buffer = fmt::format("ON: {}", a()->current_sub().name);
          fileExtract.Write(buffer);
          fileExtract.Write("\r\n\r\n", 4);
          fileExtract.Write(post->title, strlen(post->title));
          fileExtract.Write("\r\n", 2);
          fileExtract.Write(b);
          fileExtract.Close();
        }
        bout.nl();
        bout << "|#5Allow editing? ";
        if (bin.yesno()) {
          bout.nl();
          LoadFileIntoWorkspace(a()->context(), filename, false);
        } else {
          bout.nl();
          LoadFileIntoWorkspace(a()->context(), filename, true);
        }
        send_email();
        auto tmpfn = FilePath(a()->sess().dirs().temp_directory(), INPUT_MSG);
        if (File::Exists(tmpfn)) {
          File::Remove(tmpfn);
        }
      }
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    } break;
    }
  } else {
    if (lcs() || (a()->config()->sl(a()->sess().effective_sl()).ability & ability_read_post_anony) || post->anony == 0) {
      email("", post->owneruser, post->ownersys, false, 0);
    } else {
      email("", post->owneruser, post->ownersys, false, post->anony);
    }
    clear_quotes(a()->sess());
  }
}

static void HandleScanReadFind(int& msgno, MsgScanOption& scan_option) {
  const auto r = wwiv::bbs::FindNextMessage(msgno);
  if (r.found) {
    msgno = r.msgnum;
    scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
  } else {
    scan_option = MsgScanOption::SCAN_OPTION_READ_PROMPT;
  }
}

static FullScreenView CreateFullScreenListTitlesView() {

  std::map<std::string, std::string> m;
  m["subnum"] = std::to_string(a()->current_user_sub_num());
  const auto& c = a()->current_sub();
  m["name"] = c.name;
  m["desc"] = c.desc;
  m["maxmsgs"] = std::to_string(c.maxmsgs);
  m["filename"] = c.filename;
  m["num_msgs"] = std::to_string(a()->GetNumMessagesInCurrentMessageArea());
  m["subnum"] = std::to_string(a()->current_user_sub_num());
  a()->context().add_context_variables("cursub", m);

  const auto saved_mci_enabled = bout.mci_enabled();
  ScopeExit at_exit([=] {
    a()->context().clear_context_variables();
    bout.set_mci_enabled(saved_mci_enabled);
  });
  bout.set_mci_enabled(true);
  auto num_header_lines = 2;
  if (bout.printfile("fs_msgscan")) {
    if (const auto iter = a()->context().return_values().find("num_header_lines");
        iter != std::end(a()->context().return_values())) {
      num_header_lines = to_number<int>(iter->second);
    }
  } else {
    bout.format("|[2J|#4Sub #{} - {}  ({} messages.)|[K|#0\r\n", a()->current_user_sub_num(),
                  a()->current_sub().name, a()->GetNumMessagesInCurrentMessageArea());
    bout.format("|14      Num {:<42} From\r\n", "Title");
  }

  bout.clear_lines_listed();
  const auto screen_width = a()->user()->GetScreenChars();
  const auto screen_length = a()->user()->GetScreenLines() - 1;
  return FullScreenView(bout, bin, num_header_lines, screen_width, screen_length);
}

static std::string CreateLine(std::unique_ptr<Message>&& msg, const int msgnum) {
  if (!msg) {
    return "";
  }
  string tmpbuf;
  const auto& h = msg->header();
  if (h.local() && h.from_usernum() == a()->sess().user_num()) {
    tmpbuf = fmt::sprintf("|09[|11%d|09]", msgnum);
  } else if (!h.local()) {
    tmpbuf = fmt::sprintf("|09<|11%d|09>", msgnum);
  } else {
    tmpbuf = fmt::sprintf("|09(|11%d|09)", msgnum);
  }
  string line = "       ";
  if (h.storage_type() == 2) {
    // HACK: Need to make this generic. this before supporting JAM.
    // N.B. If for some reason dynamic_cast fails, a std::bad_cast is thrown.
    const auto& wh = dynamic_cast<const WWIVMessageHeader&>(h);
    if (wh.last_read() > a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()]) {
      line[0] = '*';
    }
  }
  if (h.pending_network() || h.unvalidated()) {
    line[0] = '+';
  }
  const auto tmpbuf_size = size_without_colors(tmpbuf);
  line.resize(std::max<int>(0, 9 - tmpbuf_size));
  line += tmpbuf;
  line += "|11 ";
  if ((h.unvalidated() || h.deleted()) && !lcs()) {
    line += "<<< NOT VALIDATED YET >>>";
  } else {
    // Need the StringTrim on post title since some FSEDs
    // added \r in the title string, also gets rid of extra spaces
    line += trim_to_size_ignore_colors(StringTrim(h.title()), 60);
  }

  if (a()->user()->GetScreenChars() >= 80) {
    if (size_without_colors(line) > 50) {
      line = trim_to_size_ignore_colors(line, 50);
    }
    line += std::string(51 - size_without_colors(line), ' ');
    if (okansi()) {
      line += "|09\xB3|10 ";
    } else {
      line += "| ";
    }
    if ((h.anony() & 0x0f) && ((a()->config()->sl(a()->sess().effective_sl()).ability & ability_read_post_anony) == 0)) {
      line += ">UNKNOWN<";
    } else {
      line += trim_to_size_ignore_colors(h.from(), 25);
    }
  }
  return line;
}

static std::vector<std::string> CreateMessageTitleVector(MessageArea* area, int start, int num) {
  vector<string> lines;
  for (auto i = start; i < (start + num); i++) {
    auto line = CreateLine(area->ReadMessage(i), i);
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

static void display_title_new(const std::vector<std::string>& lines, const FullScreenView& fs,
                              int i, bool selected) {
  bout.GotoXY(1, i + fs.lines_start());
  const auto l = lines.at(i);
  if (selected) {
    bout << "|17|12>";
  } else {
    bout << "|16|#0 ";
  }
  bout << pad_to_ignore_colors(l, fs.screen_width() - 1) << "|#0";
}

static void display_titles_new(const std::vector<std::string>& lines, const FullScreenView& fs,
                               int start, int selected) {
  for (auto i = 0; i < fs.message_height(); i++) {
    if (i >= ssize(lines)) {
      break;
    }
    const auto is_selected = i == selected - start;
    display_title_new(lines, fs, i, is_selected);
  }
}

static ReadMessageResult HandleListTitlesFullScreen(int& msgnum, MsgScanOption& scan_option_type) {
  bout.cls();
  auto* api = a()->msgapi();
  unique_ptr<MessageArea> area(
      api->Open(a()->current_sub(), a()->sess().GetCurrentReadMessageArea()));
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

  const auto window_top_min = 1;
  const auto first = 1;
  const auto last =
      std::max<int>(first, area->number_of_messages() - fs.message_height() + window_top_min);
  const auto height = std::min<int>(num_msgs_in_area, fs.message_height());

  auto selected = msgnum;
  auto window_top = std::min(msgnum, last);
  auto window_bottom = window_top + height - window_top_min + 1;
  // When starting mid-range, sometimes the selected is past the bottom.
  if (selected > window_bottom)
    selected = window_bottom;

  auto done = false;
  auto need_redraw = true;
  auto last_selected = selected;
  while (!done) {
    a()->CheckForHangup();
    auto lines = CreateMessageTitleVector(area.get(), window_top, height);
    if (need_redraw) {
      // Full redraw of the screen
      display_titles_new(lines, fs, window_top, selected);
    } else if (last_selected != selected) {
      // Partial redraw of part of the screen
      display_title_new(lines, fs, (last_selected - window_top), false);
      display_title_new(lines, fs, (selected - window_top), true);
    }

    if (last_selected != selected) {
      // Do this for all cases, even if need_redraw isn' true.
      bout.GotoXY(1, fs.lines_start() + selected - (window_top - window_top_min) + window_top_min);
      fs.DrawBottomBar(StrCat("Selected: ", selected));
      last_selected = selected;
    }

    if (need_redraw) {
      fs.ClearCommandLine();
      fs.PutsCommandLine("|#9(|#2Q|#9=Quit, |#2?|#9=Help): ");
    }
    need_redraw = false;
    auto key = bin.bgetch_event(
        wwiv::common::Input::numlock_status_t::NOTNUMBERS,
                     [&](wwiv::common::Input::bgetch_timeout_status_t status, int s) {
                              if (status == wwiv::common::Input::bgetch_timeout_status_t::WARNING) {
            fs.PrintTimeoutWarning(s);
                              } else if (status ==
                                         wwiv::common::Input::bgetch_timeout_status_t::CLEAR) {
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
          need_redraw = true;
        }
      } else {
        selected--;
      }
    } break;
    case COMMAND_PAGEUP: {
      auto orig_window_top = window_top;
      window_top -= height;
      selected -= height;
      window_top = std::max<int>(window_top, window_top_min);
      selected = std::max<int>(selected, 1);
      if (orig_window_top != window_top) {
        // Only redraw the screen if we've moved it.
        need_redraw = true;
      }
    } break;
    case COMMAND_HOME: {
      need_redraw = true;
      selected = 1;
      window_top = window_top_min;
    } break;
    case COMMAND_DOWN: {
      auto current_window_bottom = window_top + height - window_top_min;
      auto d = num_msgs_in_area - height + window_top_min;
      if (selected < current_window_bottom) {
        selected++;
      } else if (window_top < d) {
        selected++;
        window_top++;
        need_redraw = true;
      }
    } break;
    case COMMAND_PAGEDN: {
      auto orig_window_top = window_top;
      window_top += height;
      selected += height;
      window_top = std::min<int>(window_top, num_msgs_in_area - height + window_top_min);
      selected = std::min<int>(selected, num_msgs_in_area);
      if (orig_window_top != window_top) {
        // Only redraw the screen if we've moved it.
        need_redraw = true;
      }
    } break;
    case COMMAND_END: {
      window_top = num_msgs_in_area - height + window_top_min;
      selected = window_top;
      need_redraw = true;
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
    }
    default: {
      if ((key & 0xff) == key) {
        key = toupper(key & 0xff);
        switch (key) {
        case 'F': {
          fs.ClearCommandLine();

          const auto r = wwiv::bbs::FindNextMessageFS(fs, msgnum);
          if (r.found) {
            msgnum = r.msgnum;
            scan_option_type = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
          } else {
            scan_option_type = MsgScanOption::SCAN_OPTION_READ_PROMPT;
          }

          fs.ClearCommandLine();
          ReadMessageResult result;
          result.option = ReadMessageOption::READ_MESSAGE;
          return result;
        }
        case 'J': {
          fs.ClearCommandLine();
          bout << "Enter Message Number (1-" << num_msgs_in_area << ") :";
          msgnum = bin.input_number(msgnum, 1, num_msgs_in_area, false);

          ReadMessageResult result;
          result.option = ReadMessageOption::READ_MESSAGE;
          fs.ClearCommandLine();
          return result;
        }
        case 'Q': {
          ReadMessageResult result;
          result.option = ReadMessageOption::COMMAND;
          result.command = 'Q';
          fs.ClearCommandLine();
          return result;
        }
        case '?': {
          need_redraw = true;
          fs.ClearMessageArea();
          if (!bout.printfile(TITLE_FSED_NOEXT)) {
            fs.ClearCommandLine();
            bout << "|#6Unable to find file: " << TITLE_FSED_NOEXT;
            bout.pausescr();
            fs.ClearCommandLine();
          } else {
            fs.ClearCommandLine();
            bout.pausescr();
            fs.ClearCommandLine();
          }
        } break;
        } // default switch
      }
    } break;
    } // switch
  }
  return {};
}

static void HandleListTitles(int& msgnum, MsgScanOption& scan_option_type) {
  bout.cls();
  auto* api = a()->msgapi();
  unique_ptr<MessageArea> area(
      api->Open(a()->current_sub(), a()->sess().GetCurrentReadMessageArea()));
  if (!area) {
    return;
  }
  scan_option_type = MsgScanOption::SCAN_OPTION_READ_PROMPT;
  auto fs = CreateFullScreenListTitlesView();

  const auto num_msgs_in_area = area->number_of_messages();
  auto abort = false;
  if (msgnum >= num_msgs_in_area) {
    return;
  }

  bout << "|#7" << static_cast<unsigned char>(198)
       << string(a()->user()->GetScreenChars() - 3, static_cast<unsigned char>(205))
       << static_cast<unsigned char>(181) << "\r\n";
  const auto num_title_lines = std::max<int>(a()->sess().num_screen_lines() - 6, 1);
  auto i = 0;
  while (!abort && ++i <= num_title_lines) {
    ++msgnum;
    const auto line = CreateLine(area->ReadMessage(msgnum), msgnum);
    bout.bpla(line, &abort);
    if (msgnum >= num_msgs_in_area) {
      abort = true;
    }
  }
  bout << "|#7" << static_cast<unsigned char>(198)
       << string(a()->user()->GetScreenChars() - 3, static_cast<unsigned char>(205))
       << static_cast<unsigned char>(181) << "\r\n";
}

static void HandleMessageDownload(int msgnum) {
  if (msgnum > 0 && msgnum <= a()->GetNumMessagesInCurrentMessageArea()) {
    auto o = readfile(&(get_post(msgnum)->msg), (a()->current_sub().filename));
    if (!o) {
      return;
    }
    const auto& b = o.value();
    bout << "|#1Message Download -\r\n\n";
    bout << "|#2Filename to use? ";
    const auto filename = bin.input_filename(12);
    if (!okfn(filename)) {
      return;
    }
    const auto f = FilePath(a()->sess().dirs().temp_directory(), filename);
    File::Remove(f);
    TextFile tf(f, "wt");
    tf.Write(b);

    bool bFileAbortStatus;
    bool bStatus;
    send_file(f.string(), &bStatus, &bFileAbortStatus, f.string(), -1, b.length());
    bout << "|#1Message download... |#2" << (bStatus ? "successful" : "unsuccessful");
    if (bStatus) {
      sysoplog() << "Downloaded message";
    }
  }
}

void HandleMessageMove(int& msg_num) {
  if ((lcs()) && (msg_num > 0) &&
      (msg_num <= a()->GetNumMessagesInCurrentMessageArea())) {
    string ss1;
    tmp_disable_conf(true);
    bout.nl();
    do {
      bout << "|#5(|#2Q|#5=|#2Quit|#5) Move to which sub? ";
      ss1 = mmkey(MMKeyAreaType::subs);
      if (ss1[0] == '?') {
        old_sublist();
      }
    } while (ss1[0] == '?');
    auto temp_sub_num = -1;
    if (ss1[0] == 0) {
      tmp_disable_conf(false);
      return;
    }
    auto ok = false;
    for (auto i1 = 0; i1 < ssize(a()->usub) && !ok; i1++) {
      if (ss1 == a()->usub[i1].keys) {
        temp_sub_num = i1;
        bout.nl();
        bout << "|#9Copying to " << a()->subs().sub(a()->usub[temp_sub_num].subnum).name << endl;
        ok = true;
      }
    }
    if (temp_sub_num != -1) {
      if (!wwiv::bbs::check_acs(a()->subs().sub(a()->usub[temp_sub_num].subnum).post_acs)) {
        bout.nl();
        bout << "Sorry, you don't have post access on that sub.\r\n\n";
        temp_sub_num = -1;
      }
    }
    if (temp_sub_num != -1) {
      open_sub(true);
      resynch(&msg_num, nullptr);
      auto p2 = *get_post(msg_num);
      auto p1 = p2;
      auto b = readfile(&(p2.msg), (a()->current_sub().filename)).value_or("");
      bout.nl();
      bout << "|#5Delete original post? ";
      if (bin.yesno()) {
        delete_message(msg_num);
        if (msg_num > 1) {
          msg_num--;
        }
      }
      close_sub();
      iscan(temp_sub_num);
      open_sub(true);
      p2.msg.storage_type = static_cast<unsigned char>(a()->current_sub().storage_type);
      savefile(b, &(p2.msg), (a()->current_sub().filename));
      a()->status_manager()->Run([&](Status& s) { p2.qscan = s.next_qscanptr(); });
      if (a()->GetNumMessagesInCurrentMessageArea() >= a()->current_sub().maxmsgs) {
        auto temp_msg_num = 1;
        auto msg_to_delete = 0;
        while (msg_to_delete == 0 && temp_msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
          if ((get_post(temp_msg_num)->status & status_no_delete) == 0) {
            msg_to_delete = temp_msg_num;
          }
          ++temp_msg_num;
        }
        if (msg_to_delete == 0) {
          msg_to_delete = 1;
        }
        delete_message(msg_to_delete);
      }
      if (!(a()->current_sub().anony & anony_val_net) || a()->current_sub().nets.empty()) {
        p2.status &= ~status_pending_net;
      }
      if (!a()->current_sub().nets.empty()) {
        p2.status |= status_pending_net;
        a()->user()->posts_net(a()->user()->posts_net() + 1);
        send_net_post(&p2, a()->current_sub());
      }
      add_post(&p2);
      close_sub();
      tmp_disable_conf(false);
      iscan(a()->current_user_sub_num());
      bout.nl();
      bout << "|#9Message moved.\r\n\n";
      resynch(&msg_num, &p1);
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
  const auto fn = bin.input_path(50);
  if (fn.empty()) {
    return;
  }
  bout.nl();
  bout << "|#5Allow editing? ";
  const auto no_edit_allowed = !bin.yesno();
  bout.nl();
  LoadFileIntoWorkspace(a()->context(), fn, no_edit_allowed);
}

void HandleMessageReply(int& msg_num) {
  auto p2 = *get_post(msg_num);
  if (!lcs() && (p2.status & (status_unvalidated | status_delete))) {
    return;
  }
  const auto& cs = a()->current_sub();
  auto m = read_type2_message(&p2.msg, static_cast<char>(p2.anony & 0x0f), true,
                              cs.filename.c_str(), p2.ownersys, p2.owneruser);
  m.title = p2.title;

  grab_quotes(m.raw_message_text, m.from_user_name, a()->context());

  if (okfsed() && a()->user()->IsUseAutoQuote() && msg_num > 0 &&
      msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
    // auto_quote needs the raw message text like from readfile(), so that
    // the top two lines are header information.
    auto_quote(m.raw_message_text, m.from_user_name, quote_date_format_t::generic, p2.daten,
               a()->context());
  }

  if (!m.title.empty()) {
    a()->sess().irt(m.title);
  }
  PostReplyToData r;
  r.name = m.from_user_name;
  r.title = m.title;
  r.text = m.message_text;
  post(PostData(r));
  resynch(&msg_num, &p2);
  clear_quotes(a()->sess());
}

static void HandleMessageDelete(int& msg_num) {
  if (!lcs()) {
    return;
  }
  if (!msg_num) {
    return;
  }

  bout << "|#5Delete message #" << msg_num << ". Are you sure?";
  if (!bin.noyes()) {
    return;
  }

  open_sub(true);
  resynch(&msg_num, nullptr);
  auto p2 = *get_post(msg_num);
  delete_message(msg_num);
  close_sub();

  wwiv::core::ScopeExit at_exit([&] { resynch(&msg_num, &p2); });
  if (p2.ownersys != 0) {
    return;
  }
  User tu;
  if (!a()->users()->readuser(&tu, p2.owneruser)) {
    return;
  }
  if (tu.IsUserDeleted()) {
    return;
  }
  if (date_to_daten(tu.firston()) < p2.daten) {
    bout.nl();
    bout << "|#2Remove how many posts credit? ";
    unsigned short num_credits =
        bin.input_number<uint16_t>(0, 0, static_cast<uint16_t>(tu.messages_posted()));
    if (num_credits != 0) {
      tu.messages_posted(tu.messages_posted() - num_credits);
    }
    bout.nl();
    bout << "|#7Post credit removed = " << num_credits << endl;
    tu.deleted_posts(tu.deleted_posts() + 1);
    a()->users()->writeuser(&tu, p2.owneruser);
    a()->UpdateTopScreen();
  }
}

static void HandleMessageExtract(int& msgnum) {
  if (!so()) {
    return;
  }
  if (msgnum > 0 && msgnum <= a()->GetNumMessagesInCurrentMessageArea()) {
    if (auto o = readfile(&get_post(msgnum)->msg, (a()->current_sub().filename))) {
      const auto& b = o.value();
      extract_out(b, get_post(msgnum)->title);
    }
  }
}

static void HandleMessageHelp() {
  if (a()->sess().forcescansub()) {
    bout.printfile(MUSTREAD_NOEXT);
  } else if (lcs()) {
    bout.print_help_file(SMBMAIN_NOEXT);
  } else {
    bout.print_help_file(MBMAIN_NOEXT);
  }
}

static void HandleValUser(int msg_num) {
  if (cs() && get_post(msg_num)->ownersys == 0 && msg_num > 0 &&
      msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
    valuser(get_post(msg_num)->owneruser);
  } else if (cs() && msg_num > 0 &&
             msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
    bout.nl();
    bout << "|#6Post from another system.\r\n\n";
  }
}

static void HandleToggleAutoPurge(int msg_num) {
  if (lcs() && msg_num > 0 &&
      msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
    wwiv::bbs::OpenSub opened_sub(true);
    resynch(&msg_num, nullptr);
    auto p3 = get_post(msg_num);
    p3->status ^= status_no_delete;
    write_post(msg_num, p3);
    bout.nl();
    if (p3->status & status_no_delete) {
      bout << "|#9Message will |#6NOT|#9 be auto-purged.\r\n";
    } else {
      bout << "|#9Message |#2CAN|#9 now be auto-purged.\r\n";
    }
    bout.nl();
  }
}

static void HandleTogglePendingNet(int msg_num, int& val) {
  if (lcs() && msg_num > 0 && msg_num <= a()->GetNumMessagesInCurrentMessageArea() &&
      a()->current_sub().anony & anony_val_net && !a()->current_sub().nets.empty()) {
    wwiv::bbs::OpenSub opened_sub(true);
    resynch(&msg_num, nullptr);
    auto p3 = get_post(msg_num);
    p3->status ^= status_pending_net;
    write_post(msg_num, p3);
    close_sub();
    bout.nl();
    if (p3->status & status_pending_net) {
      val |= 2;
      bout << "|#9Will be sent out on net now.\r\n";
    } else {
      bout << "|#9Not set for net pending now.\r\n";
    }
    bout.nl();
  }
}

static void HandleRemoveFromNewScan() {
  const auto subname = a()->subs().sub(a()->current_user_sub().subnum).name;
  bout << "\r\n|#5Remove '" << subname << "' from your Q-Scan? ";
  if (bin.yesno()) {
    a()->sess().qsc_q[a()->current_user_sub().subnum / 32] ^=
        (1L << (a()->current_user_sub().subnum % 32));
    return;
  }

  bout << "\r\n|#9Mark messages in '" << subname << "' as read? ";
  if (bin.yesno()) {
    const auto status = a()->status_manager()->get_status();
    a()->sess().qsc_p[a()->current_user_sub().subnum] = status->qscanptr() - 1L;
  }
}

static void HandleToggleUnAnonymous(int msg_num) {
  if (lcs() && msg_num > 0 && msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
    wwiv::bbs::OpenSub opened_sub(true);
    resynch(&msg_num, nullptr);
    postrec* p3 = get_post(msg_num);
    p3->anony = 0;
    write_post(msg_num, p3);
    bout.nl();
    bout << "|#9Message is not anonymous now.\r\n";
  }
}

static void HandleScanReadPrompt(int& msgnum, MsgScanOption& scan_option, bool& nextsub,
                                 bool& title_scan, bool& done, bool& quit, int& val) {
  bin.resetnsp();
  const auto read_prompt = GetScanReadPrompts(msgnum);
  bout.nl();
  char szUserInput[81];
  bout << read_prompt;
  bin.input(szUserInput, 5, true);
  resynch(&msgnum, nullptr);
  while (szUserInput[0] == 32) {
    char szTempBuffer[255];
    strcpy(szTempBuffer, &(szUserInput[1]));
    strcpy(szUserInput, szTempBuffer);
  }
  if (title_scan && szUserInput[0] == 0 && msgnum < a()->GetNumMessagesInCurrentMessageArea()) {
    scan_option = MsgScanOption::SCAN_OPTION_LIST_TITLES;
    szUserInput[0] = 'T';
    szUserInput[1] = '\0';
  } else {
    title_scan = false;
    scan_option = MsgScanOption::SCAN_OPTION_READ_PROMPT;
  }
  auto nUserInput = to_number<int>(szUserInput);
  if (szUserInput[0] == '\0') {
    nUserInput = msgnum + 1;
    if (nUserInput >= a()->GetNumMessagesInCurrentMessageArea() + 1) {
      done = true;
    }
  }

  if (nUserInput != 0 && nUserInput <= a()->GetNumMessagesInCurrentMessageArea() &&
      nUserInput >= 1) {
    scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
    msgnum = nUserInput;
  } else if (szUserInput[1] == '\0') {
    if (a()->sess().forcescansub()) {
      return;
    }
    switch (szUserInput[0]) {
    case '$':
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      msgnum = a()->GetNumMessagesInCurrentMessageArea();
      break;
    case 'F':
      HandleScanReadFind(msgnum, scan_option);
      break;
    case 'Q':
      quit = true;
      done = true;
      nextsub = false;
      break;
    case 'B':
      if (nextsub) {
        HandleRemoveFromNewScan();
      }
      nextsub = true;
      done = true;
      quit = true;
      break;
    case 'T':
      title_scan = true;
      scan_option = MsgScanOption::SCAN_OPTION_LIST_TITLES;
      break;
    case 'R':
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      break;
    case '?':
      HandleMessageHelp();
      break;
    case 'A':
      HandleScanReadAutoReply(msgnum, szUserInput, scan_option);
      break;
    case 'D':
      HandleMessageDelete(msgnum);
      break;
    case 'E':
      HandleMessageExtract(msgnum);
      break;
    case 'L':
      HandleMessageLoad();
      break;
    case 'M':
      HandleMessageMove(msgnum);
      break;
    case 'N':
      HandleToggleAutoPurge(msgnum);
      break;
    case 'P':
      post(PostData());
      break;
    case 'U':
      HandleToggleUnAnonymous(msgnum);
      break;
    case 'V':
      HandleValUser(msgnum);
      break;
    case 'W':
      HandleMessageReply(msgnum);
      break;
    case 'X':
      HandleTogglePendingNet(msgnum, val);
      break;
    case 'Y':
      HandleMessageDownload(msgnum);
      break;
    case '>':
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      break;
    case '-':
      if (msgnum > 1 && (msgnum - 1 < a()->GetNumMessagesInCurrentMessageArea())) {
        --msgnum;
        scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      }
      break;
      // These used to be threaded code.
    case '*':
      break;
    case '[':
      break;
    case ']':
      break;
    }
  }
}

static void validate() {
  bout.nl();
  bout << "|#5Validate messages here? ";
  if (bin.noyes()) {
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
  if (bin.yesno()) {
    int nNumMsgsSent = 0;
    vector<postrec> to_validate;
    {
      wwiv::bbs::OpenSub opened_sub(true);
      for (int i = 1; i <= a()->GetNumMessagesInCurrentMessageArea(); i++) {
        postrec* p4 = get_post(i);
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

// Asks if user wants to post, returns true if done, meaning user says Yes (and posts) or No.
static bool query_post() {
  if (!a()->user()->IsRestrictionPost() &&
      a()->user()->posts_today() < a()->config()->sl(a()->sess().effective_sl()).posts &&
      wwiv::bbs::check_acs(a()->current_sub().post_acs)) {
    bout << "|#5Post on " << a()->current_sub().name << " (|#2Y/N/Q|#5) ? ";
    a()->sess().clear_irt();
    clear_quotes(a()->sess());
    auto q = bin.ynq();
    if (q == 'Y') {
      post(PostData());
      return true;
    }
    if (q == 'N') {
      return true;
    }
    return false;
  }
  bout << "|#5Move to the next sub?";
  return bin.yesno();
}

static void scan_new(int msgnum, MsgScanOption scan_option, bool& nextsub, bool title_scan) {
  auto done = false;
  auto val = 0;
  while (!done) {
    a()->CheckForHangup();
    ReadMessageResult result{};
    if (scan_option == MsgScanOption::SCAN_OPTION_READ_MESSAGE) {
      if (msgnum > 0 && msgnum <= a()->GetNumMessagesInCurrentMessageArea()) {
        // Only try to read messages we can.
        auto next = true;
        result = read_post(msgnum, &next, &val);
      } else {
        result.option = ReadMessageOption::NEXT_MSG;
      }
    } else if (scan_option == MsgScanOption::SCAN_OPTION_LIST_TITLES) {
      result = HandleListTitlesFullScreen(msgnum, scan_option);
    } else if (scan_option == MsgScanOption::SCAN_OPTION_READ_PROMPT) {
      auto quit = false;
      HandleScanReadPrompt(msgnum, scan_option, nextsub, title_scan, done, quit, val);
      if (quit) {
        done = true;
      }
    }

    switch (result.option) {
    case ReadMessageOption::READ_MESSAGE: {
      scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      if (msgnum > a()->GetNumMessagesInCurrentMessageArea()) {
        done = true;
      }
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
      const auto max_msgnum = a()->GetNumMessagesInCurrentMessageArea();
      bout << "Enter Message Number (1-" << max_msgnum << ") :";
      msgnum = bin.input_number(msgnum, 0, max_msgnum, false);
      if (msgnum < 1) {
        done = true;
      }
    } break;
    case ReadMessageOption::LIST_TITLES: {
      scan_option = MsgScanOption::SCAN_OPTION_LIST_TITLES;
    } break;
    case ReadMessageOption::COMMAND: {
      switch (result.command) {
      case 'Q':
        done = true;
        nextsub = false;
        break;
      case 'A':
        HandleScanReadAutoReply(msgnum, "A", scan_option);
        break;
      case 'B':
        if (nextsub) {
          HandleRemoveFromNewScan();
        }
        nextsub = true;
        done = true;
        break;
      case 'D':
        HandleMessageDelete(msgnum);
        break;
      case 'E':
        HandleMessageExtract(msgnum);
        break;
      case 'L':
        HandleMessageLoad();
        break;
      case 'M':
        HandleMessageMove(msgnum);
        break;
      case 'N':
        HandleToggleAutoPurge(msgnum);
        break;
      case 'P': {
        post(PostData());
      } break;
      case 'U':
        HandleToggleUnAnonymous(msgnum);
        break;
      case 'V':
        HandleValUser(msgnum);
        break;
      case 'W':
        HandleMessageReply(msgnum);
        break;
      case 'X':
        HandleTogglePendingNet(msgnum, val);
        break;
      case 'Y':
        HandleMessageDownload(msgnum);
        break;
      }
    } break;
    }
    if (done) {
      if ((val & 1) && lcs()) {
        validate();
      }
      if ((val & 2) && lcs()) {
        network_validate();
      }
      done = query_post();
      if (!done) {
        // back to list title.
        scan_option = MsgScanOption::SCAN_OPTION_LIST_TITLES;
      }
    }
  }
}

void scan(int msg_num, MsgScanOption scan_option, bool& nextsub, bool title_scan) {
  a()->sess().clear_irt();

  int val = 0;
  iscan(a()->current_user_sub_num());
  if (a()->sess().GetCurrentReadMessageArea() < 0) {
    bout.nl();
    bout << "No subs available.\r\n\n";
    return;
  }

  const auto& cs = a()->current_sub();
  const auto fsreader_enabled =
      a()->fullscreen_read_prompt() && a()->user()->HasStatusFlag(User::fullScreenReader);
  const bool skip_fs_reader_per_sub = (cs.anony & anony_no_fullscreen) != 0;
  if (fsreader_enabled && !skip_fs_reader_per_sub) {
    scan_new(msg_num, scan_option, nextsub, title_scan);
    return;
  }

  bool done = false;
  bool quit = false;
  do {
    a()->tleft(true);
    a()->CheckForHangup();
    set_net_num((a()->current_sub().nets.empty()) ? 0 : a()->current_sub().nets[0].net_num);
    if (scan_option != MsgScanOption::SCAN_OPTION_READ_PROMPT) {
      resynch(&msg_num, nullptr);
    }
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);

    switch (scan_option) {
    case MsgScanOption::SCAN_OPTION_READ_PROMPT: { // Read Prompt
      HandleScanReadPrompt(msg_num, scan_option, nextsub, title_scan, done, quit, val);
    } break;
    case MsgScanOption::SCAN_OPTION_LIST_TITLES: { // List Titles
      HandleListTitles(msg_num, scan_option);
    } break;
    case MsgScanOption::SCAN_OPTION_READ_MESSAGE: { // Read Message
      auto next = false;
      if (msg_num > 0 && msg_num <= a()->GetNumMessagesInCurrentMessageArea()) {
        const auto old_incom = a()->sess().incom();
        if (a()->sess().forcescansub()) {
          a()->sess().incom(false);
        }
        read_post(msg_num, &next, &val);
        if (a()->sess().forcescansub()) {
          a()->sess().incom(old_incom);
        }
      }
      bout.Color(0);
      bout.nl();
      if (next) {
        ++msg_num;
        if (msg_num > a()->GetNumMessagesInCurrentMessageArea()) {
          done = true;
        }
        scan_option = MsgScanOption::SCAN_OPTION_READ_MESSAGE;
      } else {
        scan_option = MsgScanOption::SCAN_OPTION_READ_PROMPT;
      }
    } break;
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
