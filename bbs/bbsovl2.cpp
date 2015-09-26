/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include <chrono>
#include <string>

#include "bbs/wwiv.h"
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
#if !defined ( __unix__ )
  char sl[4], dsl[4], exempt[4], sysopsub[4], ar[17], dar[17], restrict[17], rst[17], uk[8], dk[8], up[6], down[6],
       posts[6], banktime[6], gold[10], ass[6], logons[6];
  int cp, i, rc = ABORTED;

  session()->DisplaySysopWorkingIndicator(true);
  session()->localIO()->savescreen();
  curatr = session()->GetUserEditorColor();
  int wx = 5;
  int wy = 3;
  session()->localIO()->MakeLocalWindow(wx, wy - 2, 70, 16 + 2);
  const string bar = StringPrintf("\xC3%s\xB4", charstr(70 - wx + 3, '\xC4'));
  session()->localIO()->LocalXYPuts(wx, wy, bar);
  session()->localIO()->LocalXYPuts(wx, wy + 4, bar);
  session()->localIO()->LocalXYPuts(wx, wy + 7, bar);
  session()->localIO()->LocalXYPuts(wx, wy + 11, bar);
  session()->localIO()->LocalXYPuts(wx, wy + 13, bar);
  sprintf(sl, "%u", session()->user()->GetSl());
  sprintf(dsl, "%u", session()->user()->GetDsl());
  sprintf(exempt, "%u", session()->user()->GetExempt());
  if (*qsc > 999) {
    *qsc = 999;
  }
  sprintf(sysopsub, "%lu", *qsc);
  sprintf(uk, "%lu", session()->user()->GetUploadK());
  sprintf(dk, "%lu", session()->user()->GetDownloadK());
  sprintf(up, "%u", session()->user()->GetFilesUploaded());
  sprintf(down, "%u", session()->user()->GetFilesDownloaded());
  sprintf(posts, "%u", session()->user()->GetNumMessagesPosted());
  sprintf(banktime, "%u", session()->user()->GetTimeBankMinutes());
  sprintf(logons, "%u", session()->user()->GetNumLogons());
  sprintf(ass, "%u", session()->user()->GetAssPoints());

  _gcvt(session()->user()->GetGold(), 5, gold);
  strcpy(rst, restrict_string);
  for (i = 0; i <= 15; i++) {
    if (session()->user()->HasArFlag(1 << i)) {
      ar[i] = (char)('A' + i);
    } else {
      ar[i] = SPACE;
    }
    if (session()->user()->HasDarFlag(1 << i)) {
      dar[i] = (char)('A' + i);
    } else {
      dar[i] = SPACE;
    }
    if (session()->user()->HasRestrictionFlag(1 << i)) {
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
  string s = StringPrintf("[%s]", session()->user()->GetUserNameAndNumber(session()->usernum));
  StringJustify(&s, 37, SPACE, JustificationType::RIGHT);
  session()->localIO()->LocalXYAPrintf(wx + 1, wy - 1, 31, " %-29.29s%s ", "WWIV User Editor", s.c_str());

  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 1, 3,   "Security Level(SL): %s", sl);
  session()->localIO()->LocalXYAPrintf(wx + 36, wy + 1, 3,   "  Message AR: %s", ar);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 2, 3,   "DL Sec. Level(DSL): %s", dsl);
  session()->localIO()->LocalXYAPrintf(wx + 36, wy + 2, 3,   " Download AR: %s", dar);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 3, 3,   "   User Exemptions: %s", exempt);
  session()->localIO()->LocalXYAPrintf(wx + 36, wy + 3, 3,   "Restrictions: %s", restrict);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 5, 3,   "         Sysop Sub: %s", sysopsub);
  session()->localIO()->LocalXYAPrintf(wx + 36, wy + 5, 3,   "   Time Bank: %s", banktime);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 6, 3,   "        Ass Points: %s", ass);
  session()->localIO()->LocalXYAPrintf(wx + 36, wy + 6, 3,   " Gold Points: %s", gold);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 8, 3,   "       KB Uploaded: %s", uk);
  session()->localIO()->LocalXYAPrintf(wx + 35, wy + 8, 3,   "KB Downloaded: %s", dk);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 9, 3,   "    Files Uploaded: %s", up);
  session()->localIO()->LocalXYAPrintf(wx + 32, wy + 9, 3,   "Files Downloaded: %s", down);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 10, 3,  "   Messages Posted: %s", posts);
  session()->localIO()->LocalXYAPrintf(wx + 32, wy + 10, 3,  "Number of Logons: %s", logons);
  session()->localIO()->LocalXYAPrintf(wx + 2,  wy + 12, 3,  "Note: %s", session()->user()->GetNote());
  session()->localIO()->LocalXYAPrintf(wx + 1, wy + 14, 31,
                                          "    (ENTER) Next Field   (UP-ARROW) Previous Field    (ESC) Exit    ");
  curatr = 3;
  while (!done) {
    switch (cp) {
    case 0:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 1);
      session()->localIO()->LocalEditLine(sl, 3, NUM_ONLY, &rc, "");
      session()->user()->SetSl(atoi(sl));
      sprintf(sl, "%d", session()->user()->GetSl());
      session()->localIO()->LocalPrintf("%-3s", sl);
      break;
    case 1:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 1);
      session()->localIO()->LocalEditLine(ar, 16, SET, &rc, "ABCDEFGHIJKLMNOP ");
      session()->user()->SetAr(0);
      for (i = 0; i <= 15; i++) {
        if (ar[i] != SPACE) {
          session()->user()->SetArFlag(1 << i);
        }
      }
      break;
    case 2:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 2);
      session()->localIO()->LocalEditLine(dsl, 3, NUM_ONLY, &rc, "");
      session()->user()->SetDsl(atoi(dsl));
      sprintf(dsl, "%d", session()->user()->GetDsl());
      session()->localIO()->LocalPrintf("%-3s", dsl);
      break;
    case 3:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 2);
      session()->localIO()->LocalEditLine(dar, 16, SET, &rc, "ABCDEFGHIJKLMNOP ");
      session()->user()->SetDar(0);
      for (i = 0; i <= 15; i++) {
        if (dar[i] != SPACE) {
          session()->user()->SetDarFlag(1 << i);
        }
      }
      break;
    case 4:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 3);
      session()->localIO()->LocalEditLine(exempt, 3, NUM_ONLY, &rc, "");
      session()->user()->SetExempt(atoi(exempt));
      sprintf(exempt, "%u", session()->user()->GetExempt());
      session()->localIO()->LocalPrintf("%-3s", exempt);
      break;
    case 5:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 3);
      session()->localIO()->LocalEditLine(restrict, 16, SET, &rc, rst);
      session()->user()->SetRestriction(0);
      for (i = 0; i <= 15; i++) {
        if (restrict[i] != SPACE) {
          session()->user()->SetRestrictionFlag(1 << i);
        }
      }
      break;
    case 6:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 5);
      session()->localIO()->LocalEditLine(sysopsub, 3, NUM_ONLY, &rc, "");
      *qsc = atoi(sysopsub);
      sprintf(sysopsub, "%lu", *qsc);
      session()->localIO()->LocalPrintf("%-3s", sysopsub);
      break;
    case 7:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 5);
      session()->localIO()->LocalEditLine(banktime, 5, NUM_ONLY, &rc, "");
      session()->user()->SetTimeBankMinutes(atoi(banktime));
      sprintf(banktime, "%u", session()->user()->GetTimeBankMinutes());
      session()->localIO()->LocalPrintf("%-5s", banktime);
      break;
    case 8:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 6);
      session()->localIO()->LocalEditLine(ass, 5, NUM_ONLY, &rc, "");
      session()->user()->SetAssPoints(atoi(ass));
      sprintf(ass, "%u", session()->user()->GetAssPoints());
      session()->localIO()->LocalPrintf("%-5s", ass);
      break;
    case 9:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 6);
      session()->localIO()->LocalEditLine(gold, 5, NUM_ONLY, &rc, "");
      session()->user()->SetGold(static_cast<float>(atof(gold)));
      _gcvt(session()->user()->GetGold(), 5, gold);
      session()->localIO()->LocalPrintf("%-5s", gold);
      break;
    case 10:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 8);
      session()->localIO()->LocalEditLine(uk, 7, NUM_ONLY, &rc, "");
      session()->user()->SetUploadK(atol(uk));
      sprintf(uk, "%lu", session()->user()->GetUploadK());
      session()->localIO()->LocalPrintf("%-7s", uk);
      break;
    case 11:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 8);
      session()->localIO()->LocalEditLine(dk, 7, NUM_ONLY, &rc, "");
      session()->user()->SetDownloadK(atol(dk));
      sprintf(dk, "%lu", session()->user()->GetDownloadK());
      session()->localIO()->LocalPrintf("%-7s", dk);
      break;
    case 12:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 9);
      session()->localIO()->LocalEditLine(up, 5, NUM_ONLY, &rc, "");
      session()->user()->SetFilesUploaded(atoi(up));
      sprintf(up, "%u", session()->user()->GetFilesUploaded());
      session()->localIO()->LocalPrintf("%-5s", up);
      break;
    case 13:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 9);
      session()->localIO()->LocalEditLine(down, 5, NUM_ONLY, &rc, "");
      session()->user()->SetFilesDownloaded(atoi(down));
      sprintf(down, "%u", session()->user()->GetFilesDownloaded());
      session()->localIO()->LocalPrintf("%-5s", down);
      break;
    case 14:
      session()->localIO()->LocalGotoXY(wx + 22, wy + 10);
      session()->localIO()->LocalEditLine(posts, 5, NUM_ONLY, &rc, "");
      session()->user()->SetNumMessagesPosted(atoi(posts));
      sprintf(posts, "%u", session()->user()->GetNumMessagesPosted());
      session()->localIO()->LocalPrintf("%-5s", posts);
      break;
    case 15:
      session()->localIO()->LocalGotoXY(wx + 50, wy + 10);
      session()->localIO()->LocalEditLine(logons, 5, NUM_ONLY, &rc, "");
      session()->user()->SetNumLogons(atoi(logons));
      sprintf(logons, "%u", session()->user()->GetNumLogons());
      session()->localIO()->LocalPrintf("%-5s", logons);
      break;
    case 16: {
      char szNote[ 81 ];
      session()->localIO()->LocalGotoXY(wx + 8, wy + 12);
      strcpy(szNote, session()->user()->GetNote());
      session()->localIO()->LocalEditLine(szNote, 60, ALL, &rc, "");
      StringTrimEnd(szNote);
      session()->user()->SetNote(szNote);
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
  session()->localIO()->restorescreen();
  session()->ResetEffectiveSl();
  changedsl();
  session()->DisplaySysopWorkingIndicator(false);
#endif // !defined ( __unix__ )
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
    bputch(*iter);
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
 * @param nNumberOfChars Number of characters to move to the left
 */
void MoveLeft(int nNumberOfChars) {
  if (okansi()) {
    bout << "\x1b[" << nNumberOfChars << "D";
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
      bputch(*iter);
    }
  } else {
    bout << strText;
  }
  local_echo = oecho;
}
