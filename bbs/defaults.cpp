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
#include "bbs/defaults.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsovl3.h"
#include "bbs/com.h"
#include "bbs/common.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/fcns.h"
#include "bbs/instmsg.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "bbs/menu.h"
#include "bbs/misccmd.h"
#include "bbs/mmkey.h"
#include "bbs/msgbase1.h"
#include "bbs/newuser.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "bbs/wconstants.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::setw;
using std::endl;
using std::left;
using std::string;
using std::vector;
using wwiv::bbs::InputMode;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static const int STOP_LIST = 0;
static const int MAX_SCREEN_LINES_TO_SHOW = 24;

// Undefine this so users can not toggle the sysop sub on and off
// #define NOTOGGLESYSOP

void select_editor() {
  if (a()->editors.empty()) {
    bout << "\r\nNo full screen editors available.\r\n\n";
    return;
  } else if (a()->editors.size() == 1) {
    if (a()->user()->GetDefaultEditor() == 0) {
      a()->user()->SetDefaultEditor(1);
    } else {
      a()->user()->SetDefaultEditor(0);
      a()->user()->ClearStatusFlag(User::autoQuote);
    }
    return;
  }
  bout << "0. Normal non-full screen editor\r\n";
  std::set<char> odc;
  for (size_t i = 0; i < a()->editors.size(); i++) {
    bout << i + 1 << ". " << a()->editors[i].description  << wwiv::endl;
    if (((i + 1) % 10) == 0) {
      odc.insert(static_cast<char>((i + 1) / 10));
    }
  }
  bout.nl();
  bout << "|#9Which editor (|#31-" << a()->editors.size() << ", <Q>=leave as is|#9) ? ";
  string ss = mmkey(odc);

  int nEditor = StringToInt(ss);
  if (nEditor >= 1 && nEditor <= size_int(a()->editors)) {
    a()->user()->SetDefaultEditor(nEditor);
  } else if (ss == "0") {
    a()->user()->SetDefaultEditor(0);
    a()->user()->ClearStatusFlag(User::autoQuote);
  }
}

static string GetMailBoxStatus() {
  if (a()->user()->GetForwardSystemNumber() == 0 &&
      a()->user()->GetForwardUserNumber() == 0) {
    return string("Normal");
  }
  if (a()->user()->GetForwardSystemNumber() != 0) {
    if (a()->user()->IsMailboxForwarded()) {
      return StringPrintf("Forward to #%u @%u.%s.",
              a()->user()->GetForwardUserNumber(),
              a()->user()->GetForwardSystemNumber(),
              a()->net_networks[ a()->user()->GetForwardNetNumber() ].name);
    } else {
      string fwd_username;
      read_inet_addr(fwd_username, a()->usernum);
      return StrCat("Forwarded to Internet ", fwd_username);
    }
  }

  if (a()->user()->IsMailboxClosed()) {
    return string("Closed");
  }

  User ur;
  a()->users()->ReadUser(&ur, a()->user()->GetForwardUserNumber());
  if (ur.IsUserDeleted()) {
    a()->user()->SetForwardUserNumber(0);
    return string("Normal");
  }
  return StrCat("Forward to ", a()->names()->UserName(a()->user()->GetForwardUserNumber()));
}

static void print_cur_stat() {
  bout.cls();
  bout.litebar("Your Preferences");
  bout << left;
  const string screen_size = StringPrintf("%d X %d", 
      a()->user()->GetScreenChars(),
      a()->user()->GetScreenLines());
  const string ansi_setting = (a()->user()->HasAnsi() ?
          (a()->user()->HasColor() ? "Color" : "Monochrome") : "No ANSI");
  bout << "|#11|#9) Screen size       : |#2" << setw(16) << screen_size << " " 
       << "|#12|#9) ANSI              : |#2" << ansi_setting << wwiv::endl;

  const string mailbox_status = GetMailBoxStatus();
  bout << "|#13|#9) Pause on screen   : |#2" << setw(16) << (a()->user()->HasPause() ? "On " : "Off") << " "
       << "|#14|#9) Mailbox           : |#2" << mailbox_status << wwiv::endl;

  bout << setw(45) << "|#15|#9) Configured Q-scan" << " " << setw(45) << "|#16|#9) Change password" << wwiv::endl;

  if (okansi()) {
    bout << setw(45) << "|#17|#9) Update macros" << " "
         << setw(45) << "|#18|#9) Change colors" << wwiv::endl;

    unsigned int nEditorNum = a()->user()->GetDefaultEditor();
    const string editor_name = (nEditorNum > 0 && nEditorNum <= a()->editors.size()) ?
        a()->editors[nEditorNum-1].description : "None";
     bout << "|#19|#9) Full screen editor: |#2" << setw(16) << editor_name << " " 
          << "|#1A|#9) Extended colors   : |#2" << YesNoString(a()->user()->IsUseExtraColor()) << wwiv::endl;
  } else {
    bout << "|#17|#9) Update macros" << wwiv::endl;
  }

  const string internet_email_address = 
      ((a()->user()->GetEmailAddress()[0] == '\0') ? "None." : a()->user()->GetEmailAddress());
  bout << "|#1B|#9) Optional lines    : |#2" << setw(16) << a()->user()->GetOptionalVal() << " "
       << "|#1C|#9) Conferencing      : |#2" << YesNoString(a()->user()->IsUseConference()) << wwiv::endl;
  if (a()->experimental_read_prompt()) {
    bout << "|#1G|#9) Message Reader    : |#2" << (a()->user()->HasStatusFlag(User::fullScreenReader) ? "Full-Screen" : "Traditional") << wwiv::endl;;
  }
  bout << "|#1I|#9) Internet Address  : |#2" << internet_email_address << wwiv::endl;
  bout << "|#1K|#9) Configure Menus" << wwiv::endl;
  if (a()->languages.size() > 1) {
    bout<< "|#1L|#9) Language          : |#2" << setw(16) << a()->cur_lang_name << " ";
  }
  if (num_instances() > 1) {
    bout << "|#1M|#9) Allow user msgs   : |#2" << YesNoString(!a()->user()->IsIgnoreNodeMessages());
  }
  bout.nl();
  bout << "|#1S|#9) Cls Between Msgs? : |#2" << setw(16) << YesNoString(a()->user()->IsUseClearScreen()) << " "
       << "|#1T|#9) 12hr or 24hr clock: |#2" << (a()->user()->IsUse24HourClock() ? "24hr" : "12hr") << wwiv::endl;

  string wwiv_regnum = "(None)";
  if (a()->user()->GetWWIVRegNumber()) {
    wwiv_regnum = StringPrintf("%ld", a()->user()->GetWWIVRegNumber());
  }
  bout << "|#1U|#9) Use Msg AutoQuote : |#2" << setw(16) << YesNoString(a()->user()->IsUseAutoQuote()) << " "
       << "|#1W|#9) WWIV reg num      : |#2" << wwiv_regnum << wwiv::endl;

  bout << "|#1Q|#9) Quit to main menu\r\n";
}

static const string DisplayColorName(int c) {
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

const string DescribeColorCode(int nColorCode) {
  std::ostringstream os;

  if (a()->user()->HasColor()) {
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
    bout.SystemColor(static_cast<unsigned char>((i == 0) ? 0x70 : i));
    bout << i << ". " << DisplayColorName(static_cast<char>(i)).c_str() << "|#0" << wwiv::endl;
  }
}

static void reset_user_colors_to_defaults() {
  for (int i = 0; i <= 9; i++) {
    a()->user()->SetColor(i, a()->newuser_colors[i]);
    a()->user()->SetBWColor(i, a()->newuser_bwcolors[i]);
  }
}

static void change_colors() {
  bool done = false;
  bout.nl();
  do {
    bout.cls();
    bout << "|#5Customize Colors:";
    bout.nl(2);
    if (!a()->user()->HasColor()) {
      std::ostringstream os;
      os << "Monochrome base color : ";
      if ((a()->user()->GetBWColor(1) & 0x70) == 0) {
        os << DisplayColorName(a()->user()->GetBWColor(1) & 0x07);
      } else {
        os << DisplayColorName((a()->user()->GetBWColor(1) >> 4) & 0x07);
      }
      bout << os.str();
      bout.nl(2);
    }
    for (int i = 0; i < 10; i++) {
      std::ostringstream os;
      bout.Color(i);
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
      if (a()->user()->HasColor()) {
        os << DescribeColorCode(a()->user()->GetColor(i));
      } else {
        os << DescribeColorCode(a()->user()->GetBWColor(i));
      }
      bout << os.str();
      bout.nl();
    }
    bout << "\r\n|#9[|#2R|#9]eset Colors to Default Values, [|#2Q|#9]uit\r\n";
    bout << "|#9Enter Color Number to Modify (|#20|#9-|#29|#9,|#2R|#9,|#2Q|#9): ";
    char ch = onek("RQ0123456789", true);
    if (ch == 'Q') {
      done = true;
    } else if (ch == 'R') {
      reset_user_colors_to_defaults();
    } else {
      char nc = 0;
      int nColorNum = ch - '0';
      if (a()->user()->HasColor()) {
        color_list();
        bout.Color(0);
        bout.nl();
        bout << "|#9(Q=Quit) Foreground? ";
        ch = onek("Q01234567");
        if (ch == 'Q') {
          continue;
        }
        nc = static_cast<char>(ch - '0');
        bout << "|#9(Q=Quit) Background? ";
        ch = onek("Q01234567");
        if (ch == 'Q') {
          continue;
        }
        nc = static_cast<char>(nc | ((ch - '0') << 4));
      } else {
        bout.nl();
        bout << "|#9Inversed? ";
        if (yesno()) {
          if ((a()->user()->GetBWColor(1) & 0x70) == 0) {
            nc = static_cast<char>(0 | ((a()->user()->GetBWColor(1) & 0x07) << 4));
          } else {
            nc = static_cast<char>(a()->user()->GetBWColor(1) & 0x70);
          }
        } else {
          if ((a()->user()->GetBWColor(1) & 0x70) == 0) {
            nc = static_cast<char>(0 | (a()->user()->GetBWColor(1) & 0x07));
          } else {
            nc = static_cast<char>((a()->user()->GetBWColor(1) & 0x70) >> 4);
          }
        }
      }
      bout << "|#9Bold? ";
      if (yesno()) {
        nc |= 0x08;
      }

      bout << "|#9Blinking? ";
      if (yesno()) {
        nc |= 0x80;
      }

      bout.nl(2);
      bout.SystemColor(nc);
      bout << DescribeColorCode(nc);
      bout.Color(0);
      bout.nl(2);
      bout << "|#8Is this OK? ";
      if (yesno()) {
        bout << "\r\nColor saved.\r\n\n";
        if (a()->user()->HasColor()) {
          a()->user()->SetColor(nColorNum, nc);
        } else {
          a()->user()->SetBWColor(nColorNum, nc);
        }
      } else {
        bout << "\r\nNot saved, then.\r\n\n";
      }
    }
  } while (!done && !hangup);
}

void l_config_qscan() {
  bool abort = false;
  bout << "\r\n|#9Boards to q-scan marked with '*'|#0\r\n\n";
  for (size_t i = 0; (i < a()->subs().subs().size()) && (a()->usub[i].subnum != -1) && !abort; i++) {
    pla(StringPrintf("%c %s. %s",
            (qsc_q[a()->usub[i].subnum / 32] & (1L << (a()->usub[i].subnum % 32))) ? '*' : ' ',
            a()->usub[i].keys,
            a()->subs().sub(a()->usub[i].subnum).name.c_str()), &abort);
  }
  bout.nl(2);
}

void config_qscan() {
  if (okansi()) {
    config_scan_plus(QSCAN);
    return;
  }

  int oc = a()->GetCurrentConferenceMessageArea();
  int os = a()->current_user_sub().subnum;

  bool done = false;
  bool done1 = false;
  do {
    char ch;
    if (okconf(a()->user()) && uconfsub[1].confnum != -1) {
      char szConfList[MAX_CONFERENCES + 2];
      bool abort = false;
      strcpy(szConfList, " ");
      bout << "\r\nSelect Conference: \r\n\n";
      size_t i = 0;
      while (i < subconfnum && uconfsub[i].confnum != -1 && !abort) {
        pla(StringPrintf("%c) %s", subconfs[uconfsub[i].confnum].designator,
                stripcolors(reinterpret_cast<char*>(subconfs[uconfsub[i].confnum].name))), &abort);
        szConfList[i + 1] = subconfs[uconfsub[i].confnum].designator;
        szConfList[i + 2] = 0;
        i++;
      }
      bout.nl();
      bout << "Select [" << &szConfList[1] << ", <space> to quit]: ";
      ch = onek(szConfList);
    } else {
      ch = '-';
    }
    switch (ch) {
    case ' ':
      done1 = true;
      break;
    default:
      if (okconf(a()->user())  && uconfsub[1].confnum != -1) {
        size_t i = 0;
        while ((ch != subconfs[uconfsub[i].confnum].designator) && (i < subconfnum)) {
          i++;
        }

        if (i >= subconfnum) {
          break;
        }

        setuconf(ConferenceType::CONF_SUBS, i, -1);
      }
      l_config_qscan();
      done = false;
      do {
        bout.nl();
        bout << "|#2Enter message base number (|#1C=Clr All, Q=Quit, S=Set All|#2): ";
        char* s = mmkey(0);
        if (s[0]) {
          for (size_t i = 0; (i < a()->subs().subs().size()) && (a()->usub[i].subnum != -1); i++) {
            if (IsEquals(a()->usub[i].keys, s)) {
              qsc_q[a()->usub[i].subnum / 32] ^= (1L << (a()->usub[i].subnum % 32));
            }
            if (IsEquals(s, "S")) {
              qsc_q[a()->usub[i].subnum / 32] |= (1L << (a()->usub[i].subnum % 32));
            }
            if (IsEquals(s, "C")) {
              qsc_q[a()->usub[i].subnum / 32] &= ~(1L << (a()->usub[i].subnum % 32));
            }
          }
          if (IsEquals(s, "Q")) {
            done = true;
          }
          if (IsEquals(s, "?")) {
            l_config_qscan();
          }
        }
      } while (!done && !hangup);
      break;
    }
    if (!okconf(a()->user()) || uconfsub[1].confnum == -1) {
      done1 = true;
    }

  } while (!done1 && !hangup);

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, oc, os);
  }
}

static void list_macro(const char *macro_text) {
  int i = 0;

  while ((i < 80) && (macro_text[i] != 0)) {
    if (macro_text[i] >= 32) {
      bout.bputch(macro_text[i]);
    } else {
      if (macro_text[i] == 16) {
        bout.Color(macro_text[++i] - 48);
      } else {
        switch (macro_text[i]) {
        case RETURN:
          bout.bputch('|');
          break;
        case TAB:
          bout.bputch('\xF9');
          break;
        default:
          bout.bputch('^');
          bout.bputch(static_cast<unsigned char>(macro_text[i] + 64));
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
  bout << "|#5Enter your macro, press |#7[|#1CTRL-Z|#7]|#5 when finished.\r\n\n";
  okskey = false;
  bout.Color(0);
  bool done = false;
  int i = 0;
  bool toggle = false;
  int textclr = 0;
  do {
    char ch = bout.getkey();
    switch (ch) {
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
      bout.Color(0);
      bout.bputch('|');
      bout.Color(textclr);
      break;
    case TAB:
      macro_text[i++] = ch;
      bout.Color(0);
      bout.bputch('\xF9') ;
      bout.Color(textclr);
      break;
    default:
      macro_text[i++] = ch;
      if (toggle) {
        toggle = false;
        textclr = ch - 48;
        bout.Color(textclr);
      } else {
        bout.bputch(ch);
      }
      break;
    }
    macro_text[i + 1] = 0;
  } while (!done && i < 80 && !hangup);
  okskey = true;
  bout.Color(0);
  bout.nl();
  bout << "|#9Is this okay? ";
  if (!yesno()) {
    *macro_text = '\0';
  }
}

static void make_macros() {
  char szMacro[255], ch;
  bool done = false;

  do {
    bout.bputch(CL);
    bout << "|#4Macro A: \r\n";
    list_macro(a()->user()->GetMacro(2));
    bout.nl();
    bout << "|#4Macro D: \r\n";
    list_macro(a()->user()->GetMacro(0));
    bout.nl();
    bout << "|#4Macro F: \r\n";
    list_macro(a()->user()->GetMacro(1));
    bout.nl(2);
    bout << "|#9Macro to edit or Q:uit (A,D,F,Q) : |#0";
    ch = onek("QADF");
    szMacro[0] = 0;
    switch (ch) {
    case 'A':
      macroedit(szMacro);
      if (szMacro[0]) {
        a()->user()->SetMacro(2, szMacro);
      }
      break;
    case 'D':
      macroedit(szMacro);
      if (szMacro[0]) {
        a()->user()->SetMacro(0, szMacro);
      }
      break;
    case 'F':
      macroedit(szMacro);
      if (szMacro[0]) {
        a()->user()->SetMacro(1, szMacro);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !hangup);
}

static void change_password() {
  bout.nl();
  bout << "|#9Change password? ";
  if (!yesno()) {
    return;
  }

  bout.nl();
  string password = input_password("|#9You must now enter your current password.\r\n|#7: ", 8);
  if (password != a()->user()->GetPassword()) {
    bout << "\r\nIncorrect.\r\n\n";
    return;
  }
  bout.nl(2);
  password = input_password("|#9Enter your new password, 3 to 8 characters long.\r\n|#7: ", 8);
  bout.nl(2);
  string password2 = input_password("|#9Repeat password for verification.\r\n|#7: ", 8);
  if (password == password2) {
    if (password2.length() < 3) {
      bout.nl();
      bout << "|#6Password must be 3-8 characters long.\r\n|#6Password was not changed.\r\n\n";
    } else {
      a()->user()->SetPassword(password);
      bout << "\r\n|#1Password changed.\r\n\n";
      sysoplog() << "Changed Password.";
    }
  } else {
    bout << "\r\n|#6VERIFY FAILED.\r\n|#6Password not changed.\r\n\n";
  }
}

static void modify_mailbox() {
  bout.nl();
  bout << "|#9Do you want to close your mailbox? ";
  if (yesno()) {
    bout << "|#5Are you sure? ";
    if (yesno()) {
      a()->user()->CloseMailbox();
      return;
    }
  }
  bout << "|#5Do you want to forward your mail? ";
  if (!yesno()) {
    a()->user()->ClearMailboxForward();
    return;
  }
  if (a()->user()->GetSl() >= syscfg.newusersl) {
    int network_number = getnetnum_by_type(network_type_t::internet);
    a()->set_net_num(network_number);
    if (network_number != -1) {
      set_net_num(a()->net_num());
      bout << "|#5Do you want to forward to your Internet address? ";
      if (yesno()) {
        bout << "|#3Enter the Internet E-Mail Address.\r\n|#9:";
        string entered_address = Input1(a()->user()->GetEmailAddress(), 75, true, InputMode::MIXED);
        if (check_inet_addr(entered_address)) {
          a()->user()->SetEmailAddress(entered_address.c_str());
          write_inet_addr(entered_address, a()->usernum);
          a()->user()->SetForwardNetNumber(a()->net_num());
          a()->user()->SetForwardToInternet();
          bout << "\r\nSaved.\r\n\n";
        }
        return;
      }
    }
  }
  bout.nl();
  bout << "|#2Forward to? ";
  string entered_forward_to = input(40);

  int nTempForwardUser, nTempForwardSystem;
  parse_email_info(entered_forward_to, &nTempForwardUser, &nTempForwardSystem);
  a()->user()->SetForwardUserNumber(nTempForwardUser);
  a()->user()->SetForwardSystemNumber(nTempForwardSystem);
  if (a()->user()->GetForwardSystemNumber() != 0) {
    a()->user()->SetForwardNetNumber(a()->net_num());
    if (a()->user()->GetForwardUserNumber() == 0) {
      a()->user()->ClearMailboxForward();
      bout << "\r\nCan't forward to a user name, must use user number.\r\n\n";
    }
  } else if (a()->user()->GetForwardUserNumber() == a()->usernum) {
    bout << "\r\nCan't forward to yourself.\r\n\n";
    a()->user()->SetForwardUserNumber(0);
  }

  bout.nl();
  if (a()->user()->GetForwardUserNumber() == 0
      && a()->user()->GetForwardSystemNumber() == 0) {
    a()->user()->SetForwardNetNumber(0);
    bout << "Forwarding reset.";
  } else {
    bout << "Saved.";
  }
  bout.nl(2);
}

static void optional_lines() {
  bout << "|#9You may specify your optional lines value from 0-10,\r\n" ;
  bout << "|#20 |#9being all, |#210 |#9being none.\r\n";
  bout << "|#2What value? ";
  string lines = input(2);

  int nNumLines = StringToInt(lines);
  if (!lines.empty() && nNumLines >= 0 && nNumLines < 11) {
    a()->user()->SetOptionalVal(nNumLines);
  }

}

void enter_regnum() {
  bout << "|#7Enter your WWIV registration number, or enter '|#20|#7' for none.\r\n|#0:";
  string regnum = input(5, true);

  long lRegNum = atol(regnum.c_str());
  if (!regnum.empty() && lRegNum >= 0) {
    a()->user()->SetWWIVRegNumber(lRegNum);
    changedsl();
  }
}

void defaults(wwiv::menus::MenuInstanceData* menudata) {
  bool done = false;
  do {
    print_cur_stat();
    a()->tleft(true);
    if (hangup) {
      return;
    }
    bout.nl();
    char ch;
    if (okansi()) {
      bout << "|#9Defaults: ";
      string allowable = "Q?123456789ABCIKLMSTUW";
      if (a()->experimental_read_prompt()) {
        allowable.push_back('G');
      }
      ch = onek(allowable, true);
    } else {
      bout << "|#9Defaults: ";
      ch = onek("Q?1234567BCIKLMTUW", true);
    }
    switch (ch) {
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
      break;
    case '3':
      a()->user()->ToggleStatusFlag(User::pauseOnPage);
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
      make_macros();
      break;
    case '8':
      change_colors();
      break;
    case '9':
      select_editor();
      break;
    case 'A':
      a()->user()->ToggleStatusFlag(User::extraColor);
      break;
    case 'B':
      optional_lines();
      break;
    case 'C':
      a()->user()->ToggleStatusFlag(User::conference);
      changedsl();
      break;
    case 'G':
      a()->user()->ToggleStatusFlag(User::fullScreenReader);
      break;
    case 'I': {
      bout.nl();
      bout << "|#9Enter your Internet mailing address.\r\n|#7:";
      string internetAddress = inputl(65, true);
      if (!internetAddress.empty()) {
        if (check_inet_addr(internetAddress)) {
          a()->user()->SetEmailAddress(internetAddress.c_str());
          write_inet_addr(internetAddress, a()->usernum);
        } else {
          bout << "\r\n|#6Invalid address format.\r\n\n";
          pausescr();
        }
      } else {
        bout << "|#5Delete Internet address? ";
        if (yesno()) {
          a()->user()->SetEmailAddress("");
        }
      }
    }
    break;
    case 'K':
      wwiv::menus::ConfigUserMenuSet();
      menudata->finished = true;
      menudata->reload = true;
      break;
    case 'L':
      if (a()->languages.size() > 1) {
        input_language();
      }
      break;
    case 'M':
      if (num_instances() > 1) {
        a()->user()->ClearStatusFlag(User::noMsgs);
        bout.nl();
        bout << "|#5Allow messages sent between instances? ";
        if (!yesno()) {
          a()->user()->SetStatusFlag(User::noMsgs);
        }
      }
      break;
    case 'S':
      a()->user()->ToggleStatusFlag(User::clearScreen);
      break;
    case 'T':
      a()->user()->ToggleStatusFlag(User::twentyFourHourClock);
      break;
    case 'U':
      a()->user()->ToggleStatusFlag(User::autoQuote);
      break;
    case 'W':
      enter_regnum();
      break;
    }
  } while (!done && !hangup);
  a()->WriteCurrentUser();
}

// private function used by list_config_scan_plus and drawscan
static int GetMaxLinesToShowForScanPlus() {
  return (a()->user()->GetScreenLines() - (4 + STOP_LIST) >
          MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) ?
          MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) :
          a()->user()->GetScreenLines() - (4 + STOP_LIST));
}

static void list_config_scan_plus(unsigned int first, int *amount, int type) {
  char s[101];

  bool bUseConf = (subconfnum > 1 && okconf(a()->user())) ? true : false;

  bout.cls();
  bout.clear_lines_listed();

  if (bUseConf) {
    strncpy(s, type == 0 ? stripcolors(reinterpret_cast<char*>
                                       (subconfs[uconfsub[a()->GetCurrentConferenceMessageArea()].confnum].name)) : stripcolors(
              reinterpret_cast<char*>(dirconfs[uconfdir[a()->GetCurrentConferenceFileArea()].confnum].name)), 26);
    s[26] = '\0';
    bout.bprintf("|#1Configure |#2%cSCAN |#9-- |#2%-26s |#9-- |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n",
                                      type == 0 ? 'Q' : 'N', s, type == 0 ? "sub" : "dir");
  } else {
    bout.bprintf("|#1Configure |#2%cSCAN                                   |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n",
                                      type == 0 ? 'Q' : 'N', type == 0 ? "sub" : "dir");
  }
  bout.Color(7);
  bout << string(79, '\xC4');
  bout.nl();

  int max_lines = GetMaxLinesToShowForScanPlus();

  if (type == 0) {
    for (size_t this_sub = first; (this_sub < a()->subs().subs().size()) && (a()->usub[this_sub].subnum != -1) &&
         *amount < max_lines * 2; this_sub++) {
      bout.clear_lines_listed();
      sprintf(s, "|#7[|#1%c|#7] |#9%s",
              (qsc_q[a()->usub[this_sub].subnum / 32] & (1L << (a()->usub[this_sub].subnum % 32))) ? '\xFE' : ' ',
              a()->subs().sub(a()->usub[this_sub].subnum).name.c_str());
      s[44] = '\0';
      if (*amount >= max_lines) {
        bout.GotoXY(40, 3 + *amount - max_lines);
        bout << s;
      } else {
        bout << s;
        bout.nl();
      }
      ++*amount;
    }
  } else {
    for (size_t this_dir = first; (this_dir < a()->directories.size()) && (a()->udir[this_dir].subnum != -1) &&
         *amount < max_lines * 2; this_dir++) {
      bout.clear_lines_listed();
      int alias_dir = a()->udir[this_dir].subnum;
      sprintf(s, "|#7[|#1%c|#7] |#2%s", qsc_n[alias_dir / 32] & (1L << (alias_dir % 32)) ? '\xFE' : ' ',
        a()->directories[alias_dir].name);
      s[44] = 0;
      if (*amount >= max_lines) {
        bout.GotoXY(40, 3 + *amount - max_lines);
        bout << s;
      } else {
        bout << s;
        bout.nl();
      }
      ++*amount;
    }
  }
  bout.nl();
  bout.clear_lines_listed();
}

static void drawscan(int filepos, long tagged) {
  int max_lines = GetMaxLinesToShowForScanPlus();
  if (filepos >= max_lines) {
    bout.GotoXY(40, 3 + filepos - max_lines);
  } else {
    bout.GotoXY(1, filepos + 3);
  }

  bout.SystemColor(static_cast<uint8_t>(wwiv::sdk::Color::BLACK) + (static_cast<uint8_t>(wwiv::sdk::Color::CYAN) << 4));
  bout.bprintf("[%c]", tagged ? '\xFE' : ' ');
  bout.SystemColor(static_cast<uint8_t>(wwiv::sdk::Color::YELLOW) + (static_cast<uint8_t>(wwiv::sdk::Color::BLACK) << 4));

  if (filepos >= max_lines) {
    bout.GotoXY(41, 3 + filepos - max_lines);
  } else {
    bout.GotoXY(2, filepos + 3);
  }
}

static void undrawscan(int filepos, long tagged) {
  int max_lines = GetMaxLinesToShowForScanPlus();

  if (filepos >= max_lines) {
    bout.GotoXY(40, 3 + filepos - max_lines);
  } else {
    bout.GotoXY(1, filepos + 3);
  }
  bout.bprintf("|#7[|#1%c|#7]", tagged ? '\xFE' : ' ');
}

static long is_inscan(int dir) {
  bool sysdir = false;
  if (IsEquals(a()->udir[0].keys, "0")) {
    sysdir = true;
  }

  for (size_t this_dir = 0; (this_dir < a()->directories.size()); this_dir++) {
    const string key = StringPrintf("%d", (sysdir ? dir : (dir + 1)));
    if (key == a()->udir[this_dir].keys) {
      int16_t ad = a()->udir[this_dir].subnum;
      return (qsc_n[ad / 32] & (1L << ad % 32));
    }
  }
  return 0;
}

void config_scan_plus(int type) {
  int i, command;
  unsigned int top = 0;
  int amount = 0, pos = 0, side_pos = 0;
  side_menu_colors smc{};

  int useconf = (subconfnum > 1 && okconf(a()->user()));
  a()->topdata = LocalIO::topdataNone;
  a()->UpdateTopScreen();

  vector<string> menu_items = { "Next",  "Previous", "Toggle", "Clear All", "Set All" };

  if (type == 0) {
    menu_items.push_back("Read New");
  } else {
    menu_items.push_back("List");
  }

  if (useconf) {
    menu_items.push_back("{ Conf");
    menu_items.push_back("} Conf");
    menu_items.push_back("Quit");
    menu_items.push_back("?");
  } else {
    menu_items.push_back("Quit");
    menu_items.push_back("?");
  }
  bool done = false;
  while (!done && !hangup) {
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
               qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
    }
    bool redraw = true;
    bool menu_done = false;
    while (!menu_done && !hangup && !done) {
      command = side_menu(&side_pos, redraw, menu_items, 1,
                          a()->user()->GetScreenLines() - STOP_LIST > MAX_SCREEN_LINES_TO_SHOW - STOP_LIST ?
                          MAX_SCREEN_LINES_TO_SHOW - STOP_LIST :
                          a()->user()->GetScreenLines() - STOP_LIST, &smc);
      bout.clear_lines_listed();
      redraw = true;
      bout.Color(0);
      if (do_sysop_command(command)) {
        menu_done = true;
        amount = 0;
      }
      switch (command) {
      case '?':
      case CO:
        bout.cls();
        printfile(SCONFIG_HLP);
        pausescr();
        menu_done = true;
        amount = 0;
        break;
      case COMMAND_DOWN:
        undrawscan(pos, type ? is_inscan(top + pos) :
                   qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
        ++pos;
        if (pos >= amount) {
          pos = 0;
        }
        drawscan(pos, type ? is_inscan(top + pos) :
                 qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case COMMAND_UP:
        undrawscan(pos, type ? is_inscan(top + pos) :
                   qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
        if (!pos) {
          pos = amount - 1;
        } else {
          --pos;
        }
        drawscan(pos, type ? is_inscan(top + pos) :
                 qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case SPACE: {
        if (type == 0) {
#ifdef NOTOGGLESYSOP
          if (a()->usub[top + pos].subnum == 0) {
            qsc_q[a()->usub[top + pos].subnum / 32] |= (1L << (a()->usub[top + pos].subnum % 32));
          }
          else
#endif
            qsc_q[a()->usub[top + pos].subnum / 32] ^= (1L << (a()->usub[top + pos].subnum % 32));
        }
        else {
          bool sysdir = IsEquals(a()->udir[0].keys, "0");
          for (size_t this_dir = 0; (this_dir < a()->directories.size()); this_dir++) {
            const string s = StringPrintf("%d", sysdir ? top + pos : top + pos + 1);
            if (s == a()->udir[this_dir].keys) {
              int ad = a()->udir[this_dir].subnum;
              qsc_n[ad / 32] ^= (1L << (ad % 32));
            }
          }
        }
        drawscan(pos, type ? is_inscan(top + pos) :
          qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
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
            if (top >= a()->subs().subs().size()) {
              top = 0;
            }
          } else {
            if (top >= a()->directories.size()) {
              top = 0;
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 1:
          if (top > a()->user()->GetScreenLines() - 4) {
            top -= a()->user()->GetScreenLines() - 4;
          } else {
            top = 0;
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 2:
          if (type == 0) {
            qsc_q[a()->usub[top + pos].subnum / 32] ^= (1L << (a()->usub[top + pos].subnum % 32));
          } else {
            bool sysdir = IsEquals(a()->udir[0].keys, "0");
            for (size_t this_dir = 0; (this_dir < a()->directories.size()); this_dir++) {
              const string s = StringPrintf("%d", sysdir ? top + pos : top + pos + 1);
              if (s == a()->udir[this_dir].keys) {
                int ad = a()->udir[this_dir].subnum;
                qsc_n[ad / 32] ^= (1L << (ad % 32));
              }
            }
          }
          drawscan(pos, type ? is_inscan(top + pos) :
                   qsc_q[a()->usub[top + pos].subnum / 32] & (1L << (a()->usub[top + pos].subnum % 32)));
          redraw = false;
          break;
        case 3:
          if (type == 0) {
            for (size_t this_sub = 0; this_sub < a()->subs().subs().size(); this_sub++) {
              if (qsc_q[a()->usub[this_sub].subnum / 32] & (1L << (a()->usub[this_sub].subnum % 32))) {
                qsc_q[a()->usub[this_sub].subnum / 32] ^= (1L << (a()->usub[this_sub].subnum % 32));
              }
            }
          } else {
            for (size_t this_dir = 0; this_dir < a()->directories.size(); this_dir++) {
              if (qsc_n[a()->udir[this_dir].subnum / 32] & (1L << (a()->udir[this_dir].subnum % 32))) {
                qsc_n[a()->udir[this_dir].subnum / 32] ^= 1L << (a()->udir[this_dir].subnum % 32);
              }
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 4:
          if (type == 0) {
            for (size_t this_sub = 0; this_sub < a()->subs().subs().size(); this_sub++) {
              if (!(qsc_q[a()->usub[this_sub].subnum / 32] & (1L << (a()->usub[this_sub].subnum % 32)))) {
                qsc_q[a()->usub[this_sub].subnum / 32] ^= (1L << (a()->usub[this_sub].subnum % 32));
              }
            }
          } else {
            for (size_t this_dir = 0; this_dir < a()->directories.size(); this_dir++) {
              if (!(qsc_n[a()->udir[this_dir].subnum / 32] & (1L << (a()->udir[this_dir].subnum % 32)))) {
                qsc_n[a()->udir[this_dir].subnum / 32] ^= 1L << (a()->udir[this_dir].subnum % 32);
              }
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 5:
          if (type == 0) {
            qscan(top + pos, &i);
          } else {
            i = a()->current_user_dir_num();
            a()->set_current_user_dir_num(top + pos);
            listfiles();
            a()->set_current_user_dir_num(i);
          }
          menu_done = true;
          amount = 0;
          break;
        case 6:
          if (okconf(a()->user())) {
            if (type == 0) {
              if (a()->GetCurrentConferenceMessageArea() > 0) {
                a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() - 1);
              } else {
                while ((uconfsub[a()->GetCurrentConferenceMessageArea() + 1].confnum >= 0)
                       && (a()->GetCurrentConferenceMessageArea() < subconfnum - 1)) {
                  a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() + 1);
                }
              }
              setuconf(ConferenceType::CONF_SUBS, a()->GetCurrentConferenceMessageArea(), -1);
            } else {
              if (a()->GetCurrentConferenceFileArea() > 0) {
                a()->SetCurrentConferenceFileArea(a()->GetCurrentConferenceFileArea() - 1);
              } else {
                while ((uconfdir[a()->GetCurrentConferenceFileArea() + 1].confnum >= 0)
                       && (a()->GetCurrentConferenceFileArea() < dirconfnum - 1)) {
                  a()->SetCurrentConferenceFileArea(a()->GetCurrentConferenceFileArea() + 1);
                }
              }
              setuconf(ConferenceType::CONF_DIRS, a()->GetCurrentConferenceFileArea(), -1);
            }
            pos = 0;
            menu_done = true;
            amount = 0;
          }
          break;
        case 7:
          if (okconf(a()->user())) {
            if (type == 0) {
              if ((a()->GetCurrentConferenceMessageArea() < subconfnum - 1)
                  && (uconfsub[a()->GetCurrentConferenceMessageArea() + 1].confnum >= 0)) {
                a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() + 1);
              } else {
                a()->SetCurrentConferenceMessageArea(0);
              }
              setuconf(ConferenceType::CONF_SUBS, a()->GetCurrentConferenceMessageArea(), -1);
            }

            else {
              if ((a()->GetCurrentConferenceFileArea() < dirconfnum - 1)
                  && (uconfdir[a()->GetCurrentConferenceFileArea() + 1].confnum >= 0)) {
                a()->SetCurrentConferenceFileArea(a()->GetCurrentConferenceFileArea() + 1);
              } else {
                a()->SetCurrentConferenceFileArea(0);
              }
              setuconf(ConferenceType::CONF_DIRS, a()->GetCurrentConferenceFileArea(), -1);
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
          printfile(SCONFIG_HLP);
          pausescr();
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
