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
#include "protocols.h"

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

#include "init/ifcns.h"
#include "init/init.h"
#include "initlib/input.h"
#include "initlib/listbox.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "init/utility.h"
#include "init/wwivinit.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::strings::StringPrintf;

static const char *prot_name(int pn) {
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
    if (pn > 5 || pn < (initinfo.numexterns + 6)) {
      return externs[pn - 6].description;
    }
  }
  return ">NONE<";
}

static void edit_prot(int n) {
  if (n == 5) {
    // This is the "Batch" protocol which has no override.
    return;
  }

  newexternalrec c;
  if (n >= 6) {
    c = externs[n - 6];
  } else {
    c = over_intern[n - 2];
    strcpy(c.description, prot_name(n));
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

#define BASE_CHAR '!'
#define END_CHAR (BASE_CHAR+10)

void extrn_prots() {
  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    items.emplace_back(StringPrintf("2. XModem (Internal)"), 0, 2);
    items.emplace_back(StringPrintf("X. XModem CRC (Internal)"), 0, 3);
    items.emplace_back(StringPrintf("Y. YModem (Internal)"), 0, 4);
    for (int i = 0; i < initinfo.numexterns; i++) {
      items.emplace_back(StringPrintf("%d. %s (External)", i + 6, prot_name(i+6)), 0, i+6);
    }
    CursesWindow* window(out->window());
    ListBox list(out, window, "Select Protocol", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();
    const int maxProtocolNumber = initinfo.numexterns -1 + 6;

    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (initinfo.numexterns) {
          if (items[result.selected].data() < 6) {
            messagebox(out->window(), "You can only delete external protocols.");
            break;
          }
          string prompt = StringPrintf("Delete '%s' ?", items[result.selected].text().c_str());
          bool yn = dialog_yn(window, prompt);
          if (!yn) {
            break;
          }
          int i = result.selected - 3; // 3 is the number of internal protocols listed.
          for (int i1 = i; i1 < initinfo.numexterns; i1++) {
            externs[i1] = externs[i1 + 1];
          }
          --initinfo.numexterns;
        }
      } break;
      case 'I': {
        if (initinfo.numexterns >= 15) {
          messagebox(out->window(), "Too many external protocols.");
          break;
        }
        string prompt = StringPrintf("Insert before which (6-%d) ? ", maxProtocolNumber + 1);
        int i = dialog_input_number(out->window(), prompt, 2, initinfo.num_languages);
        if ((i >= 6) && (i <= initinfo.numexterns + 6)) {
          for (int i1 = initinfo.numexterns; i1 > i - 6; i1--) {
            externs[i1] = externs[i1 - 1];
          }
          ++initinfo.numexterns;
          memset(externs + i - 6, 0, sizeof(newexternalrec));
          edit_prot(i);
        } else {
          messagebox(out->window(), StringPrintf("Invalid entry: %d", i));
        }
      } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_prot(items[result.selected].data());
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  string filename = StringPrintf("%snextern.dat", syscfg.datadir);
  int hFile = open(filename.c_str(), O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, externs, initinfo.numexterns * sizeof(newexternalrec));
  close(hFile);

  filename = StringPrintf("%snintern.dat", syscfg.datadir);
  if ((over_intern[0].othr | over_intern[1].othr | over_intern[2].othr)&othr_override_internal) {
    hFile = open(filename.c_str(), O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    if (hFile > 0) {
      write(hFile, over_intern, 3 * sizeof(newexternalrec));
      close(hFile);
    }
  } else {
    unlink(filename.c_str());
  }
}
