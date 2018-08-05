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
#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "bbs/input.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/multinst.h"
#include "bbs/mmkey.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/execexternal.h"
#include "bbs/vars.h"
#include "bbs/instmsg.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/wconstants.h"
#include "core/datafile.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"
#include "sdk/user.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Displays the list of chains to a user
// Note: we aren't using a const map since [] doesn't work for const maps.
static void show_chains(int *mapp, std::map<int, int>& map) {
  bout.Color(0);
  bout.cls();
  bout.nl();
  bool abort = false;
  bool next = false;
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG) 
      && a()->chains_reg.size() > 0) {
    bout.bpla(StringPrintf("|#5  Num |#1%-42.42s|#2%-22.22s|#1%-5.5s", "Description", "Sponsored by", "Usage"), &abort);

    if (okansi()) {
      bout.bpla(StringPrintf("|#%d %s", FRAME_COLOR,
              "\xDA\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xBF"), &abort);
    } else {
      bout.bpla(StringPrintf(" +---+-----------------------------------------+---------------------+-----+"), &abort);
    }
    for (int i = 0; i < *mapp && !abort && !a()->hangup_; i++) {
      User user;
      if (okansi()) {
        a()->users()->readuser(&user, a()->chains_reg[map[i]].regby[0]);
        bout.bpla(StringPrintf(" |#%d\xB3|#5%3d|#%d\xB3|#1%-41s|#%d\xB3|%2.2d%-21s|#%d\xB3|#1%5d|#%d\xB3",
                FRAME_COLOR,
                i + 1,
                FRAME_COLOR,
                a()->chains[map[i]].description,
                FRAME_COLOR,
                (a()->chains_reg[map[i]].regby[0]) ? 14 : 13,
                (a()->chains_reg[map[i]].regby[0]) ? user.GetName() : "Available",
                FRAME_COLOR,
                a()->chains_reg[map[i]].usage,
                FRAME_COLOR), &abort);
        if (a()->chains_reg[map[i]].regby[0] != 0) {
          for (int i1 = 1; i1 < 5 && !abort; i1++) {
            if (a()->chains_reg[map[i]].regby[i1] != 0) {
              a()->users()->readuser(&user, a()->chains_reg[map[i]].regby[i1]);
              bout.bpla(StringPrintf(" |#%d\xB3   \xBA%-41s\xB3|#2%-21s|#%d\xB3%5.5s\xB3",
                      FRAME_COLOR, " ", user.GetName(), FRAME_COLOR, " "), &abort);
            }
          }
        }
      } else {
        a()->users()->readuser(&user, a()->chains_reg[map[i]].regby[0]);
        bout.bpla(StringPrintf(" |%3d|%-41.41s|%-21.21s|%5d|",
                i + 1, a()->chains[map[i]].description,
                (a()->chains_reg[map[i]].regby[0]) ? user.GetName() : "Available",
                a()->chains_reg[map[i]].usage), &abort);
        if (a()->chains_reg[map[i]].regby[0] != 0) {
          for (int i1 = 1; i1 < 5; i1++) {
            if (a()->chains_reg[map[i]].regby[i1] != 0) {
              a()->users()->readuser(&user, a()->chains_reg[map[i]].regby[i1]);
              bout.bpla(StringPrintf(" |   |                                         |%-21.21s|     |",
                      (a()->chains_reg[map[i]].regby[i1]) ? user.GetName() : "Available"), &abort);
            }
          }
        }
      }
    }
    if (okansi()) {
      bout.bpla(StringPrintf("|#%d %s", FRAME_COLOR, "\xC0\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xD9"), &abort);
    } else {
      bout.bpla(StringPrintf(" +---+-----------------------------------------+---------------------+-----+"), &abort);
    }
  } else {
    bout.litebar(StrCat(a()->config()->system_name(), " Online Programs"));
    bout << "|#7\xDA\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF\r\n";
    for (int i = 0; i < *mapp && !abort && !a()->hangup_; i++) {
      bout.bputs(StringPrintf("|#7\xB3|#2%2d|#7\xB3 |#1%-33.33s|#7\xB3", i + 1, a()->chains[map[i]].description), &abort, &next);
      i++;
      if (!abort && !a()->hangup_) {
        if (i >= *mapp) {
          bout.bpla(StringPrintf("  |#7\xB3                                  |#7\xB3"), &abort);
        } else {
          bout.bpla(StringPrintf("|#2%2d|#7\xB3 |#1%-33.33s|#7\xB3", i + 1, a()->chains[map[i]].description), &abort);
        }
      }
    }
    bout << "|#7\xC0\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9\r\n";
  }
}

// Executes a "chain", index number nChainum.
void run_chain(int nChainum) {
  int inst = inst_ok(INST_LOC_CHAINS, nChainum + 1);
  if (inst != 0) {
    const string message = StringPrintf("|#2Chain %s is in use on instance %d.  ", 
        a()->chains[nChainum].description, inst);
    if (!(a()->chains[nChainum].ansir & ansir_multi_user)) {
      bout << message << "Try again later.\r\n";
      return;
    } else {
      bout << message << "Care to join in? ";
      if (!yesno()) {
        return;
      }
    }
  }
  write_inst(INST_LOC_CHAINS, static_cast<uint16_t>(nChainum + 1), INST_FLAGS_NONE);
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    a()->chains_reg[nChainum].usage++;
    wwiv::core::DataFile<chainregrec> regFile(FilePath(a()->config()->datadir(), CHAINS_REG),
      File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    if (regFile) {
      regFile.WriteVector(a()->chains_reg);
    }
  }
  const auto chainCmdLine = stuff_in(
      a()->chains[nChainum].filename, create_chain_file(), std::to_string(a()->modem_speed_),
      std::to_string(a()->primary_port()), std::to_string(a()->modem_speed_), "");

  sysoplog() << "!Ran \"" << a()->chains[nChainum].description << "\"";
  a()->user()->SetNumChainsRun(a()->user()->GetNumChainsRun() + 1);

  int flags = 0;
  auto& c = a()->chains[nChainum];
  if (!(c.ansir & ansir_no_DOS)) {
    flags |= EFLAG_COMIO;
  }
  if (c.ansir & ansir_emulate_fossil) {
    flags |= EFLAG_FOSSIL;
  }
  if (c.ansir & ansir_stdio) {
    flags |= EFLAG_STDIO;
  }
  if (c.ansir & ansir_temp_dir) {
    flags |= EFLAG_TEMP_DIR;
  }

  ExecuteExternalProgram(chainCmdLine, flags);
  write_inst(INST_LOC_CHAINS, 0, INST_FLAGS_NONE);
  a()->UpdateTopScreen();
}

//////////////////////////////////////////////////////////////////////////////
// Main high-level function for chain access and execution.

void do_chains() {
  printfile(CHAINS_NOEXT);

  std::map<int, int> map;

  a()->tleft(true);
  int mapp = 0;
  std::set<char> odc;
  for (size_t i = 0; i < a()->chains.size(); i++) {
    bool ok = true;
    chainfilerec c = a()->chains[i];
    if ((c.ansir & ansir_ansi) && !okansi()) {
      ok = false;
    }
    if ((c.ansir & ansir_local_only) && a()->using_modem) {
      ok = false;
    }
    if (c.sl > a()->GetEffectiveSl()) {
      ok = false;
    }
    if (c.ar && !a()->user()->HasArFlag(c.ar)) {
      ok = false;
    }
    if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG) 
      && a()->chains_reg.size() > 0 
      && (a()->GetEffectiveSl() < 255)) {
      chainregrec r = a()->chains_reg[i];
      if (r.maxage) {
        if (r.minage > a()->user()->GetAge() || r.maxage < a()->user()->GetAge()) {
          ok = false;
        }
      }
    }
    if (ok) {
      map[mapp++] = i;
      if (mapp < 100) {
        if ((mapp % 10) == 0) {
          odc.insert(static_cast<char>('0' + (mapp / 10)));
        }
      }
    }
  }
  if (mapp == 0) {
    bout << "\r\n\n|#5Sorry, no external programs available.\r\n";
    return;
  }

  bool done  = false;
  int start  = 0;
  string ss;
  do {
    show_chains(&mapp, map);
    a()->tleft(true);
    bout.nl();
    bout << "|#7Which chain (1-" << mapp << ", Q=Quit, ?=List): ";

    if (mapp < 100) {
      ss = mmkey(odc);
    } else {
      ss = input(3);
    }
    int chain_num = to_number<int>(ss);
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
  } while (!a()->hangup_  && !done);
}

