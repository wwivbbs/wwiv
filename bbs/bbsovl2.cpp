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
#include "bbs/bbsovl2.h"
#include "sdk/arword.h"
#include "bbs/bbs.h"
#include "bbs/confutil.h"
#include "common/output.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "sdk/names.h"
#include <chrono>
#include <string>
#include "core/log.h"

using std::chrono::milliseconds;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::strings;


// Allows local-only editing of some of the user data in a shadowized window.
void OnlineUserEditor() {
  auto& u = *a()->user();

  bout.localIO()->savescreen();
  a()->DisplaySysopWorkingIndicator(true);
  const auto wx = 5;
  const auto wy = 3;
  bout.localIO()->MakeLocalWindow(wx, wy - 2, 70, 16 + 2);
  const auto bar = StrCat("\xC3", std::string(70 - wx + 3, '\xC4'), "\xB4");
  bout.localIO()->PutsXY(wx, wy, bar);
  bout.localIO()->PutsXY(wx, wy + 4, bar);
  bout.localIO()->PutsXY(wx, wy + 7, bar);
  bout.localIO()->PutsXY(wx, wy + 11, bar);
  bout.localIO()->PutsXY(wx, wy + 13, bar);
  if (*a()->sess().qsc > 999) {
    *a()->sess().qsc = 999;
  }
  std::string restrict;
  for (int i = 0; i <= 15; i++) {
    restrict.push_back(u.has_restrict(1 << i) ? restrict_string[i] : ' ');
  }

  // heading
  const auto s = StrCat("[", a()->user()->name_and_number(), "]");
  bout.localIO()->PutsXYA(wx + 1, wy - 1, 31, fmt::format("{:<30}{:>37}"," WWIV User Editor", s));

  bout.localIO()->PutsXYA(wx + 2, wy + 1, 3, StrCat("Security Level(SL): ", u.sl()));
  auto ar = word_to_arstr(u.ar_int(), "");
  bout.localIO()->PutsXYA(wx + 36, wy + 1, 3, StrCat("  Message AR: ", ar));
  bout.localIO()->PutsXYA(wx + 2, wy + 2, 3, StrCat("DL Sec. Level(DSL): ", u.dsl()));
  auto dar = word_to_arstr(u.dar_int(), "");
  bout.localIO()->PutsXYA(wx + 36, wy + 2, 3, StrCat(" Download AR: ", dar));
  bout.localIO()->PutsXYA(wx + 2, wy + 3, 3, StrCat("   User Exemptions: ", u.exempt()));
  bout.localIO()->PutsXYA(wx + 36, wy + 3, 3, StrCat("Restrictions: ", restrict));
  bout.localIO()->PutsXYA(wx + 2, wy + 5, 3, StrCat("         Sysop Sub: ", *a()->sess().qsc));
  bout.localIO()->PutsXYA(wx + 36, wy + 5, 3, StrCat("   Time Bank: ", u.banktime_minutes()));
  bout.localIO()->PutsXYA(wx + 2, wy + 6, 3, StrCat("        Ass Points: ", u.ass_points()));
  bout.localIO()->PutsXYA(wx + 36, wy + 6, 3, StrCat(" Gold Points: ", u.gold()));
  bout.localIO()->PutsXYA(wx + 2, wy + 8, 3, StrCat("       KB Uploaded: ", u.uk()));
  bout.localIO()->PutsXYA(wx + 35, wy + 8, 3, StrCat("KB Downloaded: ", u.dk()));
  bout.localIO()->PutsXYA(wx + 2, wy + 9, 3, StrCat("    Files Uploaded: ", u.uploaded()));
  bout.localIO()->PutsXYA(wx + 32, wy + 9, 3, StrCat("Files Downloaded: ", u.downloaded()));
  bout.localIO()->PutsXYA(wx + 2, wy + 10, 3, StrCat("   Messages Posted: ", u.messages_posted()));
  bout.localIO()->PutsXYA(wx + 32, wy + 10, 3, StrCat("Number of Logons: ", u.logons()));
  bout.localIO()->PutsXYA(wx + 2, wy + 12, 3, StrCat("Note: ", u.note()));
  bout.localIO()->PutsXYA(wx + 1, wy + 14, 31,
                          "    (ENTER) Next Field   (UP-ARROW) Previous Field    (ESC) Exit    ");
  bout.curatr(3);
  bool done = false;
  int cp = 0;
  while (!done) {
    auto rc = EditlineResult::ABORTED;
    switch (cp) {
    case 0: {
      bout.localIO()->GotoXY(wx + 22, wy + 1);
      auto sl = std::to_string(u.sl());
      rc = bout.localIO()->EditLine(sl, 3, AllowedKeys::NUM_ONLY);
      u.sl(to_number<unsigned int>(sl));
      bout.localIO()->Format("{:3}", u.sl());
    } break;
    case 1: {
      bout.localIO()->GotoXY(wx + 50, wy + 1);
      rc = bout.localIO()->EditLine(ar, 16, AllowedKeys::SET, "ABCDEFGHIJKLMNOP ");
      u.ar_int(str_to_arword(ar));
    } break;
    case 2: {
      bout.localIO()->GotoXY(wx + 22, wy + 2);
      auto dsl = std::to_string(u.dsl());
      rc = bout.localIO()->EditLine(dsl, 3, AllowedKeys::NUM_ONLY);
      u.dsl(to_number<int>(dsl));
      bout.localIO()->Format("{:3}", u.dsl());
      } break;
    case 3: {
      bout.localIO()->GotoXY(wx + 50, wy + 2);

      rc = bout.localIO()->EditLine(dar, 16, AllowedKeys::SET, "ABCDEFGHIJKLMNOP ");
      u.dar_int(str_to_arword(dar));
    } break;
    case 4: {
      bout.localIO()->GotoXY(wx + 22, wy + 3);
      auto exempt = std::to_string(u.exempt());
      rc = bout.localIO()->EditLine(exempt, 3, AllowedKeys::NUM_ONLY);
      u.exempt(to_number<int>(exempt));
      bout.localIO()->Format("{:3}", exempt);
    } break;
    case 5: {
      bout.localIO()->GotoXY(wx + 50, wy + 3);
      rc = bout.localIO()->EditLine(restrict, 16, AllowedKeys::SET, restrict_string);
      u.restriction(0);
      for (int i = 0; i <= 15; i++) {
        if (restrict[i] != SPACE) {
          u.set_restrict(1 << i);
        }
      }
    } break;
    case 6: {
      bout.localIO()->GotoXY(wx + 22, wy + 5);
      auto sysopsub = std::to_string(*a()->sess().qsc);
      rc = bout.localIO()->EditLine(sysopsub, 3, AllowedKeys::NUM_ONLY);
      *a()->sess().qsc = to_number<uint32_t>(sysopsub);
      bout.localIO()->Format("{:3}", sysopsub);
    } break;
    case 7: {
      bout.localIO()->GotoXY(wx + 50, wy + 5);
      auto banktime = std::to_string(u.banktime_minutes());
      rc = bout.localIO()->EditLine(banktime, 5, AllowedKeys::NUM_ONLY);
      u.banktime_minutes(to_number<uint16_t>(banktime));
      bout.localIO()->Format("{:5}", banktime);
    } break;
    case 8: {
      bout.localIO()->GotoXY(wx + 22, wy + 6);
      auto ass = std::to_string(u.ass_points());
      rc = bout.localIO()->EditLine(ass, 5, AllowedKeys::NUM_ONLY);
      u.ass_points(to_number<int>(ass));
      bout.localIO()->Format("{:5}", ass);
    } break;
    case 9: {
      bout.localIO()->GotoXY(wx + 50, wy + 6);
      auto gold = std::to_string(u.gold());
      rc = bout.localIO()->EditLine(gold, 5, AllowedKeys::NUM_ONLY);
      u.gold(static_cast<float>(atof(gold.c_str())));
      bout.localIO()->Format("{:5}", fmt::sprintf("%7.2f", u.gold()));
    } break;
    case 10: {
      bout.localIO()->GotoXY(wx + 22, wy + 8);
      auto uk = std::to_string(u.uk());
      rc = bout.localIO()->EditLine(uk, 7, AllowedKeys::NUM_ONLY);
      u.set_uk(to_number<uint32_t>(uk));
      bout.localIO()->Format("{:7}", uk);
    } break;
    case 11: {
      bout.localIO()->GotoXY(wx + 50, wy + 8);
      auto dk = std::to_string(u.dk());
      rc = bout.localIO()->EditLine(dk, 7, AllowedKeys::NUM_ONLY);
      u.set_dk(to_number<uint32_t>(dk));
      bout.localIO()->Format("{:7}", dk);
    } break;
    case 12: {
      bout.localIO()->GotoXY(wx + 22, wy + 9);
      auto up = std::to_string(u.uploaded());
      rc = bout.localIO()->EditLine(up, 5, AllowedKeys::NUM_ONLY);
      u.uploaded(to_number<int>(up));
      bout.localIO()->Format("{:5}", up);
    } break;
    case 13: {
      bout.localIO()->GotoXY(wx + 50, wy + 9);
      auto down = std::to_string(u.downloaded());
      rc = bout.localIO()->EditLine(down, 5, AllowedKeys::NUM_ONLY);
      u.downloaded(to_number<int>(down));
      bout.localIO()->Format("{:5}", down);
    } break;
    case 14: {
      bout.localIO()->GotoXY(wx + 22, wy + 10);
      auto posts = std::to_string(u.messages_posted());
      rc = bout.localIO()->EditLine(posts, 5, AllowedKeys::NUM_ONLY);
      u.messages_posted(to_number<int>(posts));
      bout.localIO()->Format("{:5}", posts);
    } break;
    case 15: {
      bout.localIO()->GotoXY(wx + 50, wy + 10);
      auto logons = std::to_string(u.logons());
      rc = bout.localIO()->EditLine(logons, 5, AllowedKeys::NUM_ONLY);
      u.logons(to_number<int>(logons));
      bout.localIO()->Format("{:5}",u.logons());
    } break;
    case 16: {
      bout.localIO()->GotoXY(wx + 8, wy + 12);
      auto note = u.note();
      rc = bout.localIO()->EditLine(note, 60, AllowedKeys::ALL);
      StringTrimEnd(&note);
      u.note(note);
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
  bout.localIO()->restorescreen();
  a()->reset_effective_sl();
  changedsl();
  a()->DisplaySysopWorkingIndicator(false);
}

void BackPrint(const std::string& strText, int nColorCode, int nCharDelay, int nStringDelay) {
  bout.back_puts(strText, nColorCode, milliseconds(nCharDelay), milliseconds(nStringDelay));
}

void SpinPuts(const std::string& strText, int color) {
  bout.spin_puts(strText, color);
}
