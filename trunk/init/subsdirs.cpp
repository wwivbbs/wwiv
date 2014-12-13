/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <cstdlib>
#include <curses.h>
#include <fcntl.h>
#include <memory>
#include <string>
#ifdef _WIN32
#include <io.h>
#endif
#include <sys/stat.h>

#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "core/file.h"
#include "init/wwivinit.h"
#include "init/ifcns.h"
#include "init/init.h"
#include "initlib/input.h"
#include "init/subacc.h"
#include "init/utility.h"
#include "sdk/filenames.h"

static const int MAX_SUBS_DIRS = 4096;

using std::unique_ptr;
using std::string;
using namespace wwiv::strings;

static int input_number(CursesWindow* window, int max_digits) {
  string s;
  int return_code = 0;
  editline(window, &s, max_digits, NUM_ONLY, &return_code, "");
  if (s.empty()) {
    return 0;
  }
  try {
    return std::stoi(s);
  } catch (std::invalid_argument) { 
    // No conversion possible.
    return 0;
  }
}

static void convert_to(CursesWindow* window, int num_subs, int num_dirs) {
  int l1, l2, l3;

  if (num_subs % 32) {
    num_subs = (num_subs / 32 + 1) * 32;
  }
  if (num_dirs % 32) {
    num_dirs = (num_dirs / 32 + 1) * 32;
  }

  if (num_subs < 32) {
    num_subs = 32;
  }
  if (num_dirs < 32) {
    num_dirs = 32;
  }

  if (num_subs > MAX_SUBS_DIRS) {
    num_subs = MAX_SUBS_DIRS;
  }
  if (num_dirs > MAX_SUBS_DIRS) {
    num_dirs = MAX_SUBS_DIRS;
  }

  std::size_t nqscn_len = 4 * (1 + num_subs + ((num_subs + 31) / 32) + ((num_dirs + 31) / 32));
  uint32_t* nqsc = (uint32_t *)malloc(nqscn_len);
  wwiv::core::ScopeExit free_nqsc([&]() { free(nqsc); nqsc = nullptr; });
  if (!nqsc) {
    return;
  }
  memset(nqsc, 0, nqscn_len);

  uint32_t* nqsc_n = nqsc + 1;
  uint32_t* nqsc_q = nqsc_n + ((num_dirs + 31) / 32);
  uint32_t* nqsc_p = nqsc_q + ((num_subs + 31) / 32);

  memset(nqsc_n, 0xff, ((num_dirs + 31) / 32) * 4);
  memset(nqsc_q, 0xff, ((num_subs + 31) / 32) * 4);

  uint32_t* oqsc = (uint32_t *)malloc(syscfg.qscn_len);
  wwiv::core::ScopeExit free_oqsc([&]() { free(oqsc); oqsc = nullptr; });
  if (!oqsc) {
    messagebox(window, StringPrintf("Could not allocate %d bytes for old quickscan rec\n", syscfg.qscn_len));
    return;
  }
  memset(oqsc, 0, syscfg.qscn_len);

  uint32_t* oqsc_n = oqsc + 1;
  uint32_t* oqsc_q = oqsc_n + ((syscfg.max_dirs + 31) / 32);
  uint32_t* oqsc_p = oqsc_q + ((syscfg.max_subs + 31) / 32);

  if (num_dirs < syscfg.max_dirs) {
    l1 = ((num_dirs + 31) / 32) * 4;
  } else {
    l1 = ((syscfg.max_dirs + 31) / 32) * 4;
  }

  if (num_subs < syscfg.max_subs) {
    l2 = ((num_subs + 31) / 32) * 4;
    l3 = num_subs * 4;
  } else {
    l2 = ((syscfg.max_subs + 31) / 32) * 4;
    l3 = syscfg.max_subs * 4;
  }

  File oqf(syscfg.datadir, USER_QSC);
  if (!oqf.Open(File::modeBinary|File::modeReadWrite)) {
    messagebox(window, "Could not open user.qsc");
    return;
  }
  File nqf(syscfg.datadir, "userqsc.new");
  if (!nqf.Open(File::modeBinary|File::modeReadWrite|File::modeCreateFile|File::modeTruncate)) {
    messagebox(window, "Could not open userqsc.new");
    return;
  }

  int nu = oqf.GetLength() / syscfg.qscn_len;
  for (int i = 0; i < nu; i++) {
    if (i % 10 == 0) {
      window->Printf("%u/%u\r", i, nu);
    }
    oqf.Read(oqsc, syscfg.qscn_len);

    *nqsc = *oqsc;
    memcpy(nqsc_n, oqsc_n, l1);
    memcpy(nqsc_q, oqsc_q, l2);
    memcpy(nqsc_p, oqsc_p, l3);
    nqf.Write(nqsc, nqscn_len);
  }

  oqf.Close();
  nqf.Close();
  oqf.Delete();
  File::Rename(nqf.full_pathname(), oqf.full_pathname());

  syscfg.max_subs = num_subs;
  syscfg.max_dirs = num_dirs;
  syscfg.qscn_len = nqscn_len;
  save_config();
  window->Printf("Done\n");
}

void up_subs_dirs() {
  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Update Sub/Directory Maximums", 16, 76));

  int y=1;
  window->PrintfXY(2, y++, "Current max # subs: %d", syscfg.max_subs);
  window->PrintfXY(2, y++, "Current max # dirs: %d", syscfg.max_dirs);

  if (dialog_yn(window.get(), "Change # subs or # dirs?")) { 
    y+=2;
    window->SetColor(SchemeId::INFO);
    window->PrintfXY(2, y++, "Enter the new max subs/dirs you wish.  Just hit <enter> to leave that");
    window->PrintfXY(2, y++, "value unchanged.  All values will be rounded up to the next 32.");
    window->PrintfXY(2, y++, "Values can range from 32-1024");

    y++;
    window->SetColor(SchemeId::PROMPT);
    window->PutsXY(2, y++, "New max subs: ");
    int num_subs = input_number(window.get(), 4);
    if (!num_subs) {
      num_subs = syscfg.max_subs;
    }
    window->SetColor(SchemeId::PROMPT);
    window->PutsXY(2, y++, "New max dirs: ");
    int num_dirs = input_number(window.get(), 4);
    if (!num_dirs) {
      num_dirs = syscfg.max_dirs;
    }

    if (num_subs % 32) {
      num_subs = (num_subs / 32 + 1) * 32;
    }
    if (num_dirs % 32) {
      num_dirs = (num_dirs / 32 + 1) * 32;
    }

    if (num_subs < 32) {
      num_subs = 32;
    }
    if (num_dirs < 32) {
      num_dirs = 32;
    }

    if (num_subs > MAX_SUBS_DIRS) {
      num_subs = MAX_SUBS_DIRS;
    }
    if (num_dirs > MAX_SUBS_DIRS) {
      num_dirs = MAX_SUBS_DIRS;
    }

    if ((num_subs != syscfg.max_subs) || (num_dirs != syscfg.max_dirs)) {
      const string text = StringPrintf("Change to %d subs and %d dirs? ", num_subs, num_dirs);
      if (dialog_yn(window.get(), text)) {
        window->SetColor(SchemeId::INFO);
        window->Printf("Please wait...\n");
        convert_to(window.get(), num_subs, num_dirs);
      }
    }
  }
}
