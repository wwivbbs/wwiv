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
#include "sdk/chains.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/dropfile.h"
#include "bbs/execexternal.h"
#include "bbs/instmsg.h"
#include "bbs/mmkey.h"
#include "bbs/multinst.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "common/input.h"
#include "core/cp437.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static void show_chain(const chain_t& c, bool ansi, int chain_num, bool& abort) {
  User user{};
  const auto r = c.regby;
  const auto is_regged = r.empty() ? false : a()->users()->readuser(&user, *r.begin());
  const std::string regname = (is_regged) ? user.GetName() : "Available";
  if (ansi) {
    bout.bpla(
        fmt::sprintf(" |#%d\xB3|#5%3d|#%d\xB3|#1%-41s|#%d\xB3|%2.2d%-21s|#%d\xB3|#1%5d|#%d\xB3",
                     FRAME_COLOR, chain_num, FRAME_COLOR, c.description, FRAME_COLOR,
                     (is_regged) ? 14 : 13, regname, FRAME_COLOR, c.usage, FRAME_COLOR),
        &abort);
  } else {
    bout.bpla(
        fmt::sprintf(" |%3d|%-41.41s|%-21.21s|%5d|", chain_num, c.description, regname, c.usage),
        &abort);
  }

  if (!is_regged) {
    return;
  }

  for (const auto rb : r) {
    if (rb <= 0) {
      continue;
    }
    if (!a()->users()->readuser(&user, rb)) {
      continue;
    }
    if (ansi) {
      bout.bpla(fmt::sprintf(" |#%d\xB3   \xBA%-41s\xB3|#2%-21s|#%d\xB3%5.5s\xB3", FRAME_COLOR, " ",
                             user.GetName(), FRAME_COLOR, " "),
                &abort);
    } else {
      bout.bpla(fmt::sprintf(" |   |                                         |%-21.21s|     |",
                             rb ? user.GetName() : "Available"),
                &abort);
    }
  }
}

// Displays the list of chains to a user
// Note: we aren't using a const map since [] doesn't work for const maps.
static void show_chains(int *mapp, std::map<int, int>& map) {
  bout.cls();
  bout.nl();
  auto abort = false;
  auto next = false;
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG) && a()->chains->HasRegisteredChains()) {
    bout.bpla(fmt::sprintf("|#5  Num |#1%-42.42s|#2%-22.22s|#1%-5.5s", "Description", "Sponsored by", "Usage"), &abort);

    if (okansi()) {
      bout.bpla(fmt::sprintf("|#%d %s", FRAME_COLOR,
              "\xDA\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xBF"), &abort);
    } else {
      bout.bpla(fmt::sprintf(" +---+-----------------------------------------+---------------------+-----+"), &abort);
    }
    for (auto i = 0; i < *mapp && !abort && !a()->sess().hangup(); i++) {
      const auto& c = a()->chains->at(map[i]);
      show_chain(c, okansi(), i+1, abort);
    }
    if (okansi()) {
      bout.bpla(fmt::sprintf("|#%d %s", FRAME_COLOR, "\xC0\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xD9"), &abort);
    } else {
      bout.bpla(fmt::sprintf(" +---+-----------------------------------------+---------------------+-----+"), &abort);
    }
  } else {
    bout.litebar(StrCat(a()->config()->system_name(), " Online Programs"));
    bout << "|#7\xDA\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF\r\n";
    for (auto i = 0; i < *mapp && !abort && !a()->sess().hangup(); i++) {
      bout.bputs(fmt::sprintf("|#7\xB3|#2%2d|#7\xB3 |#1%-33.33s|#7\xB3", i + 1, a()->chains->at(map[i]).description), &abort, &next);
      i++;
      if (!abort && !a()->sess().hangup()) {
        if (i >= *mapp) {
          bout.bpla(fmt::sprintf("  |#7\xB3                                  |#7\xB3"), &abort);
        } else {
          bout.bpla(fmt::sprintf("|#2%2d|#7\xB3 |#1%-33.33s|#7\xB3", i + 1,
                                 a()->chains->at(map[i]).description),
                    &abort);
        }
      }
    }
    bout << "|#7\xC0\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9\r\n";
  }
}

// Executes a "chain", index number chain_num.
void run_chain(int chain_num) {
  const auto& c = a()->chains->at(chain_num);
  auto inst = inst_ok(INST_LOC_CHAINS, chain_num + 1);
  if (inst != 0) {
    const auto message =
        fmt::format("|#2Chain {} is in use on instance {}.  ", c.description, inst);
    if (!c.multi_user) {
      bout << message << "Try again later.\r\n";
      return;
    }
    bout << message << "Care to join in? ";
    if (!bin.yesno()) {
      return;
    }
  }
  write_inst(INST_LOC_CHAINS, static_cast<uint16_t>(chain_num + 1), INST_FLAGS_NONE);
  a()->chains->increment_chain_usage(chain_num);
  a()->chains->Save();
  const auto chainCmdLine =
      stuff_in(c.filename, create_chain_file(), std::to_string(a()->modem_speed_),
               std::to_string(a()->primary_port()), std::to_string(a()->modem_speed_), "");

  sysoplog() << "!Ran \"" << c.description << "\"";
  a()->user()->SetNumChainsRun(a()->user()->GetNumChainsRun() + 1);
#ifdef _WIN32
  ScopeExit at_exit;
  if (c.local_console_cp437) {
    set_wwiv_codepage(wwiv_codepage_t::cp437);
    at_exit.swap([] { set_wwiv_codepage(wwiv_codepage_t::utf8); });
  }
#endif
  // TODO(rushfan): Maybe we need Chains::to_exec_flags(chain_t)?
  auto flags = ansir_to_flags(Chains::to_ansir(c));
  if (c.exec_mode == chain_exec_mode_t::netfoss) {
    // NetFoss requires launching from the temp dir.
    flags |= EFLAG_TEMP_DIR;
  }
  ExecuteExternalProgram(chainCmdLine, flags);
  write_inst(INST_LOC_CHAINS, 0, INST_FLAGS_NONE);
  a()->UpdateTopScreen();
}

//////////////////////////////////////////////////////////////////////////////
// Main high-level function for chain access and execution.

void do_chains() {
  bout.printfile(CHAINS_NOEXT);

  std::map<int, int> map;

  a()->tleft(true);
  auto mapp{0};
  std::set<char> odc;
  const auto& chains = a()->chains->chains();
  for (auto i = 0; i < wwiv::stl::ssize(chains); i++) {
    auto c = chains[i];
    if (c.ansi && !okansi()) {
      continue;
    }
    if (c.local_only && a()->sess().using_modem()) {
      continue;
    }
    if (!wwiv::bbs::check_acs(c.acs)) {
      continue;
    }
    map[mapp++] = i;
    if (mapp < 100) {
      if ((mapp % 10) == 0) {
        odc.insert(static_cast<char>('0' + (mapp / 10)));
      }
    }
  }
  if (mapp == 0) {
    bout << "\r\n\n|#5Sorry, no external programs available.\r\n";
    return;
  }

  auto done  = false;
  auto start  = 0;
  string ss;
  do {
    show_chains(&mapp, map);
    a()->tleft(true);
    bout.nl();
    bout << "|#5Which chain (1-" << mapp << ", Q=Quit, ?=List): ";

    if (mapp < 100) {
      ss = mmkey(odc);
    } else {
      ss = bin.input_upper(3);
    }
    const auto chain_num = to_number<int>(ss);
    if (chain_num > 0 && chain_num <= mapp) {
      bout << "\r\n|#6Please wait...\r\n";
      run_chain(map[chain_num - 1]);
    } else if (ss == "Q") {
      done = true;
    } else if (ss == "?") {
      show_chains(&mapp, map);
    } else if (ss == "P") {
      if (start > 0) {
        start -= 14;
      }
      start = std::max<int>(start, 0);
    } else if (ss == "N") {
      if (start + 14 < mapp) {
        start += 14;
      }
    }
  } while (!a()->sess().hangup()  && !done);
}

