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
#include <string>

#include "bbs/wconstants.h"
#include "bbs/wwiv.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "bbs/keycodes.h"

using std::string;

static char str_yes[81],
            str_no[81];
char  str_pause[81],
      str_quit[81];


/**
 * Copies the next line located at pszWholeBuffer[ plBufferPtr ] to pszOutLine
 *
 * @param @pszOutLine The output buffer
 * @param pszWholeBuffer The original text buffer
 * @param plBufferPtr The offset into pszWholeBuffer
 * @param lBufferLength The length of pszWholeBuffer
 */
void copy_line(char *pszOutLine, char *pszWholeBuffer, long *plBufferPtr, long lBufferLength) {
  WWIV_ASSERT(pszOutLine);
  WWIV_ASSERT(pszWholeBuffer);
  WWIV_ASSERT(plBufferPtr);

  if (*plBufferPtr >= lBufferLength) {
    pszOutLine[0] = '\0';
    return;
  }
  long lCurrentPtr = *plBufferPtr;
  int nLinePtr = 0;
  while ((pszWholeBuffer[lCurrentPtr] != '\r') && (pszWholeBuffer[lCurrentPtr] != '\n')
         && (lCurrentPtr < lBufferLength)) {
    pszOutLine[nLinePtr++] = pszWholeBuffer[lCurrentPtr++];
  }
  pszOutLine[nLinePtr] = '\0';
  if ((pszWholeBuffer[lCurrentPtr] == '\r') && (lCurrentPtr < lBufferLength)) {
    ++lCurrentPtr;
  }
  if ((pszWholeBuffer[lCurrentPtr] == '\n') && (lCurrentPtr < lBufferLength)) {
    ++lCurrentPtr;
  }
  *plBufferPtr = lCurrentPtr;
}

bool inli(string* outBuffer, string* rollOver, string::size_type nMaxLen, bool bAddCRLF,
          bool bAllowPrevious, bool bTwoColorChatMode) {
  char szBuffer[ 4096 ] = {0}, szRollover[ 4096 ] = {0};
  strcpy(szBuffer, outBuffer->c_str());
  strcpy(szRollover, rollOver->c_str());
  bool ret = inli(szBuffer, szRollover, nMaxLen, bAddCRLF, bAllowPrevious, bTwoColorChatMode);
  outBuffer->assign(szBuffer);
  rollOver->assign(szRollover);
  return ret;
}

// returns true if needs to keep inputting this line
bool inli(char *pszBuffer, char *pszRollover, string::size_type nMaxLen, bool bAddCRLF, bool bAllowPrevious,
          bool bTwoColorChatMode) {
  char szRollOver[255];

  WWIV_ASSERT(pszBuffer);
  WWIV_ASSERT(pszRollover);

  int cm = chatting;

  int begx = GetSession()->localIO()->WhereX();
  if (pszRollover[0] != 0) {
    char* ss = szRollOver;
    for (int i = 0; pszRollover[i]; i++) {
      if (pszRollover[i] == CC || pszRollover[i] == CO) {
        *ss++ = 'P' - '@';
      } else {
        *ss++ = pszRollover[i];
      }
      // Add a second ^P if we are working on RIP codes
      if (pszRollover[i] == CO && pszRollover[i + 1] != CO && pszRollover[i - 1] != CO) {
        *ss++ = 'P' - '@';
      }
    }
    *ss = '\0';
    if (charbufferpointer) {
      char szTempBuffer[255];
      strcpy(szTempBuffer, szRollOver);
      strcat(szTempBuffer, &charbuffer[charbufferpointer]);
      strcpy(&charbuffer[1], szTempBuffer);
      charbufferpointer = 1;
    } else {
      strcpy(&charbuffer[1], szRollOver);
      charbufferpointer = 1;
    }
    pszRollover[0] = '\0';
  }
  string::size_type cp = 0;
  bool done = false;
  unsigned char ch = '\0';
  do {
    ch = getkey();
    if (bTwoColorChatMode) {
      GetSession()->bout.Color(GetSession()->IsLastKeyLocal() ? 1 : 0);
    }
    if (cm) {
      if (chatting == 0) {
        ch = RETURN;
      }
    }
    if (ch >= SPACE) {
      if ((GetSession()->localIO()->WhereX() < (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) && (cp < nMaxLen)) {
        pszBuffer[cp++] = ch;
        bputch(ch);
        if (GetSession()->localIO()->WhereX() == (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) {
          done = true;
        }
      } else {
        if (GetSession()->localIO()->WhereX() >= (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) {
          done = true;
        }
      }
    } else switch (ch) {
      case CG:
        if (chatting && outcom) {
          rputch(CG);
        }
        break;
      case RETURN:                            // C/R
        pszBuffer[cp] = '\0';
        done = true;
        break;
      case BACKSPACE:                             // Backspace
        if (cp) {
          if (pszBuffer[cp - 2] == CC) {
            cp -= 2;
            GetSession()->bout.Color(0);
          } else if (pszBuffer[cp - 2] == CO) {
            for (string::size_type i = strlen(interpret(pszBuffer[cp - 1])); i > 0; i--) {
              GetSession()->bout.BackSpace();
            }
            cp -= 2;
            if (pszBuffer[cp - 1] == CO) {
              cp--;
            }
          } else {
            if (pszBuffer[cp - 1] == BACKSPACE) {
              cp--;
              bputch(SPACE);
            } else {
              cp--;
              GetSession()->bout.BackSpace();
            }
          }
        } else if (bAllowPrevious) {
          if (okansi()) {
            if (GetSession()->GetMMKeyArea() == WSession::mmkeyFileAreas) {
              GetSession()->bout << "\r\x1b[K";
            }
            GetSession()->bout << "\x1b[1A";
          } else {
            GetSession()->bout << "[*> Previous Line <*]\r\n";
          }
          return true;
        }
        break;
      case CX:                            // Ctrl-X
        while (GetSession()->localIO()->WhereX() > begx) {
          GetSession()->bout.BackSpace();
          cp = 0;
        }
        GetSession()->bout.Color(0);
        break;
      case CW:                            // Ctrl-W
        if (cp) {
          do {
            if (pszBuffer[cp - 2] == CC) {
              cp -= 2;
              GetSession()->bout.Color(0);
            } else if (pszBuffer[cp - 1] == BACKSPACE) {
              cp--;
              bputch(SPACE);
            } else {
              cp--;
              GetSession()->bout.BackSpace();
            }
          } while (cp && pszBuffer[cp - 1] != SPACE && pszBuffer[cp - 1] != BACKSPACE);
        }
        break;
      case CN:                            // Ctrl-N
        if (GetSession()->localIO()->WhereX() && cp < nMaxLen) {
          bputch(BACKSPACE);
          pszBuffer[cp++] = BACKSPACE;
        }
        break;
      case CP:                            // Ctrl-P
        if (cp < nMaxLen - 1) {
          ch = getkey();
          if (ch >= SPACE && ch <= 126) {
            pszBuffer[cp++] = CC;
            pszBuffer[cp++] = ch;
            GetSession()->bout.Color(ch - '0');
          } else if (ch == CP && cp < nMaxLen - 2) {
            ch = getkey();
            if (ch != CP) {
              pszBuffer[cp++] = CO;
              pszBuffer[cp++] = CO;
              pszBuffer[cp++] = ch;
              bputch('\xf');
              bputch('\xf');
              bputch(ch);
            }
          }
        }
        break;
      case TAB: {                           // Tab
        int charsNeeded = 5 - (cp % 5);
        if ((cp + charsNeeded) < nMaxLen
            && (GetSession()->localIO()->WhereX() + charsNeeded) < GetSession()->GetCurrentUser()->GetScreenChars()) {
          charsNeeded = 5 - ((GetSession()->localIO()->WhereX() + 1) % 5);
          for (int j = 0; j < charsNeeded; j++) {
            pszBuffer[cp++] = SPACE;
            bputch(SPACE);
          }
        }
      }
      break;
      }
  } while (!done && !hangup);

  if (hangup) {
    // Caller isn't here, so we are not saving any message.
    return false;
  }

  if (ch != RETURN) {
    string::size_type lastwordstart = cp - 1;
    while (lastwordstart > 0 && pszBuffer[lastwordstart] != SPACE && pszBuffer[lastwordstart] != BACKSPACE) {
      lastwordstart--;
    }
    if (lastwordstart > static_cast<string::size_type>(GetSession()->localIO()->WhereX() / 2)
        && lastwordstart != (cp - 1)) {
      string::size_type lastwordlen = cp - lastwordstart - 1;
      for (string::size_type j = 0; j < lastwordlen; j++) {
        bputch(BACKSPACE);
      }
      for (string::size_type j = 0; j < lastwordlen; j++) {
        bputch(SPACE);
      }
      for (string::size_type j = 0; j < lastwordlen; j++) {
        pszRollover[j] = pszBuffer[cp - lastwordlen + j];
      }
      pszRollover[lastwordlen] = '\0';
      cp -= lastwordlen;
    }
    pszBuffer[cp++] = CA;
    pszBuffer[cp] = '\0';
  }
  if (bAddCRLF) {
    GetSession()->bout.NewLine();
  }
  return false;
}


// Returns 1 if current user has sysop access (permanent or temporary), else
// returns 0.
bool so() {
  return (GetSession()->GetEffectiveSl() == 255);
}

/**
 * Checks for Co-SysOp status
 * @return true if current user has limited co-sysop access (or better)
 */
bool cs() {
  if (so()) {
    return true;
  }

  return (getslrec(GetSession()->GetEffectiveSl()).ability & ability_cosysop) ? true : false;
}


/**
 * Checks for limitied Co-SysOp status
 * <em>Note: Limited co sysop status may be for this message area only.</em>
 *
 * @return true if current user has limited co-sysop access (or better)
 */
bool lcs() {
  if (cs()) {
    return true;
  }

  if (getslrec(GetSession()->GetEffectiveSl()).ability & ability_limited_cosysop) {
    if (*qsc == 999) {
      return true;
    }
    return (*qsc == static_cast<uint32_t>(usub[GetSession()->GetCurrentMessageArea()].subnum)) ? true : false;
  }
  return false;
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool checka() {
  bool ignored_abort = false;
  bool ignored_next = false;
  return checka(&ignored_abort, &ignored_next);
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool checka(bool *abort) {
  bool ignored_next = false;
  return checka(abort, &ignored_next);
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets next to true if control-N was hit, for zipping past messages quickly.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool checka(bool *abort, bool *next) {
  if (nsp == -1) {
    *abort = true;
    nsp = 0;
  }
  while (bkbhit() && !*abort && !hangup) {
    CheckForHangup();
    char ch = bgetch();
    if (!GetSession()->tagging || GetSession()->GetCurrentUser()->IsUseNoTagging()) {
      lines_listed = 0;
    }
    switch (ch) {
    case CN:
      *next = true;
    case CC:
    case SPACE:
    case CX:
    case 'Q':
    case 'q':
      *abort = true;
      break;
    case 'P':
    case 'p':
    case CS:
      ch = getkey();
      break;
    }
  }
  return *abort;
}

// Prints an abortable string (contained in *pszText). Returns 1 in *abort if the
// string was aborted, else *abort should be zero.
void pla(const string& text, bool *abort) {
  if (CheckForHangup()) {
    *abort = true;
  }
  checka(abort);
  for (string::const_iterator iter = text.begin(); iter != text.end() && !*abort; ++iter) {
    bputch(*iter, true);
    checka(abort);
  }
  FlushOutComChBuffer();
  if (!*abort) {
    GetSession()->bout.NewLine();
  }
}

void plal(const string& text, string::size_type limit, bool *abort) {
  CheckForHangup();
  if (hangup) {
    *abort = true;
  }

  checka(abort);

  limit += text.length() - stripcolors(text).length();
  string::size_type nCharsDisplayed = 0;
  for (string::const_iterator iter = text.begin(); iter != text.end() && nCharsDisplayed++ < limit
       && !*abort; ++iter) {
    if (*iter != '\r' && *iter != '\n') {
      bputch(*iter, true);
    }
    checka(abort);
  }

  FlushOutComChBuffer();
  if (!*abort) {
    GetSession()->bout.NewLine();
  }
}

// Returns 1 if sysop is "chattable", else returns 0. Takes into account
// current user's chat restriction (if any) and sysop high and low times,
// if any, as well as status of scroll-lock key.
bool sysop2() {
  bool ok = sysop1();
  if (GetSession()->GetCurrentUser()->IsRestrictionChat()) {
    ok = false;
  }
  if (syscfg.sysoplowtime != syscfg.sysophightime) {
    if (syscfg.sysophightime > syscfg.sysoplowtime) {
      if (timer() <= (syscfg.sysoplowtime * SECONDS_PER_MINUTE_FLOAT) ||
          timer() >= (syscfg.sysophightime * SECONDS_PER_MINUTE_FLOAT)) {
        ok = false;
      }
    } else if (timer() <= (syscfg.sysoplowtime * SECONDS_PER_MINUTE_FLOAT) &&
               timer() >= (syscfg.sysophightime * SECONDS_PER_MINUTE_FLOAT)) {
      ok = false;
    }
  }
  return ok;
}

// Returns 1 if computer type string in *s matches the current user's
// defined computer type, else returns 0.
bool checkcomp(const char *pszComputerType) {
  WWIV_ASSERT(pszComputerType);
  return strstr(ctypes(GetSession()->GetCurrentUser()->GetComputerType()), pszComputerType) ? true : false;
}


// Returns 1 if ANSI detected, or if local user, else returns 0. Uses the
// cursor position interrogation ANSI sequence for remote detection.
// If the user is asked and choosed NO, then -1 is returned.
int check_ansi() {
  if (!incom) {
    return 1;
  }

  while (bkbhitraw()) {
    bgetchraw();
  }

  rputs("\x1b[6n");

  long l = timer1() + 36 + 18;

  while ((timer1() < l) && (!hangup)) {
    CheckForHangup();
    char ch = bgetchraw();
    if (ch == '\x1b') {
      l = timer1() + 18;
      while (timer1() < l && !hangup) {
        if ((timer1() + 1820) < l) {
          l = timer1() + 18;
        }
        CheckForHangup();
        ch = bgetchraw();
        if (ch) {
          if ((ch < '0' || ch > '9') && ch != ';' && ch != '[') {
            return 1;
          }
        }
      }
      return 1;
    } else if (ch == 'N') {
      return -1;
    }
    if ((timer1() + 1820) < l) {
      l = timer1() + 36;
    }
  }
  return 0;
}


// Sets current language to language index n, reads in menus and help files,
// and initializes stringfiles for that language. Returns false if problem,
// else returns true.
bool set_language_1(int n) {
  int idx = 0;
  for (idx = 0; idx < GetSession()->num_languages; idx++) {
    if (languages[idx].num == n) {
      break;
    }
  }

  if (idx >= GetSession()->num_languages && n == 0) {
    idx = 0;
  }

  if (idx >= GetSession()->num_languages) {
    return false;
  }

  GetSession()->SetCurrentLanguageNumber(n);
  cur_lang_name = languages[idx].name;
  GetSession()->language_dir = languages[idx].dir;

  strncpy(str_yes, "Yes", sizeof(str_yes) - 1);
  strncpy(str_no, "No", sizeof(str_no) - 1);
  strncpy(str_quit, "Quit", sizeof(str_quit) - 1);
  strncpy(str_pause, "More? [Y/n/c]", sizeof(str_pause) - 1);
  str_yes[0] = upcase(str_yes[0]);
  str_no[0] = upcase(str_no[0]);
  str_quit[0] = upcase(str_quit[0]);

  return true;
}


//
// Sets language to language #n, returns false if a problem, else returns true.
//
bool set_language(int n) {
  if (GetSession()->GetCurrentLanguageNumber() == n) {
    return true;
  }

  int nOldCurLang = GetSession()->GetCurrentLanguageNumber();

  if (!set_language_1(n)) {
    if (nOldCurLang >= 0) {
      if (!set_language_1(nOldCurLang)) {
        set_language_1(0);
      }
    }
    return false;
  }
  return true;
}


char *mmkey(int dl, bool bListOption) {
  static char cmd1[10], cmd2[81], ch;
  int i, p, cp;

  do {
    do {
      ch = getkey();
      if (bListOption && (ch == RETURN || ch == SPACE)) {
        ch = upcase(ch);
        cmd1[0] = ch;
        return cmd1;
      }
    } while ((ch <= ' ' || ch == RETURN || ch > 126) && !hangup);
    ch = upcase(ch);
    bputch(ch);
    if (ch == RETURN) {
      cmd1[0] = '\0';
    } else {
      cmd1[0] = ch;
    }
    cmd1[1] = '\0';
    p = 0;
    switch (dl) {

    case 1:
      if (strchr(dtc, ch) != nullptr) {
        p = 2;
      } else if (strchr(dcd, ch) != nullptr) {
        p = 1;
      }
      break;
    case 2:
      if (strchr(odc, ch) != nullptr) {
        p = 1;
      }
      break;
    case 0:
      if (strchr(tc, ch) != nullptr) {
        p = 2;
      } else if (strchr(dc, ch) != nullptr) {
        p = 1;
      }
      break;
    }
    if (p) {
      cp = 1;
      do {
        do {
          ch = getkey();
        } while ((((ch < ' ') && (ch != RETURN) && (ch != BACKSPACE)) || (ch > 126)) && !hangup);
        ch = upcase(ch);
        if (ch == RETURN) {
          GetSession()->bout.NewLine();
          if (dl == 2) {
            GetSession()->bout.NewLine();
          }
          if (!GetSession()->GetCurrentUser()->IsExpert() && !okansi()) {
            newline = true;
          }
          return cmd1;
        } else {
          if (ch == BACKSPACE) {
            GetSession()->bout.BackSpace();
            cmd1[ --cp ] = '\0';
          } else {
            cmd1[ cp++ ]  = ch;
            cmd1[ cp ]    = '\0';
            bputch(ch);
            if (ch == '/' && cmd1[0] == '/') {
              input(cmd2, 50);
              if ((GetSession()->GetMMKeyArea() != WSession::mmkeyMessageAreas &&
                   GetSession()->GetMMKeyArea() != WSession::mmkeyFileAreas && dl != 2) || !newline) {
                if (isdigit(cmd2[0])) {
                  if (GetSession()->GetMMKeyArea() == WSession::mmkeyMessageAreas && dl == 0) {
                    for (i = 0; i < GetSession()->num_subs && usub[i].subnum != -1; i++) {
                      if (wwiv::strings::IsEquals(usub[i].keys, cmd2)) {
                        GetSession()->bout.NewLine();
                        break;
                      }
                    }
                  }
                  if (GetSession()->GetMMKeyArea() == WSession::mmkeyFileAreas && dl == 1) {
                    for (i = 0; i < GetSession()->num_dirs; i++) {
                      if (wwiv::strings::IsEquals(udir[i].keys, cmd2)) {
                        GetSession()->bout.NewLine();
                        break;
                      }
                    }
                  }
                  if (dl == 2) {
                    GetSession()->bout.NewLine();
                  }
                } else {
                  GetSession()->bout.NewLine();
                }
                newline = true;
              }
              return cmd2;
            } else if (cp == p + 1) {
              if ((GetSession()->GetMMKeyArea() != WSession::mmkeyMessageAreas &&
                   GetSession()->GetMMKeyArea() != WSession::mmkeyFileAreas && dl != 2) || !newline) {
                if (isdigit(cmd1[ 0 ])) {
                  if (dl == 2 || !okansi()) {
                    GetSession()->bout.NewLine();
                  }
                  if (!GetSession()->GetCurrentUser()->IsExpert() && !okansi()) {
                    newline = true;
                  }
                } else {
                  GetSession()->bout.NewLine();
                  newline = true;
                }
              } else {
                GetSession()->bout.NewLine();
                newline = true;
              }
              return cmd1;
            }
          }
        }
      } while (cp > 0);
    } else {
      if ((GetSession()->GetMMKeyArea() != WSession::mmkeyMessageAreas &&
           GetSession()->GetMMKeyArea() != WSession::mmkeyFileAreas && dl != 2) || !newline) {
        switch (cmd1[0]) {
        case '>':
        case '+':
        case ']':
        case '}':
        case '<':
        case '-':
        case '[':
        case '{':
        case 'H':
          if (dl == 2 || !okansi()) {
            GetSession()->bout.NewLine();
          }
          if (!GetSession()->GetCurrentUser()->IsExpert() && !okansi()) {
            newline = true;
          }
          break;
        default:
          if (isdigit(cmd1[0])) {
            if (dl == 2 || !okansi()) {
              GetSession()->bout.NewLine();
            }
            if (!GetSession()->GetCurrentUser()->IsExpert() && !okansi()) {
              newline = true;
            }
          } else {
            GetSession()->bout.NewLine();
            newline = true;
          }
          break;
        }
      } else {
        GetSession()->bout.NewLine();
        newline = true;
      }
      return cmd1;
    }
  } while (!hangup);
  cmd1[0] = '\0';
  return cmd1;
}


const char *YesNoString(bool bYesNo) {
  return (bYesNo) ? str_yes : str_no;
}
