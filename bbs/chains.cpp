/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/bbsutl.h"
#include "bbs/chnedit.h"
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

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static void show_chain(const chain_t& c, bool ansi, int chain_num, bool& abort) {
  User user{};
  const auto r = c.regby;
  const auto is_regged = r.empty() ? false : a()->users()->readuser(&user, *r.begin());
  const std::string regname = is_regged ? user.name() : "Available";
  if (ansi) {
    bout.bpla(
        fmt::format(" |#{:d}\xB3|#5{:3d}|#{:d}\xB3|#1{:>41}|#{:d}\xB3|{:2d}{:21}|#{:d}\xB3|#1{:5d}|#{:d}\xB3",
                     FRAME_COLOR, chain_num, FRAME_COLOR, c.description, FRAME_COLOR,
                     (is_regged) ? 14 : 13, regname, FRAME_COLOR, c.usage, FRAME_COLOR),
        &abort);
  } else {
    bout.bpla(
        fmt::format(" |{:3d}|{:41}|{:21}|{:5d}|", chain_num, c.description, regname, c.usage),
        &abort);
  }

  if (!is_regged) {
    return;
  }

  for (const auto rb : r) {
    if (rb <= 1) {
      continue;
    }
    if (r.size()==1) {
      continue;
    }
    if (!a()->users()->readuser(&user, rb)) {
      continue;
    }
    if (ansi) {
      bout.bpla(fmt::format(" |#{:d}\xB3   \xBA{:41}\xB3|#2{:21}|#{:d}\xB3{:5}\xB3", FRAME_COLOR, " ",
                             user.name(), FRAME_COLOR, " "),
                &abort);
    } else {
      bout.bpla(fmt::format(" |   |                                         |{:21}|     |",
                             rb ? user.name() : "Available"),
                &abort);
    }
  }
}

// Displays the list of chains to a user
// Note: we aren't using a const map since [] doesn't work for const maps.
static void show_chains(int *mapp, std::map<int, int>& map) {
  bout.cls();
  bout.printfile(CHAINS_NOEXT);
  auto abort = false;
  auto next = false;
  if (a()->chains->HasRegisteredChains()) {
    bout.bpla(fmt::format("|#5  Num |#1{:42}|#2{:22}|#1{:5}", "Description", "Sponsored by", "Usage"), &abort);

    if (okansi()) {
      bout.bpla(fmt::format("|#{:d} {}", FRAME_COLOR,
              "\xDA\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xBF"), &abort);
    } else {
      bout.bpla(" +---+-----------------------------------------+---------------------+-----+", &abort);
    }
    for (auto i = 0; i < *mapp && !abort && !a()->sess().hangup(); i++) {
      const auto& c = a()->chains->at(map[i]);
      show_chain(c, okansi(), i+1, abort);
    }
    if (okansi()) {
      bout.bpla(fmt::format("|#{:d} {}", FRAME_COLOR, "\xC0\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xD9"), &abort);
    } else {
      bout.bpla(" +---+-----------------------------------------+---------------------+-----+", &abort);
    }
  } else {
    bout.litebar(StrCat(a()->config()->system_name(), " Online Programs"));
    if (a()->config()->toggles().show_chain_usage) {
      bout.bpla(fmt::format(" |#5Num |#1{:28}|#1{:5} |#5Num |#1{:28}|#1{:5}", "Description", "Usage", "Description", "Usage"), &abort);
      if (okansi()) {
      bout.outstr("|#7\xDA\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xBF\r\n");
      } else {
	bout.bpla("+--+----------------------------+-----+--+----------------------------+-----+", &abort);
      }
    for (auto i = 0; i < *mapp && !abort && !a()->sess().hangup(); i++) {
      const auto bar = fmt::format("|#{:d}{}", FRAME_COLOR, okansi() ? "\xB3": "|") ; 
      bout.outstr(fmt::format("{}|#2{:2d}{} |#1{:27}{}|#1{:5d}{}",
			       bar, i + 1,
			       bar, a()->chains->at(map[i]).description,
			       bar, a()->chains->at(map[i]).usage, bar)
		  , &abort, &next);
      i++;
      if (!abort && !a()->sess().hangup()) {
        if (i >= *mapp) {
	  bout.bpla(fmt::format("  {}                            {}     {}",bar,bar,bar), &abort);
        } else {
	  bout.bpla(fmt::format("|#2{:2d}{} |#1{:27}{}|#1{:5d}{}",
				 i + 1, bar,
				 a()->chains->at(map[i]).description, bar,
				 a()->chains->at(map[i]).usage, bar), &abort);
	}
      }
    }
      if (okansi()) {
    bout.outstr("|#7\xC0\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xD9\r\n");
      } else {
	bout.bpla("+--+----------------------------+-----+--+----------------------------+-----+", &abort);
      }
    } else {
      if (okansi()) {
	bout.outstr("|#7\xDA\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF\r\n");
      } else {
	bout.bpla("+--+----------------------------------+--+----------------------------------+", &abort);
      }
    const auto bar = fmt::format("|#{:d}{}", FRAME_COLOR, okansi() ? "\xB3": "|") ; 
    for (auto i = 0; i < *mapp && !abort && !a()->sess().hangup(); i++) {
      bout.outstr(fmt::format("{}|#2{:2d}{} |#1{:33}{}", bar, i + 1, bar, a()->chains->at(map[i]).description, bar), &abort, &next);
      i++;
      if (!abort && !a()->sess().hangup()) {
        if (i >= *mapp) {
          bout.bpla(fmt::format("  {}                                  {}",bar,bar), &abort);
        } else {
          bout.bpla(fmt::format("|#2{:2d}{} |#1{:33}{}", i + 1, bar,
                                 a()->chains->at(map[i]).description, bar),
                    &abort);
        }
      }
    }
    if (okansi()) {
    bout.outstr("|#7\xC0\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9\r\n");
          } else {
      bout.bpla("+--+----------------------------------+--+----------------------------------+", &abort);
    }
    }
  }
}

// Executes a "chain", index number chain_num.
void run_chain(int chain_num) {
  const auto& c = a()->chains->at(chain_num);
  if (auto inst = find_instance_by_loc(INST_LOC_CHAINS, chain_num + 1); inst != 0) {
    const auto h =
        fmt::format("|#2Chain {} is in use on instance {}. ", c.description, inst);
    if (!c.multi_user) {
      bout.print("{} Try again later.\r\n", h);
      return;
    }
    bout.print("{} Care to join in? ", h);
    if (!bin.yesno()) {
      return;
    }
  }
  write_inst(INST_LOC_CHAINS, chain_num + 1, INST_FLAGS_NONE);
  a()->chains->increment_chain_usage(chain_num);
  a()->chains->Save();
  sysoplog(fmt::format("!Ran \"{}\"", c.description));
  a()->user()->chains_run(a()->user()->chains_run() + 1);
#ifdef _WIN32
  ScopeExit at_exit;
  if (c.local_console_cp437) {
    set_wwiv_codepage(wwiv_codepage_t::cp437);
    at_exit.swap([] { set_wwiv_codepage(wwiv_codepage_t::utf8); });
  }
#endif
  wwiv::bbs::CommandLine cl(c.filename);
  cl.args(create_chain_file(), std::to_string(a()->modem_speed_),
          std::to_string(a()->primary_port()), std::to_string(a()->modem_speed_));
  const auto flags = Chains::to_exec_flags(c);
  ExecuteExternalProgram(cl, flags);
  write_inst(INST_LOC_CHAINS, 0, INST_FLAGS_NONE);
  if (c.pause){
     bout.pausescr();
  }
  a()->UpdateTopScreen();
}

//////////////////////////////////////////////////////////////////////////////
// Main high-level function for chain access and execution.

void do_chains() {

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
    bout.outstr("\r\n\n|#5Sorry, no external programs available.\r\n");
    return;
  }

  auto done  = false;
  auto start  = 0;
  std::string ss;
  do {
    show_chains(&mapp, map);
    a()->tleft(true);
    bout.nl();
    bout.print("|#5Which Chain (1-{}, Q=Quit, ", mapp);
    if (so()) {
      bout.outstr("*=ChainEdit, ");
    }
    bout.outstr("?=List): ");

    if (mapp < 100) {
      ss = mmkey(odc);
    } else {
      ss = bin.input_upper(3);
    }
    if (const auto chain_num = to_number<int>(ss); chain_num > 0 && chain_num <= mapp) {
      bout.outstr("\r\n|#6Please wait...\r\n");
      run_chain(map[chain_num - 1]);
    } else if (ss == "Q") {
      done = true;
    } else if (ss == "*" && so()) {
      chainedit();
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

