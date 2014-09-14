/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "wwivcolors.h"

#include "wwiv.h"
#include "common.h"
#include "menu.h"
#include "printfile.h"
#include "core/strings.h"

#define STOP_LIST 0
#define MAX_SCREEN_LINES_TO_SHOW 24

#define NORMAL_HIGHLIGHT   (YELLOW+(BLACK<<4))
#define NORMAL_MENU_ITEM   (CYAN+(BLACK<<4))
#define CURRENT_HIGHLIGHT  (RED+(LIGHTGRAY<<4))
#define CURRENT_MENU_ITEM  (BLACK+(LIGHTGRAY<<4))
// Undefine this so users can not toggle the sysop sub on and off
// #define NOTOGGLESYSOP


//
// Local functions
//
void reset_user_colors_to_defaults();
const std::string DisplayColorName(int c);


void select_editor() {
  if (GetSession()->GetNumberOfEditors() == 0) {
    GetSession()->bout << "\r\nNo full screen editors available.\r\n\n";
    return;
  } else if (GetSession()->GetNumberOfEditors() == 1) {
    if (GetSession()->GetCurrentUser()->GetDefaultEditor() == 0) {
      GetSession()->GetCurrentUser()->SetDefaultEditor(1);
    } else {
      GetSession()->GetCurrentUser()->SetDefaultEditor(0);
      GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::autoQuote);
    }
    return;
  }
  for (int i1 = 0; i1 < 5; i1++) {
    odc[ i1 ] = '\0';
  }
  GetSession()->bout << "0. Normal non-full screen editor\r\n";
  for (int i = 0; i < GetSession()->GetNumberOfEditors(); i++) {
    GetSession()->bout << i + 1 << ". " << editors[i].description  << wwiv::endl;
    if (((i + 1) % 10) == 0) {
      odc[(i + 1) / 10 - 1 ] = static_cast<char>((i + 1) / 10);
    }
  }
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#9Which editor (|#31-" << GetSession()->GetNumberOfEditors() << ", <Q>=leave as is|#9) ? ";
  char *ss = mmkey(2);
  int nEditor = atoi(ss);
  if (nEditor >= 1 && nEditor <= GetSession()->GetNumberOfEditors()) {
    GetSession()->GetCurrentUser()->SetDefaultEditor(nEditor);
  } else if (wwiv::strings::IsEquals(ss, "0")) {
    GetSession()->GetCurrentUser()->SetDefaultEditor(0);
    GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::autoQuote);
  }
}


const char* GetMailBoxStatus(char* pszStatusOut) {
  if (GetSession()->GetCurrentUser()->GetForwardSystemNumber() == 0 &&
      GetSession()->GetCurrentUser()->GetForwardUserNumber() == 0) {
    strcpy(pszStatusOut, "Normal");
    return pszStatusOut;
  }
  if (GetSession()->GetCurrentUser()->GetForwardSystemNumber() != 0) {
    if (GetSession()->GetCurrentUser()->IsMailboxForwarded()) {
      sprintf(pszStatusOut, "Forward to #%u @%u.%s.",
              GetSession()->GetCurrentUser()->GetForwardUserNumber(),
              GetSession()->GetCurrentUser()->GetForwardSystemNumber(),
              net_networks[ GetSession()->GetCurrentUser()->GetForwardNetNumber() ].name);
    } else {
      char szForwardUserName[80];
      read_inet_addr(szForwardUserName, GetSession()->usernum);
      sprintf(pszStatusOut, "Forwarded to Internet %s", szForwardUserName);
    }
    return pszStatusOut;
  }

  if (GetSession()->GetCurrentUser()->IsMailboxClosed()) {
    strcpy(pszStatusOut, "Closed");
    return pszStatusOut;
  }

  WUser ur;
  GetApplication()->GetUserManager()->ReadUser(&ur, GetSession()->GetCurrentUser()->GetForwardUserNumber());
  if (ur.IsUserDeleted()) {
    GetSession()->GetCurrentUser()->SetForwardUserNumber(0);
    strcpy(pszStatusOut, "Normal");
    return pszStatusOut;
  }
  sprintf(pszStatusOut, "Forward to %s", ur.GetUserNameAndNumber(GetSession()->GetCurrentUser()->GetForwardUserNumber()));
  return pszStatusOut;
}


void print_cur_stat() {
  char s1[255], s2[255];
  GetSession()->bout.ClearScreen();
  GetSession()->bout << "|#5Your Preferences\r\n\n";
  sprintf(s1, "|#11|#9) Screen size       : |#2%d X %d", GetSession()->GetCurrentUser()->GetScreenChars(),
          GetSession()->GetCurrentUser()->GetScreenLines());
  sprintf(s2, "|#12|#9) ANSI              : |#2%s", GetSession()->GetCurrentUser()->HasAnsi() ?
          (GetSession()->GetCurrentUser()->HasColor() ? "Color" : "Monochrome") : "No ANSI");
  GetSession()->bout.WriteFormatted("%-48s %-45s\r\n", s1, s2);

  sprintf(s1, "|#13|#9) Pause on screen   : |#2%s", GetSession()->GetCurrentUser()->HasPause() ? "On" : "Off");
  char szMailBoxStatus[81];
  sprintf(s2, "|#14|#9) Mailbox           : |#2%s", GetMailBoxStatus(szMailBoxStatus));
  GetSession()->bout.WriteFormatted("%-48s %-45s\r\n", s1, s2);

  sprintf(s1, "|#15|#9) Configured Q-scan");
  sprintf(s2, "|#16|#9) Change password");
  GetSession()->bout.WriteFormatted("%-45s %-45s\r\n", s1, s2);

  if (okansi()) {
    sprintf(s1, "|#17|#9) Update macros");
    sprintf(s2, "|#18|#9) Change colors");
    GetSession()->bout.WriteFormatted("%-45s %-45s\r\n", s1, s2);
    int nEditorNum = GetSession()->GetCurrentUser()->GetDefaultEditor();
    sprintf(s1, "|#19|#9) Full screen editor: |#2%s",
            ((nEditorNum > 0) && (nEditorNum <= GetSession()->GetNumberOfEditors())) ?
            editors[ nEditorNum - 1 ].description : "None");
    sprintf(s2, "|#1A|#9) Extended colors   : |#2%s", YesNoString(GetSession()->GetCurrentUser()->IsUseExtraColor()));
    GetSession()->bout.WriteFormatted("%-48.48s %-45s\r\n", s1, s2);
  } else {
    GetSession()->bout << "|#17|#9) Update macros\r\n";
  }
  sprintf(s1, "|#1B|#9) Optional lines    : |#2%d", GetSession()->GetCurrentUser()->GetOptionalVal());
  sprintf(s2, "|#1C|#9) Conferencing      : |#2%s", YesNoString(GetSession()->GetCurrentUser()->IsUseConference()));
  GetSession()->bout.WriteFormatted("%-48s %-45s\r\n", s1, s2);
  GetSession()->bout << "|#1I|#9) Internet Address  : |#2" << ((GetSession()->GetCurrentUser()->GetEmailAddress()[0] ==
                     '\0') ? "None." : GetSession()->GetCurrentUser()->GetEmailAddress()) << wwiv::endl;
  GetSession()->bout << "|#1K|#9) Configure Menus\r\n";
  if (GetSession()->num_languages > 1) {
    sprintf(s1, "|#1L|#9) Language          : |#2%s", cur_lang_name);
    GetSession()->bout.WriteFormatted("%-48s ", s1);
  }
  if (num_instances() > 1) {
    sprintf(s1, "|#1M|#9) Allow user msgs   : |#2%s",
            YesNoString(GetSession()->GetCurrentUser()->IsIgnoreNodeMessages() ? false : true));
    GetSession()->bout.WriteFormatted("%-48s", s1);
  }
  GetSession()->bout.NewLine();
  sprintf(s1, "|#1S|#9) Cls Between Msgs? : |#2%s", YesNoString(GetSession()->GetCurrentUser()->IsUseClearScreen()));
  sprintf(s2, "|#1T|#9) 12hr or 24hr clock: |#2%s", GetSession()->GetCurrentUser()->IsUse24HourClock() ? "24hr" : "12hr");
  GetSession()->bout.WriteFormatted("%-48s %-45s\r\n", s1, s2);
  sprintf(s1, "|#1U|#9) Use Msg AutoQuote : |#2%s", YesNoString(GetSession()->GetCurrentUser()->IsUseAutoQuote()));

  char szWWIVRegNum[80];
  if (GetSession()->GetCurrentUser()->GetWWIVRegNumber()) {
    sprintf(szWWIVRegNum, "%ld", GetSession()->GetCurrentUser()->GetWWIVRegNumber());
  } else {
    strcpy(szWWIVRegNum, "(None)");
  }
  sprintf(s2, "|#1W|#9) WWIV reg num      : |#2%s", szWWIVRegNum);
  GetSession()->bout.WriteFormatted("%-48s %-45s\r\n", s1, s2);

  GetSession()->bout << "|#1Q|#9) Quit to main menu\r\n";
}


const std::string DisplayColorName(int c) {
  if (checkcomp("Ami") || checkcomp("Mac")) {
    std::ostringstream os;
    os << "Color #" << c;
    return std::string(os.str());
  }

  switch (c) {
  case 0:
    return std::string("Black");
  case 1:
    return std::string("Blue");
  case 2:
    return std::string("Green");
  case 3:
    return std::string("Cyan");
  case 4:
    return std::string("Red");
  case 5:
    return std::string("Magenta");
  case 6:
    return std::string("Yellow");
  case 7:
    return std::string("White");
  default:
    return std::string("");
  }
}


const std::string DescribeColorCode(int nColorCode) {
  std::ostringstream os;

  if (GetSession()->GetCurrentUser()->HasColor()) {
    os << DisplayColorName(nColorCode & 0x07) << " on " << DisplayColorName((nColorCode >> 4) & 0x07);
  } else {
    os << ((nColorCode & 0x07) ? "Normal" : "Inversed");
  }

  if (nColorCode & 0x08) {
    os << ((checkcomp("Ami") || checkcomp("Mac")) ? ", Bold" : ", Intense");
  }
  if (nColorCode & 0x80) {
    if (checkcomp("Ami")) {
      os << ", Italicized";
    } else if (checkcomp("Mac")) {
      os << ", Underlined";
    } else {
      os << ", Blinking";
    }
  }
  return std::string(os.str());
}


void color_list() {
  GetSession()->bout.NewLine(2);
  for (int i = 0; i < 8; i++) {
    GetSession()->bout.SystemColor(static_cast< unsigned char >((i == 0) ? 0x70 : i));
    GetSession()->bout << i << ". " << DisplayColorName(static_cast< char >(i)).c_str() << "|#0" << wwiv::endl;
  }
}


void change_colors() {
  bool done = false;
  GetSession()->bout.NewLine();
  do {
    GetSession()->bout.ClearScreen();
    GetSession()->bout << "|#5Customize Colors:";
    GetSession()->bout.NewLine(2);
    if (!GetSession()->GetCurrentUser()->HasColor()) {
      std::ostringstream os;
      os << "Monochrome base color : ";
      if ((GetSession()->GetCurrentUser()->GetBWColor(1) & 0x70) == 0) {
        os << DisplayColorName(GetSession()->GetCurrentUser()->GetBWColor(1) & 0x07);
      } else {
        os << DisplayColorName((GetSession()->GetCurrentUser()->GetBWColor(1) >> 4) & 0x07);
      }
      GetSession()->bout << os.str();
      GetSession()->bout.NewLine(2);
    }
    for (int i = 0; i < 10; i++) {
      std::ostringstream os;
      GetSession()->bout.Color(i);
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
      if (GetSession()->GetCurrentUser()->HasColor()) {
        os << DescribeColorCode(GetSession()->GetCurrentUser()->GetColor(i));
      } else {
        os << DescribeColorCode(GetSession()->GetCurrentUser()->GetBWColor(i));
      }
      GetSession()->bout << os.str();
      GetSession()->bout.NewLine();
    }
    GetSession()->bout << "\r\n|#9[|#2R|#9]eset Colors to Default Values, [|#2Q|#9]uit\r\n";
    GetSession()->bout << "|#9Enter Color Number to Modify (|#20|#9-|#29|#9,|#2R|#9,|#2Q|#9): ";
    char ch = onek("RQ0123456789", true);
    if (ch == 'Q') {
      done = true;
    } else if (ch == 'R') {
      reset_user_colors_to_defaults();
    } else {
      char nc = 0;
      int nColorNum = ch - '0';
      if (GetSession()->GetCurrentUser()->HasColor()) {
        color_list();
        GetSession()->bout.Color(0);
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#9(Q=Quit) Foreground? ";
        ch = onek("Q01234567");
        if (ch == 'Q') {
          continue;
        }
        nc = static_cast< char >(ch - '0');
        GetSession()->bout << "|#9(Q=Quit) Background? ";
        ch = onek("Q01234567");
        if (ch == 'Q') {
          continue;
        }
        nc = static_cast< char >(nc | ((ch - '0') << 4));
      } else {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#9Inversed? ";
        if (yesno()) {
          if ((GetSession()->GetCurrentUser()->GetBWColor(1) & 0x70) == 0) {
            nc = static_cast< char >(0 | ((GetSession()->GetCurrentUser()->GetBWColor(1) & 0x07) << 4));
          } else {
            nc = static_cast< char >(GetSession()->GetCurrentUser()->GetBWColor(1) & 0x70);
          }
        } else {
          if ((GetSession()->GetCurrentUser()->GetBWColor(1) & 0x70) == 0) {
            nc = static_cast< char >(0 | (GetSession()->GetCurrentUser()->GetBWColor(1) & 0x07));
          } else {
            nc = static_cast< char >((GetSession()->GetCurrentUser()->GetBWColor(1) & 0x70) >> 4);
          }
        }
      }
      if (checkcomp("Ami") || checkcomp("Mac")) {
        GetSession()->bout << "|#9Bold? ";
      } else {
        GetSession()->bout << "|#9Intensified? ";
      }
      if (yesno()) {
        nc |= 0x08;
      }

      if (checkcomp("Ami")) {
        GetSession()->bout << "|#9Italicized? ";
      } else if (checkcomp("Mac")) {
        GetSession()->bout << "|#9Underlined? ";
      } else {
        GetSession()->bout << "|#9Blinking? ";
      }

      if (yesno()) {
        nc |= 0x80;
      }

      GetSession()->bout.NewLine(2);
      GetSession()->bout.SystemColor(nc);
      GetSession()->bout << DescribeColorCode(nc);
      GetSession()->bout.Color(0);
      GetSession()->bout.NewLine(2);
      GetSession()->bout << "|#8Is this OK? ";
      if (yesno()) {
        GetSession()->bout << "\r\nColor saved.\r\n\n";
        if (GetSession()->GetCurrentUser()->HasColor()) {
          GetSession()->GetCurrentUser()->SetColor(nColorNum, nc);
        } else {
          GetSession()->GetCurrentUser()->SetBWColor(nColorNum, nc);
        }
      } else {
        GetSession()->bout << "\r\nNot saved, then.\r\n\n";
      }
    }
  } while (!done && !hangup);
}



void l_config_qscan() {
  bool abort = false;
  GetSession()->bout << "\r\n|#9Boards to q-scan marked with '*'|#0\r\n\n";
  for (int i = 0; (i < GetSession()->num_subs) && (usub[i].subnum != -1) && !abort; i++) {
    char szBuffer[81];
    sprintf(szBuffer, "%c %s. %s",
            (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32))) ? '*' : ' ',
            usub[i].keys,
            subboards[usub[i].subnum].name);
    pla(szBuffer, &abort);
  }
  GetSession()->bout.NewLine(2);
}


void config_qscan() {
  if (okansi()) {
    config_scan_plus(QSCAN);
    return;
  }

  int oc = GetSession()->GetCurrentConferenceMessageArea();
  int os = usub[GetSession()->GetCurrentMessageArea()].subnum;

  bool done = false;
  bool done1 = false;
  do {
    char ch;
    if (okconf(GetSession()->GetCurrentUser()) && uconfsub[1].confnum != -1) {
      char szConfList[MAX_CONFERENCES + 2];
      bool abort = false;
      strcpy(szConfList, " ");
      GetSession()->bout << "\r\nSelect Conference: \r\n\n";
      int i = 0;
      while (i < subconfnum && uconfsub[i].confnum != -1 && !abort) {
        char szBuffer[120];
        sprintf(szBuffer, "%c) %s", subconfs[uconfsub[i].confnum].designator,
                stripcolors(reinterpret_cast<char*>(subconfs[uconfsub[i].confnum].name)));
        pla(szBuffer, &abort);
        szConfList[i + 1] = subconfs[uconfsub[i].confnum].designator;
        szConfList[i + 2] = 0;
        i++;
      }
      GetSession()->bout.NewLine();
      GetSession()->bout << "Select [" << &szConfList[1] << ", <space> to quit]: ";
      ch = onek(szConfList);
    } else {
      ch = '-';
    }
    switch (ch) {
    case ' ':
      done1 = true;
      break;
    default:
      if (okconf(GetSession()->GetCurrentUser())  && uconfsub[1].confnum != -1) {
        int i = 0;
        while ((ch != subconfs[uconfsub[i].confnum].designator) && (i < subconfnum)) {
          i++;
        }

        if (i >= subconfnum) {
          break;
        }

        setuconf(CONF_SUBS, i, -1);
      }
      l_config_qscan();
      done = false;
      do {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#2Enter message base number (|#1C=Clr All, Q=Quit, S=Set All|#2): ";
        char* s = mmkey(0);
        if (s[0]) {
          for (int i = 0; (i < GetSession()->num_subs) && (usub[i].subnum != -1); i++) {
            if (wwiv::strings::IsEquals(usub[i].keys, s)) {
              qsc_q[usub[i].subnum / 32] ^= (1L << (usub[i].subnum % 32));
            }
            if (wwiv::strings::IsEquals(s, "S")) {
              qsc_q[usub[i].subnum / 32] |= (1L << (usub[i].subnum % 32));
            }
            if (wwiv::strings::IsEquals(s, "C")) {
              qsc_q[usub[i].subnum / 32] &= ~(1L << (usub[i].subnum % 32));
            }
          }
          if (wwiv::strings::IsEquals(s, "Q")) {
            done = true;
          }
          if (wwiv::strings::IsEquals(s, "?")) {
            l_config_qscan();
          }
        }
      } while (!done && !hangup);
      break;
    }
    if (!okconf(GetSession()->GetCurrentUser()) || uconfsub[1].confnum == -1) {
      done1 = true;
    }

  } while (!done1 && !hangup);

  if (okconf(GetSession()->GetCurrentUser())) {
    setuconf(CONF_SUBS, oc, os);
  }
}

void make_macros() {
  char szMacro[255], ch;
  bool done = false;

  do {
    bputch(CL);
    GetSession()->bout << "|#4Macro A: \r\n";
    list_macro(GetSession()->GetCurrentUser()->GetMacro(2));
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#4Macro D: \r\n";
    list_macro(GetSession()->GetCurrentUser()->GetMacro(0));
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#4Macro F: \r\n";
    list_macro(GetSession()->GetCurrentUser()->GetMacro(1));
    GetSession()->bout.NewLine(2);
    GetSession()->bout << "|#9Macro to edit or Q:uit (A,D,F,Q) : |#0";
    ch = onek("QADF");
    szMacro[0] = 0;
    switch (ch) {
    case 'A':
      macroedit(szMacro);
      if (szMacro[0]) {
        GetSession()->GetCurrentUser()->SetMacro(2, szMacro);
      }
      break;
    case 'D':
      macroedit(szMacro);
      if (szMacro[0]) {
        GetSession()->GetCurrentUser()->SetMacro(0, szMacro);
      }
      break;
    case 'F':
      macroedit(szMacro);
      if (szMacro[0]) {
        GetSession()->GetCurrentUser()->SetMacro(1, szMacro);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !hangup);
}


void list_macro(const char *pszMacroText) {
  int i = 0;

  while ((i < 80) && (pszMacroText[i] != 0)) {
    if (pszMacroText[i] >= 32) {
      bputch(pszMacroText[i]);
    } else {
      if (pszMacroText[i] == 16) {
        GetSession()->bout.Color(pszMacroText[++i] - 48);
      } else {
        switch (pszMacroText[i]) {
        case RETURN:
          bputch('|');
          break;
        case TAB:
          bputch('\xF9');
          break;
        default:
          bputch('^');
          bputch(static_cast< unsigned char >(pszMacroText[i] + 64));
          break;
        }
      }
    }
    ++i;
  }
  GetSession()->bout.NewLine();
}

char *macroedit(char *pszMacroText) {
  *pszMacroText = '\0';
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#5Enter your macro, press |#7[|#1CTRL-Z|#7]|#5 when finished.\r\n\n";
  okskey = false;
  GetSession()->bout.Color(0);
  bool done = false;
  int i = 0;
  bool toggle = false;
  int textclr = 0;
  do {
    char ch = getkey();
    switch (ch) {
    case CZ:
      done = true;
      break;
    case BACKSPACE:
      GetSession()->bout.BackSpace();
      i--;
      if (i < 0) {
        i = 0;
      }
      pszMacroText[i] = '\0';
      break;
    case CP:
      pszMacroText[i++] = ch;
      toggle = true;
      break;
    case RETURN:
      pszMacroText[i++] = ch;
      GetSession()->bout.Color(0);
      bputch('|');
      GetSession()->bout.Color(textclr);
      break;
    case TAB:
      pszMacroText[i++] = ch;
      GetSession()->bout.Color(0);
      bputch('\xF9') ;
      GetSession()->bout.Color(textclr);
      break;
    default:
      pszMacroText[i++] = ch;
      if (toggle) {
        toggle = false;
        textclr = ch - 48;
        GetSession()->bout.Color(textclr);
      } else {
        bputch(ch);
      }
      break;
    }
    pszMacroText[i + 1] = 0;
  } while (!done && i < 80 && !hangup);
  okskey = true;
  GetSession()->bout.Color(0);
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#9Is this okay? ";
  if (!yesno()) {
    *pszMacroText = '\0';
  }
  return pszMacroText;
}


void change_password() {
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#9Change password? ";
  if (!yesno()) {
    return;
  }

  std::string password, password2;
  GetSession()->bout.NewLine();
  input_password("|#9You must now enter your current password.\r\n|#7: ", &password, 8);
  if (password != GetSession()->GetCurrentUser()->GetPassword()) {
    GetSession()->bout << "\r\nIncorrect.\r\n\n";
    return;
  }
  GetSession()->bout.NewLine(2);
  input_password("|#9Enter your new password, 3 to 8 characters long.\r\n|#7: ", &password, 8);
  GetSession()->bout.NewLine(2);
  input_password("|#9Repeat password for verification.\r\n|#7: ", &password2, 8);
  if (password == password2) {
    if (password2.length() < 3) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6Password must be 3-8 characters long.\r\n|#6Password was not changed.\r\n\n";
    } else {
      GetSession()->GetCurrentUser()->SetPassword(password.c_str());
      GetSession()->bout << "\r\n|#1Password changed.\r\n\n";
      sysoplog("Changed Password.");
    }
  } else {
    GetSession()->bout << "\r\n|#6VERIFY FAILED.\r\n|#6Password not changed.\r\n\n";
  }
}


void modify_mailbox() {
  char s[81];

  GetSession()->bout.NewLine();

  GetSession()->bout << "|#9Do you want to close your mailbox? ";
  if (yesno()) {
    GetSession()->bout << "|#5Are you sure? ";
    if (yesno()) {
      GetSession()->GetCurrentUser()->CloseMailbox();
      return;
    }
  }
  GetSession()->bout << "|#5Do you want to forward your mail? ";
  if (!yesno()) {
    GetSession()->GetCurrentUser()->ClearMailboxForward();
    return;
  }
  if (GetSession()->GetCurrentUser()->GetSl() >= syscfg.newusersl) {
    int nNetworkNumber = getnetnum("FILEnet");
    GetSession()->SetNetworkNumber(nNetworkNumber);
    if (nNetworkNumber != -1) {
      set_net_num(GetSession()->GetNetworkNumber());
      GetSession()->bout << "|#5Do you want to forward to your Internet address? ";
      if (yesno()) {
        GetSession()->bout << "|#3Enter the Internet E-Mail Address.\r\n|#9:";
        Input1(s, GetSession()->GetCurrentUser()->GetEmailAddress(), 75, true, 
            wwiv::bbs::InputMode::MIXED);
        if (check_inet_addr(s)) {
          GetSession()->GetCurrentUser()->SetEmailAddress(s);
          write_inet_addr(s, GetSession()->usernum);
          GetSession()->GetCurrentUser()->SetForwardNetNumber(GetSession()->GetNetworkNumber());
          GetSession()->GetCurrentUser()->SetForwardToInternet();
          GetSession()->bout << "\r\nSaved.\r\n\n";
        }
        return;
      }
    }
  }
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#2Forward to? ";
  input(s, 40);

  int nTempForwardUser, nTempForwardSystem;
  parse_email_info(s, &nTempForwardUser, &nTempForwardSystem);
  GetSession()->GetCurrentUser()->SetForwardUserNumber(nTempForwardUser);
  GetSession()->GetCurrentUser()->SetForwardSystemNumber(nTempForwardSystem);
  if (GetSession()->GetCurrentUser()->GetForwardSystemNumber() != 0) {
    GetSession()->GetCurrentUser()->SetForwardNetNumber(GetSession()->GetNetworkNumber());
    if (GetSession()->GetCurrentUser()->GetForwardUserNumber() == 0) {
      GetSession()->GetCurrentUser()->ClearMailboxForward();
      GetSession()->bout << "\r\nCan't forward to a user name, must use user number.\r\n\n";
    }
  } else if (GetSession()->GetCurrentUser()->GetForwardUserNumber() == GetSession()->usernum) {
    GetSession()->bout << "\r\nCan't forward to yourself.\r\n\n";
    GetSession()->GetCurrentUser()->SetForwardUserNumber(0);
  }

  GetSession()->bout.NewLine();
  if (GetSession()->GetCurrentUser()->GetForwardUserNumber() == 0
      && GetSession()->GetCurrentUser()->GetForwardSystemNumber() == 0) {
    GetSession()->GetCurrentUser()->SetForwardNetNumber(0);
    GetSession()->bout << "Forwarding reset.\r\n";
  } else {
    GetSession()->bout << "Saved.\r\n";
  }
  GetSession()->bout.NewLine();
}


void optional_lines() {
  GetSession()->bout << "|#9You may specify your optional lines value from 0-10,\r\n" ;
  GetSession()->bout << "|#20 |#9being all, |#210 |#9being none.\r\n";
  GetSession()->bout << "|#2What value? ";
  std::string lines;
  input(&lines, 2);

  int nNumLines = atoi(lines.c_str());
  if (!lines.empty() && nNumLines >= 0 && nNumLines < 11) {
    GetSession()->GetCurrentUser()->SetOptionalVal(nNumLines);
  }

}


void enter_regnum() {
  GetSession()->bout << "|#7Enter your WWIV registration number, or enter '|#20|#7' for none.\r\n|#0:";
  std::string regnum;
  input(&regnum, 5, true);

  long lRegNum = atol(regnum.c_str());
  if (!regnum.empty() && lRegNum >= 0) {
    GetSession()->GetCurrentUser()->SetWWIVRegNumber(lRegNum);
    changedsl();
  }
}


void defaults(MenuInstanceData * pMenuData) {
  bool done = false;
  do {
    print_cur_stat();
    GetSession()->localIO()->tleft(true);
    if (hangup) {
      return;
    }
    GetSession()->bout.NewLine();
    char ch;
    if (okansi()) {
      GetSession()->bout << "|#9Defaults: (1-9,A-C,I,K,L,M,S,T,U,W,?,Q) : ";
      ch = onek("Q?123456789ABCIKLMSTUW", true);
    } else {
      GetSession()->bout << "|#9Defaults: (1-7,B,C,I,K,L,M,S,T,U,W,?,Q) : ";
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
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::pauseOnPage);
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
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::extraColor);
      break;
    case 'B':
      optional_lines();
      break;
    case 'C':
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::conference);
      changedsl();
      break;

    case 'I': {
      std::string internetAddress;
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#9Enter your Internet mailing address.\r\n|#7:";
      inputl(&internetAddress, 65, true);
      if (!internetAddress.empty()) {
        if (check_inet_addr(internetAddress.c_str())) {
          GetSession()->GetCurrentUser()->SetEmailAddress(internetAddress.c_str());
          write_inet_addr(internetAddress.c_str(), GetSession()->usernum);
        } else {
          GetSession()->bout << "\r\n|#6Invalid address format.\r\n\n";
          pausescr();
        }
      } else {
        GetSession()->bout << "|#5Delete Internet address? ";
        if (yesno()) {
          GetSession()->GetCurrentUser()->SetEmailAddress("");
        }
      }
    }
    break;
    case 'K':
      ConfigUserMenuSet();
      pMenuData->nFinished = 1;
      pMenuData->nReload = 1;
      break;
    case 'L':
      if (GetSession()->num_languages > 1) {
        input_language();
      }
      break;
    case 'M':
      if (num_instances() > 1) {
        GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::noMsgs);
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#5Allow messages sent between instances? ";
        if (!yesno()) {
          GetSession()->GetCurrentUser()->SetStatusFlag(WUser::noMsgs);
        }
      }
      break;
    case 'S':
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::clearScreen);
      break;
    case 'T':
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::twentyFourHourClock);
      break;
    case 'U':
      GetSession()->GetCurrentUser()->ToggleStatusFlag(WUser::autoQuote);
      break;
    case 'W':
      enter_regnum();
      break;
    }
  } while (!done && !hangup);
  GetSession()->WriteCurrentUser();
}



// private function used by list_config_scan_plus and drawscan
int GetMaxLinesToShowForScanPlus() {
#ifdef MAX_SCREEN_LINES_TO_SHOW
  return (GetSession()->GetCurrentUser()->GetScreenLines() - (4 + STOP_LIST) >
          MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) ?
          MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) :
          GetSession()->GetCurrentUser()->GetScreenLines() - (4 + STOP_LIST));
#else
  return GetSession()->GetCurrentUser()->GetScreenLines() - (4 + STOP_LIST);
#endif
}


void list_config_scan_plus(int first, int *amount, int type) {
  char s[101];

  bool bUseConf = (subconfnum > 1 && okconf(GetSession()->GetCurrentUser())) ? true : false;

  GetSession()->bout.ClearScreen();
  lines_listed = 0;

  if (bUseConf) {
    strncpy(s, type == 0 ? stripcolors(reinterpret_cast<char*>
                                       (subconfs[uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum].name)) : stripcolors(
              reinterpret_cast<char*>(dirconfs[uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum].name)), 26);
    s[26] = '\0';
    GetSession()->bout.WriteFormatted("|#1Configure |#2%cSCAN |#9-- |#2%-26s |#9-- |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n",
                                      type == 0 ? 'Q' : 'N', s, type == 0 ? "sub" : "dir");
  } else {
    GetSession()->bout.WriteFormatted("|#1Configure |#2%cSCAN                                   |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n",
                                      type == 0 ? 'Q' : 'N', type == 0 ? "sub" : "dir");
  }
  repeat_char('\xC4', 79);

  int max_lines = GetMaxLinesToShowForScanPlus();

  if (type == 0) {
    for (int this_sub = first; (this_sub < GetSession()->num_subs) && (usub[this_sub].subnum != -1) &&
         *amount < max_lines * 2; this_sub++) {
      lines_listed = 0;
      sprintf(s, "|#7[|#1%c|#7] |#9%s",
              (qsc_q[usub[this_sub].subnum / 32] & (1L << (usub[this_sub].subnum % 32))) ? '\xFE' : ' ',
              subboards[usub[this_sub].subnum].name);
      s[44] = '\0';
      if (*amount >= max_lines) {
        GetSession()->bout.GotoXY(40, 3 + *amount - max_lines);
        GetSession()->bout << s;
      } else {
        GetSession()->bout << s;
        GetSession()->bout.NewLine();
      }
      ++*amount;
    }
  } else {
    for (int this_dir = first; (this_dir < GetSession()->num_dirs) && (udir[this_dir].subnum != -1) &&
         *amount < max_lines * 2; this_dir++) {
      lines_listed = 0;
      int alias_dir = udir[this_dir].subnum;
      sprintf(s, "|#7[|#1%c|#7] |#2%s", qsc_n[alias_dir / 32] & (1L << (alias_dir % 32)) ? '\xFE' : ' ',
              directories[alias_dir].name);
      s[44] = 0;
      if (*amount >= max_lines) {
        GetSession()->bout.GotoXY(40, 3 + *amount - max_lines);
        GetSession()->bout << s;
      } else {
        GetSession()->bout << s;
        GetSession()->bout.NewLine();
      }
      ++*amount;
    }
  }
  GetSession()->bout.NewLine();
  lines_listed = 0;
}


void config_scan_plus(int type) {
  char **menu_items, s[50];
  int i, command, this_dir, this_sub, ad;
  int sysdir = 0, top = 0, amount = 0, pos = 0, side_pos = 0;
  struct side_menu_colors smc = {
    NORMAL_HIGHLIGHT,
    NORMAL_MENU_ITEM,
    CURRENT_HIGHLIGHT,
    CURRENT_MENU_ITEM
  };

  int useconf = (subconfnum > 1 && okconf(GetSession()->GetCurrentUser()));
  GetSession()->topdata = WLocalIO::topdataNone;
  GetApplication()->UpdateTopScreen();

  menu_items = static_cast<char **>(BbsAlloc2D(10, 10, sizeof(char)));
  strcpy(menu_items[0], "Next");
  strcpy(menu_items[1], "Previous");
  strcpy(menu_items[2], "Toggle");
  strcpy(menu_items[3], "Clear All");
  strcpy(menu_items[4], "Set All");

  if (type == 0) {
    strcpy(menu_items[5], "Read New");
  } else {
    strcpy(menu_items[5], "List");
  }

  if (useconf) {
    strcpy(menu_items[6], "{ Conf");
    strcpy(menu_items[7], "} Conf");
    strcpy(menu_items[8], "Quit");
    strcpy(menu_items[9], "?");
    menu_items[9][0] = 0;
  } else {
    strcpy(menu_items[6], "Quit");
    strcpy(menu_items[7], "?");
    menu_items[7][0] = 0;
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
               qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
    }
    bool redraw = true;
    bool menu_done = false;
    while (!menu_done && !hangup && !done) {
      command = side_menu(&side_pos, redraw, menu_items, 1,
#ifdef MAX_SCREEN_LINES_TO_SHOW
                          GetSession()->GetCurrentUser()->GetScreenLines() - STOP_LIST > MAX_SCREEN_LINES_TO_SHOW - STOP_LIST ?
                          MAX_SCREEN_LINES_TO_SHOW - STOP_LIST :
                          GetSession()->GetCurrentUser()->GetScreenLines() - STOP_LIST, &smc);
#else
                          GetSession()->GetCurrentUser()->GetScreenLines() - STOP_LIST, &smc);
#endif
      lines_listed = 0;
      redraw = true;
      GetSession()->bout.Color(0);
      if (do_sysop_command(command)) {
        menu_done = true;
        amount = 0;
      }
      switch (command) {
      case '?':
      case CO:
        GetSession()->bout.ClearScreen();
        printfile(SCONFIG_HLP);
        pausescr();
        menu_done = true;
        amount = 0;
        break;
      case COMMAND_DOWN:
        undrawscan(pos, type ? is_inscan(top + pos) :
                   qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
        ++pos;
        if (pos >= amount) {
          pos = 0;
        }
        drawscan(pos, type ? is_inscan(top + pos) :
                 qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case COMMAND_UP:
        undrawscan(pos, type ? is_inscan(top + pos) :
                   qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
        if (!pos) {
          pos = amount - 1;
        } else {
          --pos;
        }
        drawscan(pos, type ? is_inscan(top + pos) :
                 qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case SPACE:
        if (type == 0) {
#ifdef NOTOGGLESYSOP
          if (usub[top + pos].subnum == 0) {
            qsc_q[usub[top + pos].subnum / 32] |= (1L << (usub[top + pos].subnum % 32));
          } else
#endif
            qsc_q[usub[top + pos].subnum / 32] ^= (1L << (usub[top + pos].subnum % 32));
        } else {
          if (wwiv::strings::IsEquals(udir[0].keys, "0")) {
            sysdir = 1;
          }
          for (this_dir = 0; (this_dir < GetSession()->num_dirs); this_dir++) {
            sprintf(s, "%d", sysdir ? top + pos : top + pos + 1);
            if (wwiv::strings::IsEquals(s, udir[this_dir].keys)) {
              ad = udir[this_dir].subnum;
              qsc_n[ad / 32] ^= (1L << (ad % 32));
            }
          }
        }
        drawscan(pos, type ? is_inscan(top + pos) :
                 qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
        redraw = false;
        break;
      case EXECUTE:
        if (!useconf && side_pos > 5) {
          side_pos += 2;
        }
        switch (side_pos) {
        case 0:
          top += amount;
          if (type == 0) {
            if (top >= GetSession()->num_subs) {
              top = 0;
            }
          } else {
            if (top >= GetSession()->num_dirs) {
              top = 0;
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 1:
          if (top > GetSession()->GetCurrentUser()->GetScreenLines() - 4) {
            top -= GetSession()->GetCurrentUser()->GetScreenLines() - 4;
          } else {
            top = 0;
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 2:
          if (type == 0) {
            qsc_q[usub[top + pos].subnum / 32] ^= (1L << (usub[top + pos].subnum % 32));
          } else {
            int this_dir, sysdir = 0;
            int ad;
            if (wwiv::strings::IsEquals(udir[0].keys, "0")) {
              sysdir = 1;
            }
            for (this_dir = 0; (this_dir < GetSession()->num_dirs); this_dir++) {
              sprintf(s, "%d", sysdir ? top + pos : top + pos + 1);
              if (wwiv::strings::IsEquals(s, udir[this_dir].keys)) {
                ad = udir[this_dir].subnum;
                qsc_n[ad / 32] ^= (1L << (ad % 32));
              }
            }
          }
          drawscan(pos, type ? is_inscan(top + pos) :
                   qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
          redraw = false;
          break;
        case 3:
          if (type == 0) {
            for (this_sub = 0; this_sub < GetSession()->num_subs; this_sub++) {
              if (qsc_q[usub[this_sub].subnum / 32] & (1L << (usub[this_sub].subnum % 32))) {
                qsc_q[usub[this_sub].subnum / 32] ^= (1L << (usub[this_sub].subnum % 32));
              }
            }
          } else {
            for (this_dir = 0; this_dir < GetSession()->num_dirs; this_dir++) {
              if (qsc_n[udir[this_dir].subnum / 32] & (1L << (udir[this_dir].subnum % 32))) {
                qsc_n[udir[this_dir].subnum / 32] ^= 1L << (udir[this_dir].subnum % 32);
              }
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 4:
          if (type == 0) {
            for (this_sub = 0; this_sub < GetSession()->num_subs; this_sub++) {
              if (!(qsc_q[usub[this_sub].subnum / 32] & (1L << (usub[this_sub].subnum % 32)))) {
                qsc_q[usub[this_sub].subnum / 32] ^= (1L << (usub[this_sub].subnum % 32));
              }
            }
          } else {
            for (this_dir = 0; this_dir < GetSession()->num_dirs; this_dir++) {
              if (!(qsc_n[udir[this_dir].subnum / 32] & (1L << (udir[this_dir].subnum % 32)))) {
                qsc_n[udir[this_dir].subnum / 32] ^= 1L << (udir[this_dir].subnum % 32);
              }
            }
          }
          pos = 0;
          menu_done = true;
          amount = 0;
          break;
        case 5:
          if (type == 0) {
            express = false;
            expressabort = false;
            qscan(top + pos, &i);
          } else {
            i = GetSession()->GetCurrentFileArea();
            GetSession()->SetCurrentFileArea(top + pos);
            GetSession()->tagging = 1;
            listfiles();
            GetSession()->tagging = 0;
            GetSession()->SetCurrentFileArea(i);
          }
          menu_done = true;
          amount = 0;
          break;
        case 6:
          if (okconf(GetSession()->GetCurrentUser())) {
            if (type == 0) {
              if (GetSession()->GetCurrentConferenceMessageArea() > 0) {
                GetSession()->SetCurrentConferenceMessageArea(GetSession()->GetCurrentConferenceMessageArea() - 1);
              } else {
                while ((uconfsub[GetSession()->GetCurrentConferenceMessageArea() + 1].confnum >= 0)
                       && (GetSession()->GetCurrentConferenceMessageArea() < subconfnum - 1)) {
                  GetSession()->SetCurrentConferenceMessageArea(GetSession()->GetCurrentConferenceMessageArea() + 1);
                }
              }
              setuconf(CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1);
            } else {
              if (GetSession()->GetCurrentConferenceFileArea() > 0) {
                GetSession()->SetCurrentConferenceFileArea(GetSession()->GetCurrentConferenceFileArea() - 1);
              } else {
                while ((uconfdir[GetSession()->GetCurrentConferenceFileArea() + 1].confnum >= 0)
                       && (GetSession()->GetCurrentConferenceFileArea() < dirconfnum - 1)) {
                  GetSession()->SetCurrentConferenceFileArea(GetSession()->GetCurrentConferenceFileArea() + 1);
                }
              }
              setuconf(CONF_DIRS, GetSession()->GetCurrentConferenceFileArea(), -1);
            }
            pos = 0;
            menu_done = true;
            amount = 0;
          }
          break;
        case 7:
          if (okconf(GetSession()->GetCurrentUser())) {
            if (type == 0) {
              if ((GetSession()->GetCurrentConferenceMessageArea() < subconfnum - 1)
                  && (uconfsub[GetSession()->GetCurrentConferenceMessageArea() + 1].confnum >= 0)) {
                GetSession()->SetCurrentConferenceMessageArea(GetSession()->GetCurrentConferenceMessageArea() + 1);
              } else {
                GetSession()->SetCurrentConferenceMessageArea(0);
              }
              setuconf(CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1);
            }

            else {
              if ((GetSession()->GetCurrentConferenceFileArea() < dirconfnum - 1)
                  && (uconfdir[GetSession()->GetCurrentConferenceFileArea() + 1].confnum >= 0)) {
                GetSession()->SetCurrentConferenceFileArea(GetSession()->GetCurrentConferenceFileArea() + 1);
              } else {
                GetSession()->SetCurrentConferenceFileArea(0);
              }
              setuconf(CONF_DIRS, GetSession()->GetCurrentConferenceFileArea(), -1);
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
          GetSession()->bout.ClearScreen();
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
  lines_listed = 0;
  GetSession()->bout.NewLine();
  BbsFree2D(menu_items);
}


void drawscan(int filepos, long tagged) {
  int max_lines = GetMaxLinesToShowForScanPlus();
  if (filepos >= max_lines) {
    GetSession()->bout.GotoXY(40, 3 + filepos - max_lines);
  } else {
    GetSession()->bout.GotoXY(1, filepos + 3);
  }

  GetSession()->bout.SystemColor(BLACK + (CYAN << 4));
  GetSession()->bout.WriteFormatted("[%c]", tagged ? '\xFE' : ' ');
  GetSession()->bout.SystemColor(YELLOW + (BLACK << 4));

  if (filepos >= max_lines) {
    GetSession()->bout.GotoXY(41, 3 + filepos - max_lines);
  } else {
    GetSession()->bout.GotoXY(2, filepos + 3);
  }
}


void undrawscan(int filepos, long tagged) {
  int max_lines = GetMaxLinesToShowForScanPlus();

  if (filepos >= max_lines) {
    GetSession()->bout.GotoXY(40, 3 + filepos - max_lines);
  } else {
    GetSession()->bout.GotoXY(1, filepos + 3);
  }
  GetSession()->bout.WriteFormatted("|#7[|#1%c|#7]", tagged ? '\xFE' : ' ');
}


long is_inscan(int dir) {
  bool sysdir = false;
  if (wwiv::strings::IsEquals(udir[0].keys, "0")) {
    sysdir = true;
  }

  for (int this_dir = 0; (this_dir < GetSession()->num_dirs); this_dir++) {
    char szDir[50];
    sprintf(szDir, "%d", sysdir ? dir : dir + 1);
    if (wwiv::strings::IsEquals(szDir, udir[this_dir].keys)) {
      int ad = udir[this_dir].subnum;
      return (qsc_n[ad / 32] & (1L << ad % 32));
    }
  }

  return 0;
}


void reset_user_colors_to_defaults() {
  for (int i = 0; i <= 9; i++) {
    GetSession()->GetCurrentUser()->SetColor(i, GetSession()->newuser_colors[ i ]);
    GetSession()->GetCurrentUser()->SetBWColor(i, GetSession()->newuser_bwcolors[ i ]);
  }
}

