/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/confutil.h"
#include "bbs/utility.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/names.h"
#include <chrono>
#include <string>

using std::string;
using std::chrono::milliseconds;
using namespace wwiv::os;
using namespace wwiv::strings;

#define PREV 1
#define NEXT 2
#define DONE 4
#define ABORTED 8

// Allows local-only editing of some of the user data in a shadowized window.
void OnlineUserEditor() {
  char sl[4], dsl[4], exempt[4], sysopsub[4], ar[17], dar[17], restrict[17], rst[17], uk[8], dk[8],
      up[6], down[6], posts[6], banktime[6], gold[10], ass[6], logons[6];
  int cp, i, rc = ABORTED;

  a()->localIO()->savescreen();
  a()->DisplaySysopWorkingIndicator(true);
  int wx = 5;
  int wy = 3;
  a()->localIO()->MakeLocalWindow(wx, wy - 2, 70, 16 + 2);
  const auto bar = StrCat("\xC3", std::string(70 - wx + 3, '\xC4'), "\xB4");
  a()->localIO()->PutsXY(wx, wy, bar);
  a()->localIO()->PutsXY(wx, wy + 4, bar);
  a()->localIO()->PutsXY(wx, wy + 7, bar);
  a()->localIO()->PutsXY(wx, wy + 11, bar);
  a()->localIO()->PutsXY(wx, wy + 13, bar);
  sprintf(sl, "%u", a()->user()->GetSl());
  sprintf(dsl, "%u", a()->user()->GetDsl());
  sprintf(exempt, "%u", a()->user()->GetExempt());
  if (*a()->context().qsc > 999) {
    *a()->context().qsc = 999;
  }
  sprintf(sysopsub, "%u", *a()->context().qsc);
  sprintf(uk, "%u", a()->user()->GetUploadK());
  sprintf(dk, "%u", a()->user()->GetDownloadK());
  sprintf(up, "%u", a()->user()->GetFilesUploaded());
  sprintf(down, "%u", a()->user()->GetFilesDownloaded());
  sprintf(posts, "%u", a()->user()->GetNumMessagesPosted());
  sprintf(banktime, "%u", a()->user()->GetTimeBankMinutes());
  sprintf(logons, "%u", a()->user()->GetNumLogons());
  sprintf(ass, "%u", a()->user()->GetAssPoints());

  sprintf(gold, "%7.2f", a()->user()->GetGold());
  to_char_array(rst, restrict_string);
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
  ar[16] = '\0';
  restrict[16] = '\0';
  cp = 0;
  bool done = false;

  // heading
  const auto s = StrCat("[", a()->names()->UserName(a()->usernum), "]");
  a()->localIO()->PutsXYA(wx + 1, wy - 1, 31, fmt::format("{:<30}{:>37}"," WWIV User Editor", s));

  a()->localIO()->PutsXYA(wx + 2, wy + 1, 3, StrCat("Security Level(SL): ", sl));
  a()->localIO()->PutsXYA(wx + 36, wy + 1, 3, StrCat("  Message AR: ", ar));
  a()->localIO()->PutsXYA(wx + 2, wy + 2, 3, StrCat("DL Sec. Level(DSL): ", dsl));
  a()->localIO()->PutsXYA(wx + 36, wy + 2, 3, StrCat(" Download AR: ", dar));
  a()->localIO()->PutsXYA(wx + 2, wy + 3, 3, StrCat("   User Exemptions: ", exempt));
  a()->localIO()->PutsXYA(wx + 36, wy + 3, 3, StrCat("Restrictions: ", restrict));
  a()->localIO()->PutsXYA(wx + 2, wy + 5, 3, StrCat("         Sysop Sub: ", sysopsub));
  a()->localIO()->PutsXYA(wx + 36, wy + 5, 3, StrCat("   Time Bank: ", banktime));
  a()->localIO()->PutsXYA(wx + 2, wy + 6, 3, StrCat("        Ass Points: ", ass));
  a()->localIO()->PutsXYA(wx + 36, wy + 6, 3, StrCat(" Gold Points: ", gold));
  a()->localIO()->PutsXYA(wx + 2, wy + 8, 3, StrCat("       KB Uploaded: ", uk));
  a()->localIO()->PutsXYA(wx + 35, wy + 8, 3, StrCat("KB Downloaded: ", dk));
  a()->localIO()->PutsXYA(wx + 2, wy + 9, 3, StrCat("    Files Uploaded: ", up));
  a()->localIO()->PutsXYA(wx + 32, wy + 9, 3, StrCat("Files Downloaded: ", down));
  a()->localIO()->PutsXYA(wx + 2, wy + 10, 3, StrCat("   Messages Posted: ", posts));
  a()->localIO()->PutsXYA(wx + 32, wy + 10, 3, StrCat("Number of Logons: ", logons));
  a()->localIO()->PutsXYA(wx + 2, wy + 12, 3, StrCat("Note: ", a()->user()->GetNote()));
  a()->localIO()->PutsXYA(wx + 1, wy + 14, 31,
                          "    (ENTER) Next Field   (UP-ARROW) Previous Field    (ESC) Exit    ");
  bout.curatr(3);
  while (!done) {
    switch (cp) {
    case 0:
      a()->localIO()->GotoXY(wx + 22, wy + 1);
      a()->localIO()->EditLine(sl, 3, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetSl(to_number<unsigned int>(sl));
      sprintf(sl, "%-3d", a()->user()->GetSl());
      a()->localIO()->Puts(sl);
      break;
    case 1:
      a()->localIO()->GotoXY(wx + 50, wy + 1);
      a()->localIO()->EditLine(ar, 16, AllowedKeys::SET, &rc, "ABCDEFGHIJKLMNOP ");
      a()->user()->SetAr(0);
      for (i = 0; i <= 15; i++) {
        if (ar[i] != SPACE) {
          a()->user()->SetArFlag(1 << i);
        }
      }
      break;
    case 2:
      a()->localIO()->GotoXY(wx + 22, wy + 2);
      a()->localIO()->EditLine(dsl, 3, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetDsl(to_number<int>(dsl));
      sprintf(dsl, "%-3d", a()->user()->GetDsl());
      a()->localIO()->Puts(dsl);
      break;
    case 3:
      a()->localIO()->GotoXY(wx + 50, wy + 2);
      a()->localIO()->EditLine(dar, 16, AllowedKeys::SET, &rc, "ABCDEFGHIJKLMNOP ");
      a()->user()->SetDar(0);
      for (i = 0; i <= 15; i++) {
        if (dar[i] != SPACE) {
          a()->user()->SetDarFlag(1 << i);
        }
      }
      break;
    case 4:
      a()->localIO()->GotoXY(wx + 22, wy + 3);
      a()->localIO()->EditLine(exempt, 3, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetExempt(to_number<int>(exempt));
      sprintf(exempt, "%-3u", a()->user()->GetExempt());
      a()->localIO()->Puts(exempt);
      break;
    case 5:
      a()->localIO()->GotoXY(wx + 50, wy + 3);
      a()->localIO()->EditLine(restrict, 16, AllowedKeys::SET, &rc, rst);
      a()->user()->SetRestriction(0);
      for (i = 0; i <= 15; i++) {
        if (restrict[i] != SPACE) {
          a()->user()->SetRestrictionFlag(1 << i);
        }
      }
      break;
    case 6:
      a()->localIO()->GotoXY(wx + 22, wy + 5);
      a()->localIO()->EditLine(sysopsub, 3, AllowedKeys::NUM_ONLY, &rc, "");
      *a()->context().qsc = to_number<uint32_t>(sysopsub);
      sprintf(sysopsub, "%-3u", *a()->context().qsc);
      a()->localIO()->Puts(sysopsub);
      break;
    case 7:
      a()->localIO()->GotoXY(wx + 50, wy + 5);
      a()->localIO()->EditLine(banktime, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetTimeBankMinutes(to_number<uint16_t>(banktime));
      sprintf(banktime, "%-5u", a()->user()->GetTimeBankMinutes());
      a()->localIO()->Puts(banktime);
      break;
    case 8:
      a()->localIO()->GotoXY(wx + 22, wy + 6);
      a()->localIO()->EditLine(ass, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetAssPoints(to_number<int>(ass));
      sprintf(ass, "%-5u", a()->user()->GetAssPoints());
      a()->localIO()->Puts(ass);
      break;
    case 9:
      a()->localIO()->GotoXY(wx + 50, wy + 6);
      a()->localIO()->EditLine(gold, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetGold(static_cast<float>(atof(gold)));
      sprintf(gold, "%7.2f", a()->user()->GetGold());
      a()->localIO()->Puts(fmt::sprintf("%-5s", gold));
      break;
    case 10:
      a()->localIO()->GotoXY(wx + 22, wy + 8);
      a()->localIO()->EditLine(uk, 7, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetUploadK(to_number<uint32_t>(uk));
      sprintf(uk, "%-7u", a()->user()->GetUploadK());
      a()->localIO()->Puts(uk);
      break;
    case 11:
      a()->localIO()->GotoXY(wx + 50, wy + 8);
      a()->localIO()->EditLine(dk, 7, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetDownloadK(to_number<uint32_t>(dk));
      sprintf(dk, "%-7u", a()->user()->GetDownloadK());
      a()->localIO()->Puts(dk);
      break;
    case 12:
      a()->localIO()->GotoXY(wx + 22, wy + 9);
      a()->localIO()->EditLine(up, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetFilesUploaded(to_number<int>(up));
      sprintf(up, "%-5u", a()->user()->GetFilesUploaded());
      a()->localIO()->Puts(up);
      break;
    case 13:
      a()->localIO()->GotoXY(wx + 50, wy + 9);
      a()->localIO()->EditLine(down, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetFilesDownloaded(to_number<int>(down));
      sprintf(down, "%-5u", a()->user()->GetFilesDownloaded());
      a()->localIO()->Puts(down);
      break;
    case 14:
      a()->localIO()->GotoXY(wx + 22, wy + 10);
      a()->localIO()->EditLine(posts, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetNumMessagesPosted(to_number<int>(posts));
      sprintf(posts, "%-5u", a()->user()->GetNumMessagesPosted());
      a()->localIO()->Puts(posts);
      break;
    case 15:
      a()->localIO()->GotoXY(wx + 50, wy + 10);
      a()->localIO()->EditLine(logons, 5, AllowedKeys::NUM_ONLY, &rc, "");
      a()->user()->SetNumLogons(to_number<int>(logons));
      sprintf(logons, "%-5u", a()->user()->GetNumLogons());
      a()->localIO()->Puts(logons);
      break;
    case 16: {
      char szNote[81];
      a()->localIO()->GotoXY(wx + 8, wy + 12);
      to_char_array(szNote, a()->user()->GetNote());
      a()->localIO()->EditLine(szNote, 60, AllowedKeys::ALL, &rc, "");
      StringTrimEnd(szNote);
      a()->user()->SetNote(szNote);
    } break;
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
  a()->reset_effective_sl();
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
  bout.Color(nColorCode);
  sleep_for(milliseconds(nCharDelay));
  for (auto iter = strText.cbegin(); iter != strText.cend() && !a()->hangup_; ++iter) {
    bout.bputch(*iter);
    sleep_for(milliseconds(nCharDelay));
  }

  sleep_for(milliseconds(nStringDelay));
  for (auto iter = strText.cbegin(); iter != strText.cend() && !a()->hangup_; ++iter) {
    bout.bs();
    sleep_for(milliseconds(5));
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
  if (okansi()) {
    bout.Color(nColorCode);
    const int dly = 30;
    for (auto iter = strText.cbegin(); iter != strText.cend() && !a()->hangup_; ++iter) {
      sleep_for(milliseconds(dly));
      bout << "/";
      bout.Left(1);
      sleep_for(milliseconds(dly));
      bout << "-";
      bout.Left(1);
      sleep_for(milliseconds(dly));
      bout << "\\";
      bout.Left(1);
      sleep_for(milliseconds(dly));
      bout << "|";
      bout.Left(1);
      sleep_for(milliseconds(dly));
      bout.bputch(*iter);
    }
  } else {
    bout << strText;
  }
}
