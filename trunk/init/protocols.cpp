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
  newexternalrec c;
  if (n >= 6) {
    c = externs[n - 6];
  } else {
    if (n == 5) {
      // This is the "Batch" protocol which has no override.
      return;
    }
    c = over_intern[n - 2];
    strcpy(c.description, prot_name(n));
    strcpy(c.receivebatchfn, "-- N/A --");
    if (n != 4) {
      strcpy(c.sendbatchfn, "-- N/A --");
    }
  }

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Protocol Configuration", 19, 78));
  const int COL1_POSITION = 19;

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 50, c.description, false),
    new NumberEditItem<uint16_t>(COL1_POSITION, 2, &c.ok1),
    new CommandLineItem(2, 4, 70, c.receivefn),
    new CommandLineItem(2, 6, 70, c.sendfn),
  };
  items.set_curses_io(out, window.get());
  if (n < 6) {
    items.items().erase(items.items().begin());
  } else if (n >= 6) {
    items.items().emplace_back(new CommandLineItem(2, 8, 70, c.receivebatchfn));
    items.items().emplace_back(new CommandLineItem(2, 10, 70, c.sendbatchfn));
  } else if (n >= 4) {
    items.items().emplace_back(new CommandLineItem(2, 8, 70, c.sendbatchfn));
  }

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
  window->SetColor(SchemeId::WARNING);
  window->PutsXY(2, y++, "NOTE: Batch protocols >MUST< correctly support DSZLOG.");
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
  CursesWindow* window(out->window());
  do {
    out->Cls();
    nlx();
    for (int i = 2; i < 6 + initinfo.numexterns; i++) {
      if (i == 5) {
        continue;
      }
      window->Printf("%c. %s\n", (i < 10) ? (i + '0') : (i - 10 + BASE_CHAR), prot_name(i));
    }
    int nMaxProtocolNumber = initinfo.numexterns + 6 - 1;
    nlx();
    window->SetColor(SchemeId::PROMPT);
    window->Puts("Externals: M:odify, D:elete, I:nsert, Q:uit : ");
    window->SetColor(SchemeId::NORMAL);
    char ch = onek(window, "Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M': {
      string prompt = StringPrintf("Edit which (2-%d) ? ", nMaxProtocolNumber);
      int i = dialog_input_number(out->window(), prompt, 2, initinfo.num_languages);
      if ((i >= 2) && (i < initinfo.numexterns + 6)) {
        edit_prot(i);
      }
    }
    break;
    case 'D':
      if (initinfo.numexterns) {
        string prompt = StringPrintf("Delete which (2-%d) ? ", nMaxProtocolNumber);
        int i = dialog_input_number(out->window(), prompt, 2, initinfo.num_languages);
        if (i > 0) {
          i -= 6;
        }
        if ((i > -1) && (i < initinfo.numexterns)) {
          for (int i1 = i; i1 < initinfo.numexterns; i1++) {
            externs[i1] = externs[i1 + 1];
          }
          --initinfo.numexterns;
        }
      }
      break;
    case 'I':
      if (initinfo.numexterns >= 15) {
        messagebox(out->window(), "Too many external protocols.");
        break;
      }
      string prompt = StringPrintf("Insert before which (6-%d) ? ", nMaxProtocolNumber);
      int i = dialog_input_number(out->window(), prompt, 2, initinfo.num_languages);
      if ((i > -1) && (i <= initinfo.numexterns + 6)) {
        for (int i1 = initinfo.numexterns; i1 > i - 6; i1--) {
          externs[i1] = externs[i1 - 1];
        }
        ++initinfo.numexterns;
        memset(externs + i - 6, 0, sizeof(newexternalrec));
        edit_prot(i);
      } else {
        messagebox(out->window(), StringPrintf("Invalid entry: %d", i));
      }
      break;
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
