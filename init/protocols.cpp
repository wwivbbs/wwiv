/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "init/protocols.h"

#include <cmath>
#include <curses.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "init/init.h"
#include "initlib/input.h"
#include "initlib/listbox.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::DataFile;
using wwiv::strings::StringPrintf;

static const char *prot_name(const vector<newexternalrec>& externs, int pn) {
  switch (pn) {
  case 1:
    return "ASCII";
  case 2:
    return "Xmodem";
  case 3:
    return "Xmodem-CRC";
  case 4:
    return "Ymodem";
  case 5:
    return "Batch";
  default:
    if (pn > 5 || pn < (externs.size() + 6)) {
      return externs[pn - 6].description;
    }
  }
  return ">NONE<";
}

static void load_protocols(vector<newexternalrec>& externs, vector<newexternalrec>& over_intern) {
  externs.clear();
  over_intern.clear();
  {
    DataFile<newexternalrec> file(syscfg.datadir, NEXTERN_DAT, File::modeBinary | File::modeReadWrite);
    if (file) {
      file.ReadVector(externs, 15);
    }
  }
  {
    DataFile<newexternalrec> file(syscfg.datadir, NINTERN_DAT, File::modeBinary | File::modeReadWrite);
    if (file) {
      file.ReadVector(over_intern, 3);
    } else {
      newexternalrec e;
      memset(&e, 0, sizeof(newexternalrec));
      for (size_t i = 0; i < 3; i++) {
        over_intern.push_back(e);
      }
    }
  }
}

static void edit_prot(vector<newexternalrec>& externs, vector<newexternalrec>& over_intern, int n) {
  if (n == 5) {
    // This is the "Batch" protocol which has no override.
    return;
  }

  newexternalrec c;
  if (n >= 6) {
    c = externs[n - 6];
  } else {
    c = over_intern[n - 2];
    strcpy(c.description, prot_name(over_intern, n));
  }

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Protocol Configuration", 19, 78));
  const int COL1_POSITION = 19;

  int y = 1;
  window->PrintfXY(2, y++, "Description    : %s", c.description);
  window->PutsXY(2, y++, "Xfer OK code   : ");
  window->PutsXY(2, y++, "Receive command line: ");
  y++;
  window->PutsXY(2, y++, "Send command line   : ");
  y++;
  window->PutsXY(2, y++, "Receive batch command line:");
  y++;
  window->PutsXY(2, y++, "Send batch command line:");
  y+=2;
  window->PutsXY(2, y++, "%1 = com port baud rate");
  window->PutsXY(2, y++, "%2 = port number");
  window->PutsXY(2, y++, "%3 = filename to transfer, filename list to send for batch");
  window->PutsXY(2, y++, "%4 = modem speed");
  window->PutsXY(2, y++, "%5 = filename list to receive for batch UL");
  window->PutsXY(2, y++, "NOTE: Batch protocols >MUST< correctly support DSZLOG.");

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 50, c.description, false),
    new NumberEditItem<uint16_t>(COL1_POSITION, 2, &c.ok1),
    new CommandLineItem(2, 4, 70, c.receivefn),
    new CommandLineItem(2, 6, 70, c.sendfn),
  };
  items.set_curses_io(out, window.get());
  if (n < 6) {
    items.items().erase(items.items().begin());
    window->PutsXY(COL1_POSITION, 1, c.description);
    window->PutsXY(2, 8, "-- N/A --");
    window->PutsXY(2, 10, "-- N/A --");
  }
  // Not else since we want the n >= 4 to be invoked.
  if (n >= 6) {
    items.items().emplace_back(new CommandLineItem(2, 8, 70, c.receivebatchfn));
    items.items().emplace_back(new CommandLineItem(2, 10, 70, c.sendbatchfn));
  } else if (n >= 4) {
    items.items().emplace_back(new CommandLineItem(2, 8, 70, c.sendbatchfn));
    window->PutsXY(2, 10, "-- N/A --");
  }

  items.Run();

  if (n >= 6) {
    externs[n - 6] = c;
  } else {
    if (c.receivefn[0] || c.sendfn[0] || (c.sendbatchfn[0] && (n == 4))) {
      c.othr |= othr_override_internal;
    } else {
      c.othr &= ~othr_override_internal;
    }
    over_intern[n - 2] = c;
  }
}

void extrn_prots() {
  vector<newexternalrec> externs;
  vector<newexternalrec> over_interns;
  load_protocols(externs, over_interns);

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    items.emplace_back(StringPrintf("2. XModem (Internal)"), 0, 2);
    items.emplace_back(StringPrintf("X. XModem CRC (Internal)"), 0, 3);
    items.emplace_back(StringPrintf("Y. YModem (Internal)"), 0, 4);
    for (size_t i = 0; i < externs.size(); i++) {
      items.emplace_back(StringPrintf("%d. %s (External)", i + 6, prot_name(externs, i+6)), 0, i+6);
    }
    CursesWindow* window(out->window());
    ListBox list(out, window, "Select Protocol", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();
    const int max_protocol_number = externs.size() -1 + 6;

    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (externs.size() > 0) {
          if (items[result.selected].data() < 6) {
            messagebox(out->window(), "You can only delete external protocols.");
            break;
          }
          string prompt = StringPrintf("Delete '%s' ?", items[result.selected].text().c_str());
          bool yn = dialog_yn(window, prompt);
          if (!yn) {
            break;
          }
          int pos = result.selected - 3; // 3 is the number of internal protocols listed.
          auto it = externs.begin();
          std::advance(it, pos);
          externs.erase(it);
        }
      } break;
      case 'I': {
        if (externs.size() >= 15) {
          messagebox(out->window(), "Too many external protocols.");
          break;
        }
        string prompt = StringPrintf("Insert before which (6-%d) ? ", max_protocol_number + 1);
        int pos = dialog_input_number(out->window(), prompt, 2, max_protocol_number + 1);
        if ((pos >= 6) && (pos <= externs.size() + 6)) {
          size_t extern_pos = pos - 6;
          newexternalrec e;
          memset(&e, 0, sizeof(newexternalrec));
          if (extern_pos > externs.size()) {
            externs.push_back(e);
          } else {
            auto it = externs.begin();
            std::advance(it, extern_pos);
            externs.insert(it, e);
          }
          edit_prot(externs, over_interns, pos);
        } else {
          messagebox(out->window(), StringPrintf("Invalid entry: %d", pos));
        }
      } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_prot(externs, over_interns, items[result.selected].data());
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  DataFile<newexternalrec> newexternfile(syscfg.datadir, NEXTERN_DAT,
    File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate);
  if (newexternfile) {
    newexternfile.WriteVector(externs);
  }
  newexternfile.Close();

  if ((over_interns[0].othr | over_interns[1].othr | over_interns[2].othr)&othr_override_internal) {
    DataFile<newexternalrec>  internfile(syscfg.datadir, NINTERN_DAT,
      File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate);
    if (internfile) {
      internfile.WriteVector(over_interns);
    }
  } else {
    File::Remove(syscfg.datadir, NINTERN_DAT);
  }
}

