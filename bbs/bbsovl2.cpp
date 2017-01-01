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
#include "bbs/bbsovl2.h"

#include <chrono>
#include <string>

#include "bbs/confutil.h"
#include "bbs/bbs.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/keycodes.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wwivport.h"

using std::string;
using std::chrono::milliseconds;
using namespace wwiv::os;
using namespace wwiv::strings;

#define PREV                1
#define NEXT                2
#define DONE                4
#define ABORTED             8

// Allows local-only editing of some of the user data in a shadowized window.
void OnlineUserEditor() {
  char sl[4], dsl[4], exempt[4], sysopsub[4], ar[17], dar[17], restrict[17], rst[17], uk[8], dk[8], up[6], down[6],
       posts[6], banktime[6], gold[10], ass[6], logons[6];
  int cp, i, rc = ABORTED;

  a()->localIO()->savescreen();
  a()->DisplaySysopWorkingIndicator(true);
  int wx = 5;
  int wy = 3;
  a()->localIO()->MakeLocalWindow(wx, wy - 2, 70, 16 + 2);
  const string bar = StringPrintf("\xC3%s\xB4", charstr(70 - wx + 3, '\xC4'));
  a()->localIO()->PutsXY(wx, wy, bar);
  a()->localIO()->PutsXY(wx, wy + 4, bar);
  a()->localIO()->PutsXY(wx, wy + 7, bar);
  a()->localIO()->PutsXY(wx, wy + 11, bar);
  a()->localIO()->PutsXY(wx, wy + 13, bar);
  sprintf(sl, "%u", a()->user()->GetSl());
  sprintf(dsl, "%u", a()->user()->GetDsl());
  sprintf(exempt, "%u", a()->user()->GetExempt());
  if (*qsc > 999) {
    *qsc = 999;
  }
  sprintf(sysopsub, "%lu", *qsc);
  sprintf(uk, "%lu", a()->user()->GetUploadK());
  sprintf(dk, "%lu", a()->user()->GetDownloadK());
  sprintf(up, "%u", a()->user()->GetFilesUploaded());
  sprintf(down, "%u", a()->user()->GetFilesDownloaded());
  sprintf(posts, "%u", a()->user()->GetNumMessagesPosted());
  sprintf(banktime, "%u", a()->user()->GetTimeBankMinutes());
  sprintf(logons, "%u", a()->user()->GetNumLogons());
  sprintf(ass, "%u", a()->user()->GetAssPoints());

  sprintf(gold, "%7.2f", a()->user()->GetGold());
  strcpy(rst, restrict_string);
  for (i = 0; i <= 15; i++) {
    if (a()->user()->HasArFlag(1 << i)) {
      ar[i] = (char)('A' + i);
    } else {
      ar[i] = SPACE;
    }
    if (a()->user()->HasDarFlag(1 << i)) {
      dar[i] = (char)('A' + i);
    } else {
      dar[i] = SPACE;
    }
    if (a()->user()->HasRestrictionFlag(1 << i)) {
      restrict[i] = rst[i];
    } else {
      restrict[i] = SPACE;
    }
  }
  dar[16] = '\0';
  ar[16]  = '\0';
  restrict[16] = '\0';
  cp = 0;
  bool done = false;

  // heading
  string s = StrCat("[", a()->names()->UserName(a()->usernum), "]");
  StringJustify(&s, 37, SPACE, JustificationType::RIGHT);
  a()->localIO()->PrintfXYA(wx + 1, wy - 1, 31, " %-29.29s%s ", "WWIV User Editor", s.c_str());

  a()->localIO()->PrintfXYA(wx + 2,  wy + 1, 3,   "Security Level(SL): %s", sl);
  a()->localIO()->PrintfXYA(wx + 36, wy + 1, 3,   "  Message AR: %s", ar);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 2, 3,   "DL Sec. Level(DSL): %s", dsl);
  a()->localIO()->PrintfXYA(wx + 36, wy + 2, 3,   " Download AR: %s", dar);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 3, 3,   "   User Exemptions: %s", exempt);
  a()->localIO()->PrintfXYA(wx + 36, wy + 3, 3,   "Restrictions: %s", restrict);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 5, 3,   "         Sysop Sub: %s", sysopsub);
  a()->localIO()->PrintfXYA(wx + 36, wy + 5, 3,   "   Time Bank: %s", banktime);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 6, 3,   "        Ass Points: %s", ass);
  a()->localIO()->PrintfXYA(wx + 36, wy + 6, 3,   " Gold Points: %s", gold);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 8, 3,   "       KB Uploaded: %s", uk);
  a()->localIO()->PrintfXYA(wx + 35, wy + 8, 3,   "KB Downloaded: %s", dk);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 9, 3,   "    Files Uploaded: %s", up);
  a()->localIO()->PrintfXYA(wx + 32, wy + 9, 3,   "Files Downloaded: %s", down);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 10, 3,  "   Messages Posted: %s", posts);
  a()->localIO()->PrintfXYA(wx + 32, wy + 10, 3,  "Number of Logons: %s", logons);
  a()->localIO()->PrintfXYA(wx + 2,  wy + 12, 3,  "Note: %s", a()->user()->GetNote());
  a()->localIO()->PrintfXYA(wx + 1, wy + 14, 31,
                                          "    (ENTER) Next Field   (UP-ARROW) Previous Field    (ESC) Exit    ");
  curatr = 3;
  while (!done) {
    switch (cp) {
    case 0:
      a()->localIO()->GotoXY(wx + 22, wy + 1);
      a()->localIO()->EditLine(sl, 3, NUM_ONLY, &rc, "");
      a()->user()->SetSl(atoi(sl));
      sprintf(sl, "%d", a()->user()->GetSl());
      a()->localIO()->Printf("%-3s", sl);
      break;
    case 1:
      a()->localIO()->GotoXY(wx + 50, wy + 1);
      a()->localIO()->EditLine(ar, 16, SET, &rc, "ABCDEFGHIJKLMNOP ");
      a()->user()->SetAr(0);
      for (i = 0; i <= 15; i++) {
        if (ar[i] != SPACE) {
          a()->user()->SetArFlag(1 << i);
        }
      }
      break;
    case 2:
      a()->localIO()->GotoXY(wx + 22, wy + 2);
      a()->localIO()->EditLine(dsl, 3, NUM_ONLY, &rc, "");
      a()->user()->SetDsl(atoi(dsl));
      sprintf(dsl, "%d", a()->user()->GetDsl());
      a()->localIO()->Printf("%-3s", dsl);
      break;
    case 3:
      a()->localIO()->GotoXY(wx + 50, wy + 2);
      a()->localIO()->EditLine(dar, 16, SET, &rc, "ABCDEFGHIJKLMNOP ");
      a()->user()->SetDar(0);
      for (i = 0; i <= 15; i++) {
        if (dar[i] != SPACE) {
          a()->user()->SetDarFlag(1 << i);
        }
      }
      break;
    case 4:
      a()->localIO()->GotoXY(wx + 22, wy + 3);
      a()->localIO()->EditLine(exempt, 3, NUM_ONLY, &rc, "");
      a()->user()->SetExempt(atoi(exempt));
      sprintf(exempt, "%u", a()->user()->GetExempt());
      a()->localIO()->Printf("%-3s", exempt);
      break;
    case 5:
      a()->localIO()->GotoXY(wx + 50, wy + 3);
      a()->localIO()->EditLine(restrict, 16, SET, &rc, rst);
      a()->user()->SetRestriction(0);
      for (i = 0; i <= 15; i++) {
        if (restrict[i] != SPACE) {
          a()->user()->SetRestrictionFlag(1 << i);
        }
      }
      break;
    case 6:
      a()->localIO()->GotoXY(wx + 22, wy + 5);
      a()->localIO()->EditLine(sysopsub, 3, NUM_ONLY, &rc, "");
      *qsc = atoi(sysopsub);
      sprintf(sysopsub, "%lu", *qsc);
      a()->localIO()->Printf("%-3s", sysopsub);
      break;
    case 7:
      a()->localIO()->GotoXY(wx + 50, wy + 5);
      a()->localIO()->EditLine(banktime, 5, NUM_ONLY, &rc, "");
      a()->user()->SetTimeBankMinutes(atoi(banktime));
      sprintf(banktime, "%u", a()->user()->GetTimeBankMinutes());
      a()->localIO()->Printf("%-5s", banktime);
      break;
    case 8:
      a()->localIO()->GotoXY(wx + 22, wy + 6);
      a()->localIO()->EditLine(ass, 5, NUM_ONLY, &rc, "");
      a()->user()->SetAssPoints(atoi(ass));
      sprintf(ass, "%u", a()->user()->GetAssPoints());
      a()->localIO()->Printf("%-5s", ass);
      break;
    case 9:
      a()->localIO()->GotoXY(wx + 50, wy + 6);
      a()->localIO()->EditLine(gold, 5, NUM_ONLY, &rc, "");
      a()->user()->SetGold(static_cast<float>(atof(gold)));
      sprintf(gold, "%7.2f", a()->user()->GetGold());
      a()->localIO()->Printf("%-5s", gold);
      break;
    case 10:
      a()->localIO()->GotoXY(wx + 22, wy + 8);
      a()->localIO()->EditLine(uk, 7, NUM_ONLY, &rc, "");
      a()->user()->SetUploadK(atol(uk));
      sprintf(uk, "%lu", a()->user()->GetUploadK());
      a()->localIO()->Printf("%-7s", uk);
      break;
    case 11:
      a()->localIO()->GotoXY(wx + 50, wy + 8);
      a()->localIO()->EditLine(dk, 7, NUM_ONLY, &rc, "");
      a()->user()->SetDownloadK(atol(dk));
      sprintf(dk, "%lu", a()->user()->GetDownloadK());
      a()->localIO()->Printf("%-7s", dk);
      break;
    case 12:
      a()->localIO()->GotoXY(wx + 22, wy + 9);
      a()->localIO()->EditLine(up, 5, NUM_ONLY, &rc, "");
      a()->user()->SetFilesUploaded(atoi(up));
      sprintf(up, "%u", a()->user()->GetFilesUploaded());
      a()->localIO()->Printf("%-5s", up);
      break;
    case 13:
      a()->localIO()->GotoXY(wx + 50, wy + 9);
      a()->localIO()->EditLine(down, 5, NUM_ONLY, &rc, "");
      a()->user()->SetFilesDownloaded(atoi(down));
      sprintf(down, "%u", a()->user()->GetFilesDownloaded());
      a()->localIO()->Printf("%-5s", down);
      break;
    case 14:
      a()->localIO()->GotoXY(wx + 22, wy + 10);
      a()->localIO()->EditLine(posts, 5, NUM_ONLY, &rc, "");
      a()->user()->SetNumMessagesPosted(atoi(posts));
      sprintf(posts, "%u", a()->user()->GetNumMessagesPosted());
      a()->localIO()->Printf("%-5s", posts);
      break;
    case 15:
      a()->localIO()->GotoXY(wx + 50, wy + 10);
      a()->localIO()->EditLine(logons, 5, NUM_ONLY, &rc, "");
      a()->user()->SetNumLogons(atoi(logons));
      sprintf(logons, "%u", a()->user()->GetNumLogons());
      a()->localIO()->Printf("%-5s", logons);
      break;
    case 16: {
      char szNote[ 81 ];
      a()->localIO()->GotoXY(wx + 8, wy + 12);
      strcpy(szNote, a()->user()->GetNote());
      a()->localIO()->EditLine(szNote, 60, ALL, &rc, "");
      StringTrimEnd(szNote);
      a()->user()->SetNote(szNote);
    }
    break;
    }
    switch (rc) {
    case ABORTED:
      done = true;
      break;
    case DONE:
      done = true;
      break;
    case NEXT:
      cp = (cp + 1) % 17;
      break;
    case PREV:
      cp--;
      if (cp < 0) {
        cp = 16;
      }
      break;
    }
  }
  a()->localIO()->restorescreen();
  a()->ResetEffectiveSl();
  changedsl();
  a()->DisplaySysopWorkingIndicator(false);
}

/**
 * This function prints out a string, with a user-specifiable delay between
 * each character, and a user-definable pause after the entire string has
 * been printed, then it backspaces the string. The color is also definable.
 * The parameters are as follows:
 * <p>
 * <em>Note: ANSI is not required.</em>
 * <p>
 * Example:
 * <p>
 * BackPrint("This is an example.",3,20,500);
 *
 * @param strText  The string to print
 * @param nColorCode The color of the string
 * @param nCharDelay Delay between each character, in milliseconds
 * @param nStringDelay Delay between completion of string and backspacing
 */
void BackPrint(const string& strText, int nColorCode, int nCharDelay, int nStringDelay) {
  bool oecho = local_echo;
  local_echo = true;
  bout.Color(nColorCode);
  sleep_for(milliseconds(nCharDelay));
  for (auto iter = strText.cbegin(); iter != strText.cend() && !hangup; ++iter) {
    bout.bputch(*iter);
    sleep_for(milliseconds(nCharDelay));
  }

  sleep_for(milliseconds(nStringDelay));
  for (auto iter = strText.cbegin(); iter != strText.cend() && !hangup; ++iter) {
    bout.bs();
    sleep_for(milliseconds(5));
  }
  local_echo = oecho;
}

/**
 * This function will reposition the cursor i spaces to the left, or if the
 * cursor is on the left side of the screen already then it will not move.
 * If the user has no ANSI then nothing happens.
 * @param numOfChars Number of characters to move to the left
 */
void MoveLeft(int numOfChars) {
  if (okansi()) {
    bout << "\x1b[" << numOfChars << "D";
  }
}

/**
 * This function will print out a string, making each character "spin"
 * using the / - \ | sequence. The color is definable and is the
 * second parameter, not the first. If the user does not have ANSI
 * then the string is simply printed normally.
 * @param
 */
void SpinPuts(const string& strText, int nColorCode) {
  bool oecho  = local_echo;
  local_echo    = true;

  if (okansi()) {
    bout.Color(nColorCode);
    const int dly = 30;
    for (auto iter = strText.cbegin(); iter != strText.cend() && !hangup; ++iter) {
      sleep_for(milliseconds(dly));
      bout << "/";
      MoveLeft(1);
      sleep_for(milliseconds(dly));
      bout << "-";
      MoveLeft(1);
      sleep_for(milliseconds(dly));
      bout << "\\";
      MoveLeft(1);
      sleep_for(milliseconds(dly));
      bout << "|";
      MoveLeft(1);
      sleep_for(milliseconds(dly));
      bout.bputch(*iter);
    }
  } else {
    bout << strText;
  }
  local_echo = oecho;
}
