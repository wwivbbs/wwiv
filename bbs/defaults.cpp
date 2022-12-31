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
#include "bbs/defaults.h"

#include "bbs/bbs.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/common.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/inetmsg.h"
#include "bbs/instmsg.h"
#include "bbs/misccmd.h"
#include "bbs/mmkey.h"
#include "bbs/msgbase1.h"
#include "bbs/newuser.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/qwk/qwk.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "menus/config_menus.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/usermanager.h"
#include "sdk/files/dirs.h"
#include "sdk/net/networks.h"

#include <sstream>
#include <string>
#include <vector>

using wwiv::common::InputMode;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

static const int STOP_LIST = 0;
static const int MAX_SCREEN_LINES_TO_SHOW = 24;

void select_editor() {
  bout.outstr("|#10|#9) Line Editor\r\n");
  if (okansi() && a()->IsUseInternalFsed()) {
    bout.outstr("|#1A|#9) Full-Screen Editor\r\n");
  }
  for (auto i = 0; i < size_int(a()->editors); i++) {
    bout.print("|#1{}|#9) {}\r\n", i+1, a()->editors[i].description);
  }
  std::set<char> keys;
  keys.insert('Q');
  bout.nl();
  bout.outstr("|#9Which editor (|#1");
  if (okansi() && a()->IsUseInternalFsed()) {
    keys.insert('A');
    bout.outstr("A|#9, |#1");
  }
  bout.print("1-{}, |#9(Q=No Change) ? ", a()->editors.size());
  const auto cur_editor = a()->user()->default_editor();
  const auto k = bin.input_number_hotkey(cur_editor != 0xff ? cur_editor : 0, keys, 0,
                                         size_int(a()->editors), false);
  if (k.key == 'A') {
    a()->user()->default_editor(0xff);
  } else if (k.key == 'Q') {
    a()->user()->default_editor(cur_editor);
  } else {
    a()->user()->default_editor(k.num);
  }
}

static std::string GetMailBoxStatus() {
  if (a()->user()->forward_systemnum() == 0 &&
      a()->user()->forward_usernum() == 0) {
    return std::string("Normal");
  }
  if (a()->user()->forward_systemnum() != 0) {
    if (a()->user()->mailbox_forwarded()) {
      return fmt::format("Forward to #{} @{}.{}.", a()->user()->forward_usernum(),
                         a()->user()->forward_systemnum(),
                         wwiv::stl::at(a()->nets(), a()->user()->forward_netnum()).name);
    }
    const auto fwd_username = read_inet_addr(a()->sess().user_num());
    return StrCat("Forwarded to Internet ", fwd_username);
  }

  if (a()->user()->mailbox_closed()) {
    return "Closed";
  }

  User ur;
  a()->users()->readuser(&ur, a()->user()->forward_usernum());
  if (ur.deleted()) {
    a()->user()->forward_usernum(0);
    return "Normal";
  }
  return StrCat("Forward to ", a()->names()->UserName(a()->user()->forward_usernum()));
}

static void print_cur_stat() {
  bout.cls();
  bout.litebar("Your Preferences");
  const auto screen_size =
      fmt::format("{} X {}", a()->user()->screen_width(), a()->user()->screen_lines());
  const std::string ansi_setting =
      a()->user()->ansi() ? (a()->user()->color() ? "Color" : "Monochrome") : "No ANSI";
  bout.print("|#11|#9) Screen size       : |#2{:<16} ", screen_size);
  bout.print("|#12|#9) ANSI              : |#2{}", ansi_setting);
  bout.nl();
  bout.print("|#13|#9) Pause on screen   : |#2{:<16} ", a()->user()->pause() ? "On " : "Off");
  bout.nl();
  bout.print("|#14|#9) Mailbox           : |#2{}", GetMailBoxStatus());
  bout.nl();
  bout.print("{:<45} {}\r\n", "|#15|#9) Configured Q-scan", "|#16|#9) Change password");
  
  if (okansi()) {
    bout.print("{:<45} {}\r\n", "|#17|#9) Update macros", "|#18|#9) Change colors");

    const auto editor_num = a()->user()->default_editor();
    std::string editor_name = "Line";
    if (a()->IsUseInternalFsed() && editor_num == 0xff) {
      editor_name = "Full Screen";
    }
    if (editor_num > 0 && editor_num <= size_int(a()->editors)) {
      editor_name = a()->editors[editor_num - 1].description;
    }
    bout.print("|#19|#9) Message editor    : |#2{:<16} ", editor_name); 
    bout.print("|#1A|#9) Extended colors   : |#2{}\r\n", YesNoString(a()->user()->extra_color()));
  } else {
    bout.pl("|#17|#9) Update macros");
  }

  const auto inet_addr =
      ((a()->user()->email_address().empty()) ? "None." : a()->user()->email_address());
  bout.print("|#1B|#9) Optional lines    : |#2{:<16} ", a()->user()->optional_val());
  bout.print("|#1C|#9) Conferencing      : |#2{}\r\n", YesNoString(a()->user()->use_conference()));
  bout.print("|#1D|#9) Show Hidden Lines : |#2{:<16} ", YesNoString(a()->user()->has_flag(User::msg_show_controlcodes)));
  if (a()->fullscreen_read_prompt()) {
    bout.print("|#1G|#9) Message Reader    : |#2{}\r\n",
               (a()->user()->has_flag(User::fullScreenReader) ? "Full-Screen" : "Traditional"));
  }
  bout.print("|#1I|#9) Internet Address  : |#2{}\r\n", inet_addr);
  bout.pl("|#1K|#9) Configure Menus");
  if (num_instances() > 1) {
    bout.print("|#1M|#9) Allow user msgs   : |#2{:<16} |#1N|#9) Configure QWK", YesNoString(!a()->user()->ignore_msgs()));
  }
  bout.nl();
  bout.print("|#1S|#9) Cls Between Msgs? : |#2{:<16} ",
                      YesNoString(a()->user()->clear_screen()));
  bout.print("|#1T|#9) 12hr or 24hr clock: |#2{}\r\n", (a()->user()->twentyfour_clock() ? "24hr" : "12hr"));

  std::string wwiv_regnum = "(None)";
  if (a()->user()->wwiv_regnum()) {
    wwiv_regnum = std::to_string(a()->user()->wwiv_regnum());
  }
  bout.print("|#1U|#9) Use Msg AutoQuote : |#2{:<16} ", YesNoString(a()->user()->auto_quote()));
  bout.print("|#1W|#9) WWIV reg num      : |#2{}\r\n", wwiv_regnum);

  bout.pl("|#1Q|#9) Quit to main menu");
}

static std::string DisplayColorName(int c) {
  switch (c) {
  case 0:
    return "Black";
  case 1:
    return "Blue";
  case 2:
    return "Green";
  case 3:
    return "Cyan";
  case 4:
    return "Red";
  case 5:
    return "Magenta";
  case 6:
    return "Yellow";
  case 7:
    return "White";
  default:
    return "";
  }
}

std::string DescribeColorCode(int nColorCode) {
  std::ostringstream os;

  if (a()->user()->color()) {
    os << DisplayColorName(nColorCode & 0x07) << " on " << DisplayColorName((nColorCode >> 4) & 0x07);
  } else {
    os << ((nColorCode & 0x07) ? "Normal" : "Inversed");
  }

  if (nColorCode & 0x08) {
    os << ", Bold";
  }
  if (nColorCode & 0x80) {
    os << ", Blinking";
  }
  return os.str();
}

void color_list() {
  bout.nl(2);
  for (int i = 0; i < 8; i++) {
    bout.setc(static_cast<unsigned char>((i == 0) ? 0x70 : i));
    bout.print("{}. {}|#0\r\n", i, DisplayColorName(static_cast<char>(i)));
  }
}

static void reset_user_colors_to_defaults() {
  for (int i = 0; i <= 9; i++) {
    a()->user()->color(i, a()->newuser_colors[i]);
    a()->user()->bwcolor(i, a()->newuser_bwcolors[i]);
  }
}

void change_colors() {
  bool done = false;
  bout.nl();
  do {
    bout.cls();
    bout.outstr("|#5Customize Colors:");
    bout.nl(2);
    if (!a()->user()->color()) {
      std::ostringstream os;
      os << "Monochrome base color : ";
      if ((a()->user()->bwcolor(1) & 0x70) == 0) {
        os << DisplayColorName(a()->user()->bwcolor(1) & 0x07);
      } else {
        os << DisplayColorName((a()->user()->bwcolor(1) >> 4) & 0x07);
      }
      bout.outstr(os.str());
      bout.nl(2);
    }
    for (int i = 0; i < 10; i++) {
      std::ostringstream os;
      bout.ansic(i);
      os << i << ".";
      switch (i) {
      case 0:
        os << "Default           ";
        break;
      case 1:
        os << "Yes/No            ";
        break;
      case 2:
        os << "Prompt            ";
        break;
      case 3:
        os << "Note              ";
        break;
      case 4:
        os << "Input line        ";
        break;
      case 5:
        os << "Yes/No Question   ";
        break;
      case 6:
        os << "Notice!           ";
        break;
      case 7:
        os << "Border            ";
        break;
      case 8:
        os << "Extra color #1    ";
        break;
      case 9:
        os << "Extra color #2    ";
        break;
      }
      os << DescribeColorCode(a()->user()->color(i));
      bout.outstr(os.str());
      bout.nl();
    }
    bout.outstr("\r\n|#9[|#2R|#9]eset Colors to Default Values, [|#2Q|#9]uit\r\n");
    bout.outstr("|#9Enter Color Number to Modify (|#20|#9-|#29|#9,|#2R|#9,|#2Q|#9): ");
    char ch = onek("RQ0123456789", true);
    if (ch == 'Q') {
      done = true;
    } else if (ch == 'R') {
      reset_user_colors_to_defaults();
    } else {
      uint8_t nc;
      const int color_num = ch - '0';
      if (a()->user()->color()) {
        color_list();
        bout.ansic(0);
        bout.nl();
        bout.outstr("|#9(Q=Quit) Foreground? ");
        ch = onek("Q01234567");
        if (ch == 'Q') {
          continue;
        }
        nc = static_cast<uint8_t>(ch - '0');
        bout.outstr("|#9(Q=Quit) Background? ");
        ch = onek("Q01234567");
        if (ch == 'Q') {
          continue;
        }
        nc = static_cast<uint8_t>(nc | ((ch - '0') << 4));
      } else {
        bout.nl();
        bout.outstr("|#9Inversed? ");
        if (bin.yesno()) {
          if ((a()->user()->bwcolor(1) & 0x70) == 0) {
            nc = static_cast<uint8_t>(0 | ((a()->user()->bwcolor(1) & 0x07) << 4));
          } else {
            nc = static_cast<uint8_t>(a()->user()->bwcolor(1) & 0x70);
          }
        } else {
          if ((a()->user()->bwcolor(1) & 0x70) == 0) {
            nc = static_cast<uint8_t>(0 | (a()->user()->bwcolor(1) & 0x07));
          } else {
            nc = static_cast<uint8_t>((a()->user()->bwcolor(1) & 0x70) >> 4);
          }
        }
      }
      bout.outstr("|#9Bold? ");
      if (bin.yesno()) {
        nc |= 0x08;
      }

      bout.outstr("|#9Blinking? ");
      if (bin.yesno()) {
        nc |= 0x80;
      }

      bout.nl(2);
      bout.setc(nc);
      bout.outstr(DescribeColorCode(nc));
      bout.ansic(0);
      bout.nl(2);
      bout.outstr("|#8Is this OK? ");
      if (bin.yesno()) {
        bout.outstr("\r\nColor saved.\r\n\n");
        if (a()->user()->color()) {
          a()->user()->color(color_num, nc);
        } else {
          a()->user()->bwcolor(color_num, nc);
        }
      } else {
        bout.outstr("\r\nNot saved, then.\r\n\n");
      }
    }
  } while (!done && !a()->sess().hangup());
}

void l_config_qscan() {
  bool abort = false;
  bout.outstr("\r\n|#9Boards to q-scan marked with '*'|#0\r\n\n");
  for (size_t i = 0; i < a()->usub.size() && !abort; i++) {
    const auto ch =
        (a()->sess().qsc_q[a()->usub[i].subnum / 32] & (1L << (a()->usub[i].subnum % 32))) ? '*'
                                                                                           : ' ';
    bout.bpla(
        fmt::format("{} {}. {}", ch, a()->usub[i].keys, a()->subs().sub(a()->usub[i].subnum).name),
        &abort);
  }
  bout.nl(2);
}

void config_qscan() {
  if (okansi()) {
    config_scan_plus(QSCAN);
    return;
  }

  const auto oc = a()->sess().current_user_sub_conf_num();
  const auto os = a()->current_user_sub().subnum;

  bool done;
  bool done1 = false;
  do {
    char ch;
    if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
      std::string conf_list{" "};
      auto abort = false;
      bout.outstr("\r\nSelect Conference: \r\n\n");
      for (const auto& c : a()->uconfsub) {
        bout.bpla(fmt::format("{}) {}", c.key.key(), stripcolors(c.conf_name)), &abort);
        conf_list.push_back(c.key.key());
      }
      bout.nl();
      bout.print("Select [{}, <space> to quit]: ", conf_list.substr(1));
      ch = onek(conf_list);
    } else {
      ch = '-';
    }
    switch (ch) {
    case ' ':
      done1 = true;
      break;
    default:
      if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
        auto& conf = a()->all_confs().subs_conf();
        const auto o = conf.try_conf(ch);
        if (!o) {
          break;
        }
        setuconf(conf, o.value().key.key(), -1);
      }
      l_config_qscan();
      done = false;
      do {
        bout.nl();
        bout.outstr("|#2Enter message base number (|#1C=Clr All, Q=Quit, S=Set All|#2): ");
        std::string s = mmkey(MMKeyAreaType::subs);
        if (!s.empty()) {
          for (size_t i = 0; i < a()->usub.size(); i++) {
            if (s == a()->usub[i].keys) {
              a()->sess().qsc_q[a()->usub[i].subnum / 32] ^= (1L << (a()->usub[i].subnum % 32));
            }
            if (s == "S") {
              a()->sess().qsc_q[a()->usub[i].subnum / 32] |= (1L << (a()->usub[i].subnum % 32));
            }
            if (s == "C") {
              a()->sess().qsc_q[a()->usub[i].subnum / 32] &= ~(1L << (a()->usub[i].subnum % 32));
            }
          }
          if (s == "Q") {
            done = true;
          }
          if (s == "?") {
            l_config_qscan();
          }
        }
      } while (!done && !a()->sess().hangup());
      break;
    }
    if (!ok_multiple_conf(a()->user(), a()->uconfsub)) {
      done1 = true;
    }

  } while (!done1 && !a()->sess().hangup());

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, oc, os);
  }
}

static void list_macro(const std::string& s) {
  int i = 0;
  const char* macro_text = s.c_str();

  while (i < 80 && macro_text[i] != 0) {
    if (macro_text[i] >= 32) {
      bout.outchr(macro_text[i]);
    } else {
      if (macro_text[i] == 16) {
        bout.ansic(macro_text[++i] - 48);
      } else {
        switch (macro_text[i]) {
        case RETURN:
          bout.outchr('|');
          break;
        case TAB:
          bout.outchr('\xF9');
          break;
        default:
          bout.outchr('^');
          bout.outchr(static_cast<char>(macro_text[i] + 64));
          break;
        }
      }
    }
    ++i;
  }
  bout.nl();
}

static void macroedit(char *macro_text) {
  *macro_text = '\0';
  bout.nl();
  bout.outstr("|#5Enter your macro, press |#7[|#1CTRL-Z|#7]|#5 when finished.\r\n\n");
  bin.okskey(false);
  bout.ansic(0);
  bool done = false;
  int i = 0;
  bool toggle = false;
  int textclr = 0;
  do {
    switch (const char ch = bin.getkey(); ch) {
    case CZ:
      done = true;
      break;
    case BACKSPACE:
      bout.bs();
      i--;
      if (i < 0) {
        i = 0;
      }
      macro_text[i] = '\0';
      break;
    case CP:
      macro_text[i++] = ch;
      toggle = true;
      break;
    case RETURN:
      macro_text[i++] = ch;
      bout.ansic(0);
      bout.outchr('|');
      bout.ansic(textclr);
      break;
    case TAB:
      macro_text[i++] = ch;
      bout.ansic(0);
      bout.outchr('\xF9') ;
      bout.ansic(textclr);
      break;
    default:
      macro_text[i++] = ch;
      if (toggle) {
        toggle = false;
        textclr = ch - 48;
        bout.ansic(textclr);
      } else {
        bout.outchr(ch);
      }
      break;
    }
    macro_text[i + 1] = 0;
  } while (!done && i < 80 && !a()->sess().hangup());
  bin.okskey(true);
  bout.ansic(0);
  bout.nl();
  bout.outstr("|#9Is this okay? ");
  if (!bin.yesno()) {
    *macro_text = '\0';
  }
}

void change_macros() {
  char szMacro[255], ch;
  bool done = false;

  do {
    bout.outchr(CL);
    bout.outstr("|#4Macro A: \r\n");
    list_macro(a()->user()->macro(2));
    bout.nl();
    bout.outstr("|#4Macro D: \r\n");
    list_macro(a()->user()->macro(0));
    bout.nl();
    bout.outstr("|#4Macro F: \r\n");
    list_macro(a()->user()->macro(1));
    bout.nl(2);
    bout.outstr("|#9Macro to edit or Q:uit (A,D,F,Q) : |#0");
    ch = onek("QADF");
    szMacro[0] = 0;
    switch (ch) {
    case 'A':
      macroedit(szMacro);
      if (szMacro[0]) {
        a()->user()->macro(2, szMacro);
      }
      break;
    case 'D':
      macroedit(szMacro);
      if (szMacro[0]) {
        a()->user()->macro(0, szMacro);
      }
      break;
    case 'F':
      macroedit(szMacro);
      if (szMacro[0]) {
        a()->user()->macro(1, szMacro);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !a()->sess().hangup());
}

void change_password() {
  bout.nl();
  bout.outstr("|#9Change password? ");
  if (!bin.yesno()) {
    return;
  }

  bout.nl();
  std::string password = bin.input_password("|#9You must now enter your current password.\r\n|#7: ", 8);
  if (password != a()->user()->password()) {
    bout.outstr("\r\nIncorrect.\r\n\n");
    return;
  }
  bout.nl(2);
  password = bin.input_password("|#9Enter your new password, 3 to 8 characters long.\r\n|#7: ", 8);
  bout.nl(2);
  std::string password2 = bin.input_password("|#9Repeat password for verification.\r\n|#7: ", 8);
  if (password == password2) {
    if (password2.length() < 3) {
      bout.nl();
      bout.outstr("|#6Password must be 3-8 characters long.\r\n|#6Password was not changed.\r\n\n");
    } else {
      a()->user()->password(password);
      bout.outstr("\r\n|#1Password changed.\r\n\n");
      sysoplog("Changed Password.");
    }
  } else {
    bout.outstr("\r\n|#6VERIFY FAILED.\r\n|#6Password not changed.\r\n\n");
  }
}

void modify_mailbox() {
  bout.nl();
  bout.outstr("|#9Do you want to close your mailbox? ");
  if (bin.yesno()) {
    bout.outstr("|#5Are you sure? ");
    if (bin.yesno()) {
      a()->user()->close_mailbox();
      return;
    }
  }
  bout.outstr("|#5Do you want to forward your mail? ");
  if (!bin.yesno()) {
    a()->user()->clear_mailbox_forward();
    return;
  }
  
  if (const auto network_number = getnetnum_by_type(network_type_t::internet); network_number != -1) {
    a()->set_net_num(network_number);
    bout.outstr("|#5Do you want to forward to your Internet address? ");
    if (bin.yesno()) {
      bout.outstr("|#3Enter the Internet E-Mail Address.\r\n|#9:");
      if (const auto entered_address = bin.input_text(a()->user()->email_address(), 75);
          check_inet_addr(entered_address)) {
        a()->user()->email_address(entered_address);
        a()->user()->forward_netnum(network_number);
        a()->user()->forward_systemnum(INTERNET_EMAIL_FAKE_OUTBOUND_NODE);
        bout.outstr("\r\nSaved.\r\n\n");
      }
      return;
    }
  }

  bout.nl();
  bout.outstr("|#2Forward to? ");
  const auto entered_forward_to = bin.input_upper(40);

  auto [tu, ts] = parse_email_info(entered_forward_to);
  a()->user()->forward_usernum(tu);
  a()->user()->forward_systemnum(ts);
  if (a()->user()->forward_systemnum() != 0) {
    a()->user()->forward_netnum(a()->net_num());
    if (a()->user()->forward_usernum() == 0) {
      a()->user()->clear_mailbox_forward();
      bout.outstr("\r\nCan't forward to a user name, must use user number.\r\n\n");
    }
  } else if (a()->user()->forward_usernum() == a()->sess().user_num()) {
    bout.outstr("\r\nCan't forward to yourself.\r\n\n");
    a()->user()->forward_usernum(0);
  }

  bout.nl();
  if (a()->user()->forward_usernum() == 0
      && a()->user()->forward_systemnum() == 0) {
    a()->user()->forward_netnum(0);
    bout.outstr("Forwarding reset.");
  } else {
    bout.outstr("Saved.");
  }
  bout.nl(2);
}

void change_optional_lines() {
  bout.outstr("|#9You may specify your optional lines value from 0-10,\r\n"
            "|#20 |#9being all, |#210 |#9being none.\r\n"
            "|#2What value? ");
  if (const auto r = bin.input_number_hotkey(a()->user()->optional_val(), {'Q'}, 0, 10);
      r.key != 'Q') {
    a()->user()->optional_val(r.num);
  }
}

void enter_regnum() {
  bout.outstr("|#7Enter your WWIV registration number, or enter '|#20|#7' for none.\r\n|#0:");
  const auto regnum = bin.input_number(a()->user()->wwiv_regnum());
  a()->user()->wwiv_regnum(regnum);
  changedsl();
}

void change_email_address() {
  bout.nl();
  bout.outstr("|#9Enter your Internet mailing address.\r\n|#7:");
  if (const auto address = bin.input_text(65); !address.empty()) {
    if (check_inet_addr(address)) {
      a()->user()->email_address(address);
    } else {
      bout.outstr("\r\n|#6Invalid address format.\r\n\n");
      bout.pausescr();
    }
  } else {
    bout.outstr("|#5Delete Internet address? ");
    if (bin.yesno()) {
      a()->user()->email_address("");
    }
  }
}

void defaults(bool& need_menu_reload) {
  auto done = false;
  do {
    print_cur_stat();
    a()->tleft(true);
    if (a()->sess().hangup()) {
      return;
    }
    bout.nl();
    bout.outstr("|#9Defaults: ");
    std::string allowable = "Q?1234567BCDIKMNTUW";
    if (okansi()) {
      allowable.append("89AS");
      if (a()->fullscreen_read_prompt()) {
        allowable.push_back('G');
      }
    }
    switch (const auto ch = onek(allowable, true); ch) {
    case 'Q':
      done = true;
      break;
    case '?':
      print_cur_stat();
      break;
    case '1':
      input_screensize();
      break;
    case '2':
      input_ansistat();
      if (!okansi()) {
        a()->user()->clear_flag(User::fullScreenReader);
      }
      break;
    case '3':
      a()->user()->toggle_flag(User::pauseOnPage);
      break;
    case '4':
      modify_mailbox();
      break;
    case '5':
      config_qscan();
      break;
    case '6':
      change_password();
      break;
    case '7':
      change_macros();
      break;
    case '8':
      change_colors();
      break;
    case '9':
      select_editor();
      break;
    case 'A':
      a()->user()->toggle_flag(User::extraColor);
      break;
    case 'B':
      change_optional_lines();
      break;
    case 'C':
      a()->user()->toggle_flag(User::conference);
      changedsl();
      break;
    case 'D':
      a()->user()->toggle_flag(User::msg_show_controlcodes);
      break;
    case 'G':
      a()->user()->toggle_flag(User::fullScreenReader);
      break;
    case 'I': {
      change_email_address();
    }
    break;
    case 'K':
      wwiv::bbs::menus::ConfigUserMenuSet("");
      need_menu_reload = true;
      break;
    case 'M':
      a()->user()->toggle_flag(User::noMsgs);
      break;
    case 'N':
      wwiv::bbs::qwk::qwk_config_user();
      break;
    case 'S':
      a()->user()->toggle_flag(User::clearScreen);
      break;
    case 'T':
      a()->user()->toggle_flag(User::twentyFourHourClock);
      break;
    case 'U':
      a()->user()->toggle_flag(User::autoQuote);
      break;
    case 'W':
      enter_regnum();
      break;
    }
  } while (!done && !a()->sess().hangup());
  a()->WriteCurrentUser();
}

// private function used by list_config_scan_plus and drawscan
static int GetMaxLinesToShowForScanPlus() {
  return (a()->user()->screen_lines() - (4 + STOP_LIST) >
          MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) ?
          MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) :
          a()->user()->screen_lines() - (4 + STOP_LIST));
}

static void list_config_scan_plus(int first, int *amount, int type) {
  auto& confsubdir = type == NSCAN ? a()->uconfdir : a()->uconfsub;
  bool bUseConf = ok_multiple_conf(a()->user(), confsubdir);

  bout.cls();
  bout.clear_lines_listed();

  if (bUseConf) {
    std::string name;
    if (type == QSCAN) {
      name = a()->uconfsub[a()->sess().current_user_sub_conf_num()].conf_name;
    }
    else {
      name = a()->uconfdir[a()->sess().current_user_dir_conf_num()].conf_name;
    }
    const auto s = trim_to_size_ignore_colors(name, 26);
    bout.printf("|#1Configure |#2%cSCAN |#9-- |#2%-26s |#9-- |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n",
                 type == 0 ? 'Q' : 'N', s, type == 0 ? "sub" : "dir");
  } else {
    bout.printf("|#1Configure |#2%cSCAN                                   |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n",
                 type == 0 ? 'Q' : 'N', type == 0 ? "sub" : "dir");
  }
  bout.ansic(7);
  bout.outstr(std::string(79, '\xC4'));
  bout.nl();

  const auto max_lines = GetMaxLinesToShowForScanPlus();

  if (type == QSCAN) {
    for (size_t this_sub = first; this_sub < a()->usub.size() && *amount < max_lines * 2;
         this_sub++) {
      bout.clear_lines_listed();
      auto s = fmt::sprintf("|#7[|#1%c|#7] |#9%s",
              (a()->sess().qsc_q[a()->usub[this_sub].subnum / 32] & (1L << (a()->usub[this_sub].subnum % 32))) ? '\xFE' : ' ',
              a()->subs().sub(a()->usub[this_sub].subnum).name);
      if (s.length() > 44) {
        s.resize(44);
      }
      if (*amount >= max_lines) {
        bout.goxy(40, 3 + *amount - max_lines);
        bout.outstr(s);
      } else {
        bout.outstr(s);
        bout.nl();
      }
      ++*amount;
    }
  } else {
    for (auto this_dir = first; this_dir < wwiv::stl::size_int(a()->udir) && *amount < max_lines * 2;
         this_dir++) {
      bout.clear_lines_listed();
      const int alias_dir = a()->udir[this_dir].subnum;
      auto s =
          fmt::sprintf("|#7[|#1%c|#7] |#2%s",
                       a()->sess().qsc_n[alias_dir / 32] & (1L << (alias_dir % 32)) ? '\xFE' : ' ',
                       a()->dirs()[alias_dir].name);
      if (s.length() > 44) {
        s.resize(44);
      }
      if (*amount >= max_lines) {
        bout.goxy(40, 3 + *amount - max_lines);
        bout.outstr(s);
      } else {
        bout.outstr(s);
        bout.nl();
      }
      ++*amount;
    }
  }
  bout.nl();
  bout.clear_lines_listed();
}

static void drawscan(int filepos, bool tagged) {
  const auto max_lines = GetMaxLinesToShowForScanPlus();
  if (filepos >= max_lines) {
    bout.goxy(40, 3 + filepos - max_lines);
  } else {
    bout.goxy(1, filepos + 3);
  }

  bout.setc(static_cast<uint8_t>(wwiv::sdk::Color::BLACK) + (static_cast<uint8_t>(wwiv::sdk::Color::CYAN) << 4));
  bout.print("[{:c}]", tagged ? '\xFE' : ' ');
  bout.setc(static_cast<uint8_t>(wwiv::sdk::Color::YELLOW) + (static_cast<uint8_t>(wwiv::sdk::Color::BLACK) << 4));

  if (filepos >= max_lines) {
    bout.goxy(41, 3 + filepos - max_lines);
  } else {
    bout.goxy(2, filepos + 3);
  }
}

static void undrawscan(int filepos, long tagged) {
  const auto max_lines = GetMaxLinesToShowForScanPlus();

  if (filepos >= max_lines) {
    bout.goxy(40, 3 + filepos - max_lines);
  } else {
    bout.goxy(1, filepos + 3);
  }
  bout.printf("|#7[|#1%c|#7]", tagged ? '\xFE' : ' ');
}

static bool is_inscan(int dir) {
  auto sysdir = false;
  if (a()->udir[0].keys == "0") {
    sysdir = true;
  }

  for (auto this_dir = 0; this_dir < size_int(a()->udir); this_dir++) {
    const auto key = std::to_string(sysdir ? dir : dir + 1);
    if (key == a()->udir[this_dir].keys) {
      const auto ad = a()->udir[this_dir].subnum;
      return (a()->sess().qsc_n[ad / 32] & 1L << ad % 32) != 0;
    }
  }
  return false;
}

void config_scan_plus(int type) {
  int command;
  int top = 0;
  int amount = 0, pos = 0, side_pos = 0;
  side_menu_colors smc{};

  auto& confsubdir = type == NSCAN ? a()->uconfdir : a()->uconfsub;
  int useconf = ok_multiple_conf(a()->user(), confsubdir);
  bout.localIO()->topdata(LocalIO::topdata_t::none);
  a()->UpdateTopScreen();

  std::vector<std::string> menu_items = { "Next",  "Previous", "Toggle", "Clear All", "Set All" };

  if (type == 0) {
    menu_items.emplace_back("Read New");
  } else {
    menu_items.emplace_back("List");
  }

  if (useconf) {
    menu_items.emplace_back("{ Conf");
    menu_items.emplace_back("} Conf");
    menu_items.emplace_back("Quit");
    menu_items.emplace_back("?");
  } else {
    menu_items.emplace_back("Quit");
    menu_items.emplace_back("?");
  }
  bool done = false;
  while (!done && !a()->sess().hangup()) {
    amount = 0;
    list_config_scan_plus(top, &amount, type);
    if (!amount) {
      top = 0;
      list_config_scan_plus(top, &amount, type);
      if (!amount) {
        done = true;
      }
    }
    if (!done) {
      drawscan(pos, type ? is_inscan(top + pos) :
               a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
    }
    bool redraw = true;
    bool menu_done = false;
    while (!menu_done && !a()->sess().hangup() && !done) {
      command = side_menu(&side_pos, redraw, menu_items, 1,
                          a()->user()->screen_lines() - STOP_LIST > MAX_SCREEN_LINES_TO_SHOW - STOP_LIST ?
                          MAX_SCREEN_LINES_TO_SHOW - STOP_LIST :
                          a()->user()->screen_lines() - STOP_LIST, &smc);
      bout.clear_lines_listed();
      redraw = true;
      bout.ansic(0);
      if (do_sysop_command(command)) {
        menu_done = true;
        amount = 0;
      }
      switch (command) {
      case '?':
      case CO:
        bout.cls();
        bout.print_help_file(SCONFIG_HLP);
        bout.pausescr();
        menu_done = true;
        amount = 0;
        break;
      case COMMAND_DOWN:
        undrawscan(pos, type ? is_inscan(top + pos) :
                   a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
        ++pos;
        if (pos >= amount) {
          pos = 0;
        }
        drawscan(pos, type ? is_inscan(top + pos) :
                 a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case COMMAND_UP:
        undrawscan(pos, type ? is_inscan(top + pos) : a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] &
                                   (1L << (a()->usub[top + pos].subnum % 32)));
        if (!pos) {
          pos = amount - 1;
        } else {
          --pos;
        }
        drawscan(pos, type ? is_inscan(top + pos) : a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] &
                                 (1L << (a()->usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case SPACE: {
        if (type == 0) {
          a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] ^=
              (1L << (a()->usub[top + pos].subnum % 32));
        } else {
          auto sysdir = a()->udir[0].keys == "0";
          for (auto this_dir = 0; this_dir < size_int(a()->udir); this_dir++) {
            const auto s = std::to_string(sysdir ? top + pos : top + pos + 1);
            if (s == a()->udir[this_dir].keys) {
              int ad = a()->udir[this_dir].subnum;
              a()->sess().qsc_n[ad / 32] ^= (1L << (ad % 32));
            }
          }
        }
        drawscan(pos, type ? is_inscan(top + pos) : a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] &
                                 (1L << (a()->usub[top + pos].subnum % 32)));
        redraw = false;
      } break;
      case EXECUTE:
        if (!useconf && side_pos > 5) {
          side_pos += 2;
        }
        switch (side_pos) {
        case 0:
          top += amount;
          if (type == 0) {
            if (top >= size_int(a()->usub)) {
              top = 0;
            }
          } else {
            if (top >= size_int(a()->udir)) {
              top = 0;
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 1:
          if (top > a()->user()->screen_lines() - 4) {
            top -= a()->user()->screen_lines() - 4;
          } else {
            top = 0;
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 2:
          if (type == 0) {
            a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] ^=
                (1L << (a()->usub[top + pos].subnum % 32));
          } else {
            bool sysdir = a()->udir[0].keys == "0";
            for (int this_dir = 0; this_dir < wwiv::stl::size_int(a()->udir); this_dir++) {
              const auto s = fmt::format("{}", sysdir ? top + pos : top + pos + 1);
              if (s == a()->udir[this_dir].keys) {
                int ad = a()->udir[this_dir].subnum;
                a()->sess().qsc_n[ad / 32] ^= (1L << (ad % 32));
              }
            }
          }
          drawscan(pos, type ? is_inscan(top + pos)
                             : a()->sess().qsc_q[a()->usub[top + pos].subnum / 32] &
                                   (1L << (a()->usub[top + pos].subnum % 32)));
          redraw = false;
          break;
        case 3:
          if (type == 0) {
            for (size_t this_sub = 0; this_sub < a()->usub.size(); this_sub++) {
              if (a()->sess().qsc_q[a()->usub[this_sub].subnum / 32] &
                  (1L << (a()->usub[this_sub].subnum % 32))) {
                a()->sess().qsc_q[a()->usub[this_sub].subnum / 32] ^=
                    (1L << (a()->usub[this_sub].subnum % 32));
              }
            }
          } else {
            for (auto this_dir = 0; this_dir < size_int(a()->udir); this_dir++) {
              if (a()->sess().qsc_n[a()->udir[this_dir].subnum / 32] &
                  (1L << (a()->udir[this_dir].subnum % 32))) {
                a()->sess().qsc_n[a()->udir[this_dir].subnum / 32] ^=
                    1L << (a()->udir[this_dir].subnum % 32);
              }
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 4:
          if (type == 0) {
            for (auto this_sub = 0; this_sub < size_int(a()->usub); this_sub++) {
              if (!(a()->sess().qsc_q[a()->usub[this_sub].subnum / 32] &
                    (1L << (a()->usub[this_sub].subnum % 32)))) {
                a()->sess().qsc_q[a()->usub[this_sub].subnum / 32] ^=
                    (1L << (a()->usub[this_sub].subnum % 32));
              }
            }
          } else {
            for (auto this_dir = 0; this_dir < wwiv::stl::size_int(a()->udir); this_dir++) {
              if (!(a()->sess().qsc_n[a()->udir[this_dir].subnum / 32] &
                    (1L << (a()->udir[this_dir].subnum % 32)))) {
                a()->sess().qsc_n[a()->udir[this_dir].subnum / 32] ^=
                    1L << (a()->udir[this_dir].subnum % 32);
              }
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 5:
          if (type == 0) {
            bool nextsub = false;
            qscan(static_cast<uint16_t>(top + pos), nextsub);
          } else {
            auto cudn_saved = a()->current_user_dir_num();
            a()->set_current_user_dir_num(static_cast<uint16_t>(top + pos));
            listfiles();
            a()->set_current_user_dir_num(cudn_saved);
          }
          menu_done = true;
          amount = 0;
          break;
        case 6:
          if (okconf(a()->user())) {
            if (type == 0) {
              auto confnum = a()->sess().current_user_sub_conf_num();
              if (confnum > 0) {
                a()->sess().set_current_user_sub_conf_num(confnum - 1);
              } else {
                a()->sess().set_current_user_sub_conf_num(size_int(a()->uconfsub)-1);
              }
              setuconf(ConferenceType::CONF_SUBS, a()->sess().current_user_sub_conf_num(), -1);
            } else {
              auto confnum = a()->sess().current_user_dir_conf_num();
              if (confnum > 0) {
                a()->sess().set_current_user_dir_conf_num(confnum - 1);
              } else {
                a()->sess().set_current_user_dir_conf_num(size_int(a()->uconfdir)-1);
              }
              setuconf(ConferenceType::CONF_DIRS, a()->sess().current_user_dir_conf_num(), -1);
            }
            pos = 0;
            menu_done = true;
            amount = 0;
          }
          break;
        case 7:
          if (okconf(a()->user())) {
            if (type == 0) {
              int confnum = a()->sess().current_user_sub_conf_num();
              if (confnum < size_int(a()->uconfsub) - 1) {
                a()->sess().set_current_user_sub_conf_num(confnum + 1);
              } else {
                a()->sess().set_current_user_sub_conf_num(0);
              }
              setuconf(ConferenceType::CONF_SUBS, a()->sess().current_user_sub_conf_num(), -1);
            }

            else {
              int confnum = a()->sess().current_user_dir_conf_num();
              if (confnum < size_int(a()->uconfdir) - 1) {
                a()->sess().set_current_user_dir_conf_num(confnum + 1);
              } else {
                a()->sess().set_current_user_dir_conf_num(0);
              }
              setuconf(ConferenceType::CONF_DIRS, a()->sess().current_user_dir_conf_num(), -1);
            }
            pos = 0;
            menu_done = true;
            amount = 0;
          }
          break;
        case 8:
          menu_done = true;
          done = true;
          break;
        case 9:
          bout.cls();
          bout.print_help_file(SCONFIG_HLP);
          bout.pausescr();
          menu_done = true;
          amount = 0;
          if (!useconf) {
            side_pos -= 2;
          }
          break;
        }
        break;
      case GET_OUT:
        menu_done = true;
        done = true;
        break;
      }
    }
  }
  bout.clear_lines_listed();
  bout.nl();
}
