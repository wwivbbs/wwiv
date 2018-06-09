/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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
#include "wwivconfig/archivers.h"

#include "localui/wwiv_curses.h"
#include <cmath>
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

#include "bbs/wconstants.h" // for MAX_ARCHIVERS
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "wwivconfig/wwivconfig.h"
#include "wwivconfig/utility.h"
#include "wwivconfig/wwivinit.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::strings;

static void edit_arc(int arc_number, arcrec* a) {
  static constexpr int LABEL_X = 2;
  static constexpr int COL1_POSITION = 23;
  static constexpr int LABEL_WIDTH = COL1_POSITION - LABEL_X - 1;
  int y = 1;
  EditItems items{};

  items.add_items({new StringEditItem<char*>(COL1_POSITION, y++, 31, a->name, false),
                new StringEditItem<char*>(COL1_POSITION, y++, 3, a->extension, true),
                new CommandLineItem(COL1_POSITION, y++, 49, a->arcl),
                new CommandLineItem(COL1_POSITION, y++, 49, a->arce),
                new CommandLineItem(COL1_POSITION, y++, 49, a->arca),
                new CommandLineItem(COL1_POSITION, y++, 49, a->arcd),
                new CommandLineItem(COL1_POSITION, y++, 49, a->arck),
                new CommandLineItem(COL1_POSITION, y++, 49, a->arct),
  });
  y = 1;
  items.add_labels({new Label(2, y++, LABEL_WIDTH, "Archiver Name:"),
                  new Label(2, y++, LABEL_WIDTH, "Archiver Extension:"),
                  new Label(2, y++, LABEL_WIDTH, "List Archive:"),
                  new Label(2, y++, LABEL_WIDTH, "Extract Archive:"),
                  new Label(2, y++, LABEL_WIDTH, "Add to Archive:"),
                  new Label(2, y++, LABEL_WIDTH, "Delete from Archive:"),
                  new Label(2, y++, LABEL_WIDTH, "Comment Archive:"),
                  new Label(2, y++, LABEL_WIDTH, "Test Archive:")});

  y++;
  items.add_labels({new Label(6, y++, "%1 %2 etc. are parameters passed.  Minimum of two on Add and"),
                    new Label(6, y++,
                              "Extract command lines. For added security, a complete path to"),
                    new Label(6, y++, "the archiver and extension should be used. i.e.:"),
                    new Label(6, y++, "c:\\bin\\arcs\\zip.exe -a %1 %2")});

  items.Run(StringPrintf("Archiver #%d  %s", arc_number, ((arc_number == 1) ? "(Default)" : "")));
}

bool create_arcs(UIWindow* window, const std::string& datadir) {
  vector<arcrec> arc;
  arc.emplace_back(arcrec{"Zip", "ZIP", "zip -j %1 %2", "unzip -j -C %1 %2", "unzip -l %1",
                          "zip -d %1 -@ < BBSADS.TXT", "zip -z %1 < COMMENT.TXT", "unzip -t %1"});
  arc.emplace_back(arcrec{"Arj", "ARJ", "arj.exe a %1 %2", "arj.exe e %1 %2", "arj.exe i %1",
                          "arj.exe d %1 !BBSADS.TXT", "arj.exe c %1 -zCOMMENT.TXT",
                          "arj.exe t %1"});
  arc.emplace_back(arcrec{"Rar", "RAR", "rar.exe a -std -s- %1 %2", "rar.exe e -std -o+ %1 %2",
                          "rar.exe l -std %1", "rar.exe d -std %1 < BBSADS.TXT ",
                          "rar.exe zCOMMENT.TXT -std %1", "rar.exe t -std %1"});
  arc.emplace_back(arcrec{"Lzh", "LZH", "lha.exe a %1 %2", "lha.exe eo  %1 %2", "lha.exe l %1",
                          "lha.exe d %1  < BBSADS.TXT", "", "lha.exe t %1"});
  arc.emplace_back(arcrec{"Pak", "PAK", "pkpak.exe -a %1 %2", "pkunpak.exe -e  %1 %2",
                          "pkunpak.exe -p %1", "pkpak.exe -d %1  @BBSADS.TXT",
                          "pkpak.exe -c %1 < COMMENT.TXT ", "pkunpak.exe -t %1"});

  for (int i = 5; i < MAX_ARCS; i++) {
    arc.emplace_back(arcrec{"New Archiver Name", "EXT", "archive add command",
                            "archive extract command", "archive list command",
                            "archive delete command", "archive comment command",
                            "archive test command"});
  }

  File file(datadir, ARCHIVER_DAT);
  if (!file.Open(File::modeWriteOnly | File::modeBinary | File::modeCreateFile)) {
    messagebox(window,
               StringPrintf("Couldn't open '%s' for writing.\n", file.full_pathname().c_str()));
    return false;
  }
  file.Write(&arc[0], MAX_ARCS * sizeof(arcrec));
  return true;
}

bool edit_archivers(const wwiv::sdk::Config& config) {
  arcrec arc[MAX_ARCS];

  File file(config.datadir(), ARCHIVER_DAT);
  if (!file.Open(File::modeReadWrite | File::modeBinary)) {
    if (!create_arcs(out->window(), config.datadir())) {
      return false;
    }
    file.Open(File::modeReadWrite | File::modeBinary);
  }
  file.Read(&arc, MAX_ARCS * sizeof(arcrec));

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (auto i = 0; i < MAX_ARCS; i++) {
      items.emplace_back(StringPrintf("[%s] %s", arc[i].extension, arc[i].name));
    }
    CursesWindow* window = out->window();
    ListBox list(window, "Select Archiver", items);

    list.selection_returns_hotkey(true);
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}});
    ListBoxResult result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_arc(result.selected + 1, &arc[result.selected]);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  // Copy first four new fomat archivers to oldarcsrec
  // This was the 4.24 and lower place for them.  4.31 introduced
  // the new archivers record.
  for (int j = 0; j < 4; j++) {
    to_char_array(syscfg.arcs[j].extension, arc[j].extension);
    to_char_array(syscfg.arcs[j].arca, arc[j].arca);
    to_char_array(syscfg.arcs[j].arce, arc[j].arce);
    to_char_array(syscfg.arcs[j].arcl, arc[j].arcl);
  }

  // seek to beginning of file, write arcrecs, close file
  file.Seek(0, File::Whence::begin);
  file.Write(arc, MAX_ARCS * sizeof(arcrec));
  return true;
}
