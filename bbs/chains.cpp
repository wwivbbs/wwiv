/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include <algorithm>
#include <map>
#include <string>

#include "bbs/input.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/multinst.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/instmsg.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/wconstants.h"
#include "core/datafile.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::strings;

// Displays the list of chains to a user
// Note: we aren't using a const map since [] doesn't work for const maps.
static void show_chains(int *mapp, std::map<int, int>& map) {
  bout.Color(0);
  bout.cls();
  bout.nl();
  bool abort = false;
  bool next = false;
  if (session()->HasConfigFlag(OP_FLAGS_CHAIN_REG) 
      && session()->chains_reg.size() > 0) {
    pla(StringPrintf("|#5  Num |#1%-42.42s|#2%-22.22s|#1%-5.5s", "Description", "Sponsored by", "Usage"), &abort);

    if (okansi()) {
      pla(StringPrintf("|#%d %s", FRAME_COLOR,
              "\xDA\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xBF"), &abort);
    } else {
      pla(StringPrintf(" +---+-----------------------------------------+---------------------+-----+"), &abort);
    }
    for (int i = 0; i < *mapp && !abort && !hangup; i++) {
      WUser user;
      if (okansi()) {
        session()->users()->ReadUser(&user, session()->chains_reg[map[i]].regby[0]);
        pla(StringPrintf(" |#%d\xB3|#5%3d|#%d\xB3|#1%-41s|#%d\xB3|%2.2d%-21s|#%d\xB3|#1%5d|#%d\xB3",
                FRAME_COLOR,
                i + 1,
                FRAME_COLOR,
                session()->chains[map[i]].description,
                FRAME_COLOR,
                (session()->chains_reg[map[i]].regby[0]) ? 14 : 13,
                (session()->chains_reg[map[i]].regby[0]) ? user.GetName() : "Available",
                FRAME_COLOR,
                session()->chains_reg[map[i]].usage,
                FRAME_COLOR), &abort);
        if (session()->chains_reg[map[i]].regby[0] != 0) {
          for (int i1 = 1; i1 < 5 && !abort; i1++) {
            if (session()->chains_reg[map[i]].regby[i1] != 0) {
              session()->users()->ReadUser(&user, session()->chains_reg[map[i]].regby[i1]);
              pla(StringPrintf(" |#%d\xB3   \xBA%-41s\xB3|#2%-21s|#%d\xB3%5.5s\xB3",
                      FRAME_COLOR, " ", user.GetName(), FRAME_COLOR, " "), &abort);
            }
          }
        }
      } else {
        session()->users()->ReadUser(&user, session()->chains_reg[map[i]].regby[0]);
        pla(StringPrintf(" |%3d|%-41.41s|%-21.21s|%5d|",
                i + 1, session()->chains[map[i]].description,
                (session()->chains_reg[map[i]].regby[0]) ? user.GetName() : "Available",
                session()->chains_reg[map[i]].usage), &abort);
        if (session()->chains_reg[map[i]].regby[0] != 0) {
          for (int i1 = 1; i1 < 5; i1++) {
            if (session()->chains_reg[map[i]].regby[i1] != 0) {
              session()->users()->ReadUser(&user, session()->chains_reg[map[i]].regby[i1]);
              pla(StringPrintf(" |   |                                         |%-21.21s|     |",
                      (session()->chains_reg[map[i]].regby[i1]) ? user.GetName() : "Available"), &abort);
            }
          }
        }
      }
    }
    if (okansi()) {
      pla(StringPrintf("|#%d %s", FRAME_COLOR, "\xC0\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xD9"), &abort);
    } else {
      pla(StringPrintf(" +---+-----------------------------------------+---------------------+-----+"), &abort);
    }
  } else {
    bout.litebar(" %s Online Programs ", syscfg.systemname);
    bout << "|#7\xDA\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF\r\n";
    for (int i = 0; i < *mapp && !abort && !hangup; i++) {
      osan(StringPrintf("|#7\xB3|#2%2d|#7\xB3 |#1%-33.33s|#7\xB3", i + 1, session()->chains[map[i]].description), &abort, &next);
      i++;
      if (!abort && !hangup) {
        if (i >= *mapp) {
          pla(StringPrintf("  |#7\xB3                                  |#7\xB3"), &abort);
        } else {
          pla(StringPrintf("|#2%2d|#7\xB3 |#1%-33.33s|#7\xB3", i + 1, session()->chains[map[i]].description), &abort);
        }
      }
    }
    bout << "|#7\xC0\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9\r\n";
  }
}

// Executes a "chain", index number nChainNumber.
void run_chain(int nChainNumber) {
  int inst = inst_ok(INST_LOC_CHAINS, nChainNumber + 1);
  if (inst != 0) {
    const string message = StringPrintf("|#2Chain %s is in use on instance %d.  ", 
        session()->chains[nChainNumber].description, inst);
    if (!(session()->chains[nChainNumber].ansir & ansir_multi_user)) {
      bout << message << "Try again later.\r\n";
      return;
    } else {
      bout << message << "Care to join in? ";
      if (!yesno()) {
        return;
      }
    }
  }
  write_inst(INST_LOC_CHAINS, static_cast< unsigned short >(nChainNumber + 1), INST_FLAGS_NONE);
  if (session()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    session()->chains_reg[nChainNumber].usage++;
    wwiv::core::DataFile<chainregrec> regFile(syscfg.datadir, CHAINS_REG,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    if (regFile) {
      regFile.Write(&(session()->chains_reg[0]), session()->chains.size());
    }
  }
  const string com_speed_str = StringPrintf("%d", (com_speed == 1) ? 115200 : com_speed);
  const string com_port_num_str = StringPrintf("%d", syscfgovr.primaryport);
  const string modem_speed_str = StringPrintf("%d", modem_speed);
  const string chainCmdLine = stuff_in(session()->chains[nChainNumber].filename, create_chain_file(), com_speed_str, com_port_num_str, modem_speed_str, "");

  sysoplogf("!Ran \"%s\"", session()->chains[nChainNumber].description);
  session()->user()->SetNumChainsRun(session()->user()->GetNumChainsRun() + 1);

  int flags = 0;
  if (!(session()->chains[nChainNumber].ansir & ansir_no_DOS)) {
    flags |= EFLAG_COMIO;
  }
  if (session()->chains[nChainNumber].ansir & ansir_emulate_fossil) {
    flags |= EFLAG_FOSSIL;
  }

  ExecuteExternalProgram(chainCmdLine, flags);
  write_inst(INST_LOC_CHAINS, 0, INST_FLAGS_NONE);
  session()->UpdateTopScreen();
}

//////////////////////////////////////////////////////////////////////////////
// Main high-level function for chain access and execution.

void do_chains() {
  printfile(CHAINS_NOEXT);

  std::map<int, int> map;

  session()->localIO()->tleft(true);
  int mapp = 0;
  memset(odc, 0, sizeof(odc));
  for (size_t i = 0; i < session()->chains.size(); i++) {
    bool ok = true;
    chainfilerec c = session()->chains[i];
    if ((c.ansir & ansir_ansi) && !okansi()) {
      ok = false;
    }
    if ((c.ansir & ansir_local_only) && session()->using_modem) {
      ok = false;
    }
    if (c.sl > session()->GetEffectiveSl()) {
      ok = false;
    }
    if (c.ar && !session()->user()->HasArFlag(c.ar)) {
      ok = false;
    }
    if (session()->HasConfigFlag(OP_FLAGS_CHAIN_REG) 
      && session()->chains_reg.size() > 0 
      && (session()->GetEffectiveSl() < 255)) {
      chainregrec r = session()->chains_reg[ i ];
      if (r.maxage) {
        if (r.minage > session()->user()->GetAge() || r.maxage < session()->user()->GetAge()) {
          ok = false;
        }
      }
    }
    if (ok) {
      map[mapp++] = i;
      if (mapp < 100) {
        if ((mapp % 10) == 0) {
          odc[mapp / 10-1] = static_cast<char>('0' + (mapp / 10));
        }
      }
    }
  }
  if (mapp == 0) {
    bout << "\r\n\n|#5Sorry, no external programs available.\r\n";
    return;
  }
  show_chains(&mapp, map);

  bool done  = false;
  int start  = 0;
  char* ss = nullptr;

  do {
    session()->localIO()->tleft(true);
    bout.nl();
    bout << "|#7Which chain (1-" << mapp << ", Q=Quit, ?=List): ";

    int nChainNumber = -1;
    if (mapp < 100) {
      ss = mmkey(2);
      nChainNumber = atoi(ss);
    } else {
      string chain_number;
      input(&chain_number, 3);
      nChainNumber = atoi(chain_number.c_str());
    }
    if (nChainNumber > 0 && nChainNumber <= mapp) {
      bout << "\r\n|#6Please wait...\r\n";
      run_chain(map[ nChainNumber - 1 ]);
    } else if (IsEquals(ss, "Q")) {
      done = true;
    } else if (IsEquals(ss, "?")) {
      show_chains(&mapp, map);
    } else if (IsEquals(ss, "P")) {
      if (start > 0) {
        start -= 14;
      }
      start = std::max<int>(start, 0);
    } else if (IsEquals(ss, "N")) {
      if (start + 14 < mapp) {
        start += 14;
      }
    }
  } while (!hangup  && !done);
}

