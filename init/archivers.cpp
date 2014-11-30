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
#include "init/archivers.h"

#include <cmath>
#include <curses.h>
#include <cstdint>
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
#include "core/strings.h"
#include "core/wfile.h"
#include "core/wwivport.h"
#include "bbs/wconstants.h" // for MAX_ARCHIVERS
#include "initlib/input.h"
#include "initlib/listbox.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::strings::StringPrintf;

static void edit_arc(int arc_number, arcrec* a) {
  out->Cls(ACS_CKBOARD);
  const string title = StringPrintf("Archiver #%d  %s", arc_number,
                                    ((arc_number == 1) ? "(Default)" : ""));
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow(title, 16, 74));

  static const int COL1_POSITION = 23;
  int y=1;
  window->PutsXY(2, y++, "Archiver Name      :");
  window->PutsXY(2, y++, "Archiver Extension :");
  window->PutsXY(2, y++, "List Archive       :");
  window->PutsXY(2, y++, "Extract Archive    :");
  window->PutsXY(2, y++, "Add to Archive     :");
  window->PutsXY(2, y++, "Delete from Archive:");
  window->PutsXY(2, y++, "Comment Archive    :");
  window->PutsXY(2, y++, "Test Archive       :");
  
  y++;
  window->PutsXY(6, y++, "%1 %2 etc. are parameters passed.  Minimum of two on Add and");
  window->PutsXY(6, y++, "Extract command lines. For added security, a complete path to");
  window->PutsXY(6, y++, "the archiver and extension should be used. i.e.:");
  window->PutsXY(6, y++, "c:\\bin\\arcs\\zip.exe -a %1 %2");

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 31, a->name, false),
    new StringEditItem<char*>(COL1_POSITION, 2, 3, a->extension, true),
    new CommandLineItem(COL1_POSITION, 3, 49, a->arcl),
    new CommandLineItem(COL1_POSITION, 4, 49, a->arce),
    new CommandLineItem(COL1_POSITION, 5, 49, a->arca),
    new CommandLineItem(COL1_POSITION, 6, 49, a->arcd),
    new CommandLineItem(COL1_POSITION, 7, 49, a->arck),
    new CommandLineItem(COL1_POSITION, 8, 49, a->arct),
  };
  items.set_curses_io(out, window.get());
  items.Run();
}

bool create_arcs(CursesWindow* window) {
  vector<arcrec> arc;
  arc.emplace_back(arcrec{"Zip", "ZIP", 
    "zip %1 %2", "unzip -C %1 %2", 
    "unzip -l %1", "zip -d %1 -@ < BBSADS.TXT", 
    "zip -z %1 < COMMENT.TXT", "unzip -t %1" });
  arc.emplace_back(arcrec{"Arj","ARJ", 
    "arj.exe a %1 %2", "arj.exe e %1 %2", 
    "arj.exe i %1", "arj.exe d %1 !BBSADS.TXT", 
    "arj.exe c %1 -zCOMMENT.TXT", "arj.exe t %1" });
  arc.emplace_back(arcrec{ "Rar", "RAR", 
    "rar.exe a -std -s- %1 %2", "rar.exe e -std -o+ %1 %2", 
    "rar.exe l -std %1", "rar.exe d -std %1 < BBSADS.TXT ",
    "rar.exe zCOMMENT.TXT -std %1", "rar.exe t -std %1" });
  arc.emplace_back(arcrec{ "Lzh", "LZH", 
    "lha.exe a %1 %2", "lha.exe eo  %1 %2", 
    "lha.exe l %1", "lha.exe d %1  < BBSADS.TXT", 
    "", "lha.exe t %1" });
  arc.emplace_back(arcrec{ "Pak", "PAK", 
    "pkpak.exe -a %1 %2", "pkunpak.exe -e  %1 %2", 
    "pkunpak.exe -p %1", "pkpak.exe -d %1  @BBSADS.TXT", 
    "pkpak.exe -c %1 < COMMENT.TXT ", "pkunpak.exe -t %1" });

  for (int i = 5; i < MAX_ARCS; i++) {
    arc.emplace_back(arcrec{ "New Archiver Name", "EXT", 
      "archive add command", "archive extract command", 
      "archive list command", "archive delete command", 
      "archive comment command", "archive test command" });
  }

  WFile file(syscfg.datadir, ARCHIVER_DAT);
  if (!file.Open(WFile::modeWriteOnly|WFile::modeBinary|WFile::modeCreateFile)) {
    messagebox(window, StringPrintf("Couldn't open '%s' for writing.\n", file.full_pathname().c_str()));
    return false;
  }
  file.Write(&arc[0], MAX_ARCS * sizeof(arcrec));
  return true;
}

bool edit_archivers() {
  arcrec arc[MAX_ARCS];

  WFile file(syscfg.datadir, ARCHIVER_DAT);
  if (!file.Open(WFile::modeReadWrite|WFile::modeBinary)) {
    if (!create_arcs(out->window())) {
      return false;
    }
    file.Open(WFile::modeReadWrite|WFile::modeBinary);
  }
  file.Read(&arc, MAX_ARCS * sizeof(arcrec));
  
  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (int i = 0; i < MAX_ARCS; i++) {
      items.emplace_back(StringPrintf("[%s] %s", arc[i].extension, arc[i].name));
    }
    CursesWindow* window = out->window();
    ListBox list(out, window, "Select Archiver",
        static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        std::min<int>(10, static_cast<int>(floor(window->GetMaxY() * 0.8))),
        items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"} });
    ListBoxResult result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_arc(result.selected + 1, &arc[result.selected]);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  // copy first four new fomat archivers to oldarcsrec
  for (int j = 0; j < 4; j++) {
    strncpy(syscfg.arcs[j].extension, arc[j].extension, 4);
    strncpy(syscfg.arcs[j].arca, arc[j].arca     , 32);
    strncpy(syscfg.arcs[j].arce, arc[j].arce     , 32);
    strncpy(syscfg.arcs[j].arcl, arc[j].arcl     , 32);
  }

  // seek to beginning of file, write arcrecs, close file
  file.Seek(0, WFile::seekBegin);
  file.Write(arc, MAX_ARCS * sizeof(arcrec));
  return true;
}

