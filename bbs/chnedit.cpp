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

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "common/com.h"
#include "bbs/finduser.h"
#include "common/input.h"
#include "common/pause.h"
#include "bbs/utility.h"
#include "core/datafile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/chains.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "arword.h"

using std::string;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void insert_chain(int chain_num);
void delete_chain(int chain_num);

static string chaindata(int chain_num) {
  const auto& c = a()->chains->at(chain_num);
  char chAr = SPACE;

  if (c.ar != 0) {
    for (auto i = 0; i < 16; i++) {
      if ((1 << i) & c.ar) {
        chAr = static_cast<char>('A' + i);
      }
    }
  }
  const auto ansi_req = c.ansi ? 'Y' : 'N';
  return fmt::sprintf("|#2%2d |#1%-28.28s  |#2%-30.30s |#9%-3d    %1c  %1c", chain_num,
                      stripcolors(c.description), c.filename, c.sl, ansi_req, chAr);
}

static void showchains() {
  bout.cls();
  bool abort = false;
  bout.bpla("|#2NN Description                   Path Name                      SL  ANSI AR",
            &abort);
  bout.bpla("|#7== ----------------------------  ============================== --- ==== --",
            &abort);
  for (size_t nChainNum = 0; nChainNum < a()->chains->chains().size() && !abort; nChainNum++) {
    const auto s = chaindata(nChainNum);
    bout.bpla(s, &abort);
  }
}

void ShowChainCommandLineHelp() {
  bout << "|#2  Macro   Value\r\n";
  bout << "|#7 ======== =======================================\r\n";
  bout << "|#1   %     |#9 A single \'%\' Character\r\n";
  bout << "|#1   %1    |#9 CHAIN.TXT full pathname (legacy parameter)\r\n";
  bout << "|#1   %A    |#9 CALLINFO.BBS full pathname \r\n";
  bout << "|#1   %C    |#9 CHAIN.TXT full pathname \r\n";
  bout << "|#1   %D    |#9 DORIFOx.DEF full pathname \r\n";
  bout << "|#1   %E    |#9 DOOR32.SYS full pathname \r\n";
  bout << "|#1   %H    |#9 Socket Handle \r\n";
  bout << "|#1   %I    |#9 TEMP directory for the instance \r\n";
  bout << "|#1   %K    |#9 GFiles Comment File For Archives\r\n";
  bout << "|#1   %M    |#9 Modem Baud Rate\r\n";
  bout << "|#1   %N    |#9 Node (Instance) number\r\n";
  bout << "|#1   %O    |#9 PCBOARD.SYS full pathname\r\n";
  bout << "|#1   %P    |#9 ComPort Number\r\n";
  bout << "|#1   %R    |#9 DOOR.SYS Full Pathname\r\n";
  bout << "|#1   %S    |#9 Com Port Baud Rate\r\n";
  bout << "|#1   %T    |#9 Minutes Remaining\r\n";
  bout << "|#1   %U    |#9 Users Handle (primary name)\r\n";
  bout.nl();
}

static std::string YesNoStringList(bool b, const std::string& yes, const std::string& no) {
  return (b) ? yes : no;
}

static void modify_chain_sponsors(int chain_num, chain_t& c) {
  if (!a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    return;
  }
  bool done = false;
  do {
    bout.cls();
    bout.litebar(fmt::format("Editing Chain #{}", chain_num));
    const auto& r = c.regby;
    User regUser;
    if (!r.empty()) {
      auto it = r.begin();
      auto first = *r.begin();
      a()->users()->readuser(&regUser, *it++);
      bout << "|#9L) Registered by: |#2" << a()->names()->UserName(first) << wwiv::endl;
      for (; it != std::end(r); it++) {
        const auto rbc = *it;
        if (rbc != 0) {
          if (a()->users()->readuser(&regUser, rbc)) {
            bout << string(18, ' ') << "|#2" << a()->names()->UserName(rbc) << wwiv::endl;
          }
        }
      }
    } else {
      bout << "|#9L) Registered by: |#2AVAILABLE" << wwiv::endl;
    }
    bout.nl();
    bout << "|#9(A)dd, (R)emove, (Q)uit: Which (A,R,Q) ? ";
    auto ch = onek("QARQ", true); 
    switch (ch) {
    case 'Q':
      return;
    case 'A': {
      if (c.regby.size() > 5) {
        bout << "|#6Only 5 sponsors allowed." << wwiv::endl;
        bout.pausescr();
        break;
      }
      auto nn = input(30, true);
      auto user_number = finduser1(nn);
      if (user_number > 0) {
        c.regby.insert(static_cast<int16_t>(user_number));
      }
    } break;
    case 'R': {
      auto nn = input(30, true);
      auto user_number = finduser1(nn);
      if (user_number > 0) {
        c.regby.erase(static_cast<int16_t>(user_number));
      }
    } break;
    }
  } while (!done && !a()->context().hangup());
}

static std::string chain_exec_mode_to_string(const chain_exec_mode_t& t) {
  std::vector<string> names{"Normal", "Emulate DOS Interrupts", "Emulate DOS FOSSIL", "STDIO"};
  try {
    return names.at(static_cast<size_t>(t));
  } catch (std::out_of_range&) {
    return names.at(0);
  }
}

static void modify_chain(int chain_num) {
  auto c = a()->chains->at(chain_num);
  bool done = false;
  do {
    bout.cls();
    bout.litebar(fmt::format("Editing Chain #{}", chain_num));

    bout << "|#9A) Description  : |#2" << c.description << wwiv::endl;
    bout << "|#9B) Filename     : |#2" << c.filename << wwiv::endl;
    bout << "|#9C) SL           : |#2" << static_cast<int>(c.sl) << wwiv::endl;
    bout << "|#9D) AR           : |#2" << word_to_arstr(c.ar, "None.") << wwiv::endl;
    bout << "|#9E) ANSI         : |#2" << (c.ansi ? "|#6Required" : "|#1Optional") << wwiv::endl;
    bout << "|#9F) Exec Mode:     |#2" << chain_exec_mode_to_string(c.exec_mode) << wwiv::endl;
    bout << "|#9I) Launch From  : |#2"
         << YesNoStringList(c.dir == chain_exec_dir_t::temp, "Temp/Node Directory",
                            "BBS Root Directory")
         << wwiv::endl;
    bout << "|#9J) Local only   : |#2" << YesNoString(c.local_only) << wwiv::endl;
    bout << "|#9K) Multi user   : |#2" << YesNoString(c.multi_user) << wwiv::endl;
    char ch{};
    if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
      const auto& r = c.regby;
      User regUser;
      if (!r.empty()) {
        auto it = r.begin();
        const auto first = *it;
        a()->users()->readuser(&regUser, *it++);
        bout << "|#9L) Registered by: |#2" << a()->names()->UserName(first) << wwiv::endl;
        for (; it != std::end(r); it++) {
          const auto rbc = *it;
          if (rbc != 0) {
            if (a()->users()->readuser(&regUser, rbc)) {
              bout << string(18, ' ') << "|#2" << a()->names()->UserName(rbc) << wwiv::endl;
            }
          }
        }
      } else {
        bout << "|#9L) Registered by: |#2AVAILABLE" << wwiv::endl;
      }
      bout << "|#9M) Usage        : |#2" << c.usage << wwiv::endl;
      if (c.maxage == 0 && c.minage == 0) {
        c.maxage = 255;
      }
      bout << "|#9N) Age limit    : |#2" << static_cast<int>(c.minage) << " - "
           << static_cast<int>(c.maxage) << wwiv::endl;
      bout.nl();
      bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1N|#7,|#1R|#7,|#1[|#7,|#1]|#7) : ";
      ch = onek("QABCDEFGHIJKLMN[]", true); // removed i
    } else {
      bout.nl();
      bout << "|#9Which (A-K,R,[,],Q) ? ";
      ch = onek("QABCDEFGHIJK[]", true); // removed i
    }
    switch (ch) {
    case 'Q': {
      done = true;
    } break;
    case '[':
      a()->chains->at(chain_num) = c;
      if (--chain_num < 0) {
        chain_num = ssize(a()->chains->chains()) - 1;
      }
      c = a()->chains->at(chain_num);
      break;
    case ']':
      a()->chains->at(chain_num) = c;
      if (++chain_num >= ssize(a()->chains->chains())) {
        chain_num = 0;
      }
      c = a()->chains->at(chain_num);
      break;
    case 'A': {
      bout.nl();
      bout << "|#7New Description? ";
      auto descr = bin.input_text(c.description, 40);
      if (!descr.empty()) {
        c.description = descr;
      }
    } break;
    case 'B': {
      bout.cls();
      ShowChainCommandLineHelp();
      bout << "\r\n|#9Enter Command Line.\r\n|#7:";
      c.filename = bin.input_cmdline(c.filename, 79);
    } break;
    case 'C': {
      bout.nl();
      bout << "|#7New SL? ";
      c.sl = input_number(c.sl);
    } break;
    case 'D': {
      bout.nl();
      bout << "|#7New AR (<SPC>=None) ? ";
      auto ch2 = onek(" ABCDEFGHIJKLMNOP");
      if (ch2 == SPACE) {
        c.ar = 0;
      } else {
        c.ar = static_cast<uint16_t>(1 << (ch2 - 'A'));
      }
    } break;
    case 'E':
      c.ansi = !c.ansi;
      break;
    case 'F':
      c.exec_mode++;
#ifdef _WIN32
      if (c.exec_mode == chain_exec_mode_t::stdio) {
        c.exec_mode++;
      }
#else
      if (c.exec_mode == chain_exec_mode_t::dos) {
        c.exec_mode++;
      }
      if (c.exec_mode == chain_exec_mode_t::fossil) {
        c.exec_mode++;
      }
#endif
      break;
    case 'I':
      c.dir++;
      break;
    case 'J':
      c.local_only = !c.local_only;
      break;
    case 'K':
      c.multi_user = !c.multi_user;
      break;
    case 'L':
      if (!a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
        break;
      }
      modify_chain_sponsors(chain_num, c);
      break;
    case 'M': {
      bout.nl();
      bout << "|#5Times Run : ";
      c.usage = input_number(c.usage);
    } break;
    case 'N':
      bout.nl();
      bout << "|#5New minimum age? ";
      c.minage = input_number(c.minage);
      if (c.minage > 0) {
        bout << "|#5New maximum age? ";
        auto maxage = input_number(c.maxage);
        if (maxage < c.minage) {
          break;
        }
        c.maxage = maxage;
      }
      break;
    }
  } while (!done && !a()->context().hangup());
  a()->chains->at(chain_num) = c;
}

void insert_chain(int pos) {
  chain_t c{};
  c.description = "** NEW CHAIN **";
  c.filename = "REM";
  c.sl = 10;
  c.ar = 0;
  c.exec_mode = chain_exec_mode_t::none;
  c.maxage = 255;
  a()->chains->insert(pos, c);
  modify_chain(pos);
}

void delete_chain(int chain_num) {
  a()->chains->erase(chain_num);
}

void chainedit() {
  if (!ValidateSysopPassword()) {
    return;
  }
  showchains();
  bool done = false;
  do {
    bout.nl();
    bout << "|#7Chains: (D)elete, (I)nsert, (M)odify, (Q)uit, ? : ";
    char ch = onek("QDIM?");
    switch (ch) {
    case '?':
      showchains();
      break;
    case 'Q':
      done = true;
      break;
    case 'M': {
      bout.nl();
      bout << "|#2(Q=Quit) Chain number? ";
      auto r = input_number_hotkey(0, {'Q'}, 0, ssize(a()->chains->chains()), false);
      if (r.key != 'Q' && r.num < ssize(a()->chains->chains())) {
        modify_chain(r.num);
      }
    } break;
    case 'I': {
      if (a()->chains->chains().size() < a()->max_chains) {
        bout.nl();
        bout << "|#2(Q=Quit) Insert before which chain ('$' for end) : ";
        auto r = input_number_hotkey(0, {'$', 'Q'}, 0, ssize(a()->chains->chains()), false);
        if (r.key == 'Q') {
          break;
        }
        auto chain = (r.key == '$') ? ssize(a()->chains->chains()) : r.num;
        if (chain >= 0 && chain <= ssize(a()->chains->chains())) {
          insert_chain(chain);
        }
      }
    } break;
    case 'D': {
      bout.nl();
      bout << "|#2(Q=Quit) Delete which chain? ";
      auto r = input_number_hotkey(0, {'$', 'Q'}, 0, ssize(a()->chains->chains()), false);
      if (r.key == 'Q') {
        break;
      }
      if (r.num >= 0 && r.num < ssize(a()->chains->chains())) {
        bout.nl();
        bout << "|#5Delete " << a()->chains->at(r.num).description << "? ";
        if (bin.yesno()) {
          delete_chain(r.num);
        }
      }
    } break;
    }
  } while (!done && !a()->context().hangup());

  a()->chains->Save();
}
