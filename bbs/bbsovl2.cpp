/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/arword.h"
#include "bbs/bbs.h"
#include "bbs/confutil.h"
#include "bbs/utility.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "sdk/names.h"
#include <chrono>
#include <string>
#include "core/log.h"

using std::string;
using std::chrono::milliseconds;
using namespace wwiv::os;
using namespace wwiv::strings;


// Allows local-only editing of some of the user data in a shadowized window.
void OnlineUserEditor() {
  auto& u = *a()->user();

  a()->localIO()->savescreen();
  a()->DisplaySysopWorkingIndicator(true);
  const auto wx = 5;
  const auto wy = 3;
  a()->localIO()->MakeLocalWindow(wx, wy - 2, 70, 16 + 2);
  const auto bar = StrCat("\xC3", std::string(70 - wx + 3, '\xC4'), "\xB4");
  a()->localIO()->PutsXY(wx, wy, bar);
  a()->localIO()->PutsXY(wx, wy + 4, bar);
  a()->localIO()->PutsXY(wx, wy + 7, bar);
  a()->localIO()->PutsXY(wx, wy + 11, bar);
  a()->localIO()->PutsXY(wx, wy + 13, bar);
  if (*a()->context().qsc > 999) {
    *a()->context().qsc = 999;
  }
  std::string restrict;
  for (int i = 0; i <= 15; i++) {
    restrict.push_back(u.HasRestrictionFlag(1 << i) ? restrict_string[i] : ' ');
  }

  // heading
  const auto s = StrCat("[", a()->names()->UserName(a()->usernum), "]");
  a()->localIO()->PutsXYA(wx + 1, wy - 1, 31, fmt::format("{:<30}{:>37}"," WWIV User Editor", s));

  a()->localIO()->PutsXYA(wx + 2, wy + 1, 3, StrCat("Security Level(SL): ", u.GetSl()));
  auto ar = word_to_arstr(u.GetAr(), "");
  a()->localIO()->PutsXYA(wx + 36, wy + 1, 3, StrCat("  Message AR: ", ar));
  a()->localIO()->PutsXYA(wx + 2, wy + 2, 3, StrCat("DL Sec. Level(DSL): ", u.GetDsl()));
  auto dar = word_to_arstr(u.GetDar(), "");
  a()->localIO()->PutsXYA(wx + 36, wy + 2, 3, StrCat(" Download AR: ", dar));
  a()->localIO()->PutsXYA(wx + 2, wy + 3, 3, StrCat("   User Exemptions: ", u.GetExempt()));
  a()->localIO()->PutsXYA(wx + 36, wy + 3, 3, StrCat("Restrictions: ", restrict));
  a()->localIO()->PutsXYA(wx + 2, wy + 5, 3, StrCat("         Sysop Sub: ", *a()->context().qsc));
  a()->localIO()->PutsXYA(wx + 36, wy + 5, 3, StrCat("   Time Bank: ", u.GetTimeBankMinutes()));
  a()->localIO()->PutsXYA(wx + 2, wy + 6, 3, StrCat("        Ass Points: ", u.GetAssPoints()));
  a()->localIO()->PutsXYA(wx + 36, wy + 6, 3, StrCat(" Gold Points: ", u.GetGold()));
  a()->localIO()->PutsXYA(wx + 2, wy + 8, 3, StrCat("       KB Uploaded: ", u.uk()));
  a()->localIO()->PutsXYA(wx + 35, wy + 8, 3, StrCat("KB Downloaded: ", u.dk()));
  a()->localIO()->PutsXYA(wx + 2, wy + 9, 3, StrCat("    Files Uploaded: ", u.GetFilesUploaded()));
  a()->localIO()->PutsXYA(wx + 32, wy + 9, 3, StrCat("Files Downloaded: ", u.GetFilesDownloaded()));
  a()->localIO()->PutsXYA(wx + 2, wy + 10, 3, StrCat("   Messages Posted: ", u.GetNumMessagesPosted()));
  a()->localIO()->PutsXYA(wx + 32, wy + 10, 3, StrCat("Number of Logons: ", u.GetNumLogons()));
  a()->localIO()->PutsXYA(wx + 2, wy + 12, 3, StrCat("Note: ", u.GetNote()));
  a()->localIO()->PutsXYA(wx + 1, wy + 14, 31,
                          "    (ENTER) Next Field   (UP-ARROW) Previous Field    (ESC) Exit    ");
  bout.curatr(3);
  bool done = false;
  int cp = 0;
  while (!done) {
    auto rc = EditlineResult::ABORTED;
    switch (cp) {
    case 0: {
      a()->localIO()->GotoXY(wx + 22, wy + 1);
      auto sl = std::to_string(u.GetSl());
      rc = a()->localIO()->EditLine(sl, 3, AllowedKeys::NUM_ONLY);
      u.SetSl(to_number<unsigned int>(sl));
      a()->localIO()->Format("{:3}", u.GetSl());
    } break;
    case 1: {
      a()->localIO()->GotoXY(wx + 50, wy + 1);
      rc = a()->localIO()->EditLine(ar, 16, AllowedKeys::SET, "ABCDEFGHIJKLMNOP ");
      u.SetAr(str_to_arword(ar));
    } break;
    case 2: {
      a()->localIO()->GotoXY(wx + 22, wy + 2);
      auto dsl = std::to_string(u.GetDsl());
      rc = a()->localIO()->EditLine(dsl, 3, AllowedKeys::NUM_ONLY);
      u.SetDsl(to_number<int>(dsl));
      a()->localIO()->Format("{:3}", u.GetDsl());
      } break;
    case 3: {
      a()->localIO()->GotoXY(wx + 50, wy + 2);

      rc = a()->localIO()->EditLine(dar, 16, AllowedKeys::SET, "ABCDEFGHIJKLMNOP ");
      u.SetDar(str_to_arword(dar));
    } break;
    case 4: {
      a()->localIO()->GotoXY(wx + 22, wy + 3);
      auto exempt = std::to_string(u.GetExempt());
      rc = a()->localIO()->EditLine(exempt, 3, AllowedKeys::NUM_ONLY);
      u.SetExempt(to_number<int>(exempt));
      a()->localIO()->Format("{:3}", exempt);
    } break;
    case 5: {
      a()->localIO()->GotoXY(wx + 50, wy + 3);
      rc = a()->localIO()->EditLine(restrict, 16, AllowedKeys::SET, restrict_string);
      u.SetRestriction(0);
      for (int i = 0; i <= 15; i++) {
        if (restrict[i] != SPACE) {
          u.SetRestrictionFlag(1 << i);
        }
      }
    } break;
    case 6: {
      a()->localIO()->GotoXY(wx + 22, wy + 5);
      auto sysopsub = std::to_string(*a()->context().qsc);
      rc = a()->localIO()->EditLine(sysopsub, 3, AllowedKeys::NUM_ONLY);
      *a()->context().qsc = to_number<uint32_t>(sysopsub);
      a()->localIO()->Format("{:3}", sysopsub);
    } break;
    case 7: {
      a()->localIO()->GotoXY(wx + 50, wy + 5);
      auto banktime = std::to_string(u.GetTimeBankMinutes());
      rc = a()->localIO()->EditLine(banktime, 5, AllowedKeys::NUM_ONLY);
      u.SetTimeBankMinutes(to_number<uint16_t>(banktime));
      a()->localIO()->Format("{:5}", banktime);
    } break;
    case 8: {
      a()->localIO()->GotoXY(wx + 22, wy + 6);
      auto ass = std::to_string(u.GetAssPoints());
      rc = a()->localIO()->EditLine(ass, 5, AllowedKeys::NUM_ONLY);
      u.SetAssPoints(to_number<int>(ass));
      a()->localIO()->Format("{:5}", ass);
    } break;
    case 9: {
      a()->localIO()->GotoXY(wx + 50, wy + 6);
      auto gold = std::to_string(u.GetGold());
      rc = a()->localIO()->EditLine(gold, 5, AllowedKeys::NUM_ONLY);
      u.SetGold(static_cast<float>(atof(gold.c_str())));
      a()->localIO()->Format("{:5}", fmt::sprintf("%7.2f", u.GetGold()));
    } break;
    case 10: {
      a()->localIO()->GotoXY(wx + 22, wy + 8);
      auto uk = std::to_string(u.uk());
      rc = a()->localIO()->EditLine(uk, 7, AllowedKeys::NUM_ONLY);
      u.set_uk(to_number<uint32_t>(uk));
      a()->localIO()->Format("{:7}", uk);
    } break;
    case 11: {
      a()->localIO()->GotoXY(wx + 50, wy + 8);
      auto dk = std::to_string(u.dk());
      rc = a()->localIO()->EditLine(dk, 7, AllowedKeys::NUM_ONLY);
      u.set_dk(to_number<uint32_t>(dk));
      a()->localIO()->Format("{:7}", dk);
    } break;
    case 12: {
      a()->localIO()->GotoXY(wx + 22, wy + 9);
      auto up = std::to_string(u.GetFilesUploaded());
      rc = a()->localIO()->EditLine(up, 5, AllowedKeys::NUM_ONLY);
      u.SetFilesUploaded(to_number<int>(up));
      a()->localIO()->Format("{:5}", up);
    } break;
    case 13: {
      a()->localIO()->GotoXY(wx + 50, wy + 9);
      auto down = std::to_string(u.GetFilesDownloaded());
      rc = a()->localIO()->EditLine(down, 5, AllowedKeys::NUM_ONLY);
      u.SetFilesDownloaded(to_number<int>(down));
      a()->localIO()->Format("{:5}", down);
    } break;
    case 14: {
      a()->localIO()->GotoXY(wx + 22, wy + 10);
      auto posts = std::to_string(u.GetNumMessagesPosted());
      rc = a()->localIO()->EditLine(posts, 5, AllowedKeys::NUM_ONLY);
      u.SetNumMessagesPosted(to_number<int>(posts));
      a()->localIO()->Format("{:5}", posts);
    } break;
    case 15: {
      a()->localIO()->GotoXY(wx + 50, wy + 10);
      auto logons = std::to_string(u.GetNumLogons());
      rc = a()->localIO()->EditLine(logons, 5, AllowedKeys::NUM_ONLY);
      u.SetNumLogons(to_number<int>(logons));
      a()->localIO()->Format("{:5}",u.GetNumLogons());
    } break;
    case 16: {
      a()->localIO()->GotoXY(wx + 8, wy + 12);
      auto note = u.GetNote();
      rc = a()->localIO()->EditLine(note, 60, AllowedKeys::ALL);
      StringTrimEnd(&note);
      u.SetNote(note);
    } break;
    default: {
      LOG(ERROR) << "Unknown case: " << cp;
      rc = EditlineResult::ABORTED;
    } break;
    }
    switch (rc) {
    case EditlineResult::ABORTED:
      done = true;
      break;
    case EditlineResult::DONE:
      done = true;
      break;
    case EditlineResult::NEXT:
      cp = (cp + 1) % 17;
      break;
    case EditlineResult::PREV:
      cp--;
      if (cp < 0) {
        cp = 16;
      }
      break;
    default:
      done = true;
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
