/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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
#include "wwivconfig/archivers.h"

#include "core/file.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "local_io/wconstants.h" // for MAX_ARCHIVERS
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;

static void edit_arc(int arc_number, arcrec* a) {
  auto y = 1;
  EditItems items{};

  items.add(new Label("Archiver Name:"), new StringEditItem<char*>(31, a->name, EditLineMode::ALL),
            1, y);
  y++;
  items.add(new Label("Archiver Extension:"),
            new StringEditItem<char*>(3, a->extension, EditLineMode::UPPER_ONLY), 1, y);
  y++;
  items.add(new Label("List Archive:"), new CommandLineItem(49, a->arcl), 1, y);
  y++;
  items.add(new Label("Extract Archive:"), new CommandLineItem(49, a->arce), 1, y);
  y++;
  items.add(new Label("Add to Archive:"), new CommandLineItem(49, a->arca), 1, y);
  y++;
  items.add(new Label("Delete from Archive:"), new CommandLineItem(49, a->arcd), 1, y);
  y++;
  items.add(new Label("Comment Archive:"), new CommandLineItem(49, a->arck), 1, y);
  y++;
  items.add(new Label("Test Archive:"), new CommandLineItem(49, a->arct), 1, y);

  y+=2;
  items.add(new MultilineLabel(R""""(%1 %2 etc. are parameters passed.  Minimum of two on Add and
Extract command lines. For added security, a complete path to
the archiver and extension should be used. i.e.:
c:\bin\arcs\zip.exe -a %1 %2)"""" ), 1, y);

  items.relayout_items_and_labels();
  items.Run(fmt::format("Archiver #{}  {}", arc_number, ((arc_number == 1) ? "(Default)" : "")));
}

bool create_arcs(UIWindow* window, const std::filesystem::path& datadir) {
  vector<arcrec> arc;
  arc.emplace_back(arcrec{"Zip", "ZIP", "zip -j %1 %2", "unzip -o -j -C %1 %2", "@internal",
                          "zip -d %1 -@ < BBSADS.TXT", "zip -z %1 < COMMENT.TXT", "unzip -t %1"});
  arc.emplace_back(arcrec{"Arj", "ARJ", "arj.exe a %1 %2", "arj.exe e %1 %2", "@internal",
                          "arj.exe d %1 !BBSADS.TXT", "arj.exe c %1 -zCOMMENT.TXT",
                          "arj.exe t %1"});
  arc.emplace_back(arcrec{"Rar", "RAR", "rar.exe a -std -s- %1 %2", "rar.exe e -std -o+ %1 %2",
                          "rar.exe l -std %1", "rar.exe d -std %1 < BBSADS.TXT ",
                          "rar.exe zCOMMENT.TXT -std %1", "rar.exe t -std %1"});
  arc.emplace_back(arcrec{"Lzh", "LZH", "lha.exe a %1 %2", "lha.exe eo  %1 %2", "@internal",
                          "lha.exe d %1  < BBSADS.TXT", "", "lha.exe t %1"});
  arc.emplace_back(arcrec{"Pak", "PAK", "pkpak.exe -a %1 %2", "pkunpak.exe -e  %1 %2",
                          "@internal", "pkpak.exe -d %1  @BBSADS.TXT",
                          "pkpak.exe -c %1 < COMMENT.TXT ", "pkunpak.exe -t %1"});
  arc.emplace_back(arcrec{"Arc", "ARC", "pkpak.exe -a %1 %2", "pkunpak.exe -e  %1 %2",
                          "@internal", "pkpak.exe -d %1  @BBSADS.TXT",
                          "pkpak.exe -c %1 < COMMENT.TXT ", "pkunpak.exe -t %1"});

  for (int i = wwiv::stl::size_int(arc); i < MAX_ARCS; i++) {
    arc.emplace_back(arcrec{"New Archiver Name", "EXT", "archive add command",
                            "archive extract command", "archive list command",
                            "archive delete command", "archive comment command",
                            "archive test command"});
  }

  File file(FilePath(datadir, ARCHIVER_DAT));
  if (!file.Open(File::modeWriteOnly | File::modeBinary | File::modeCreateFile)) {
    messagebox(window,
               fmt::format("Couldn't open '{}' for writing.\n", file.full_pathname()));
    return false;
  }
  file.Write(&arc[0], MAX_ARCS * sizeof(arcrec));
  return true;
}

bool edit_archivers(wwiv::sdk::Config& config) {
  arcrec arc[MAX_ARCS];

  File file(FilePath(config.datadir(), ARCHIVER_DAT));
  if (!file.Open(File::modeReadWrite | File::modeBinary)) {
    if (!create_arcs(curses_out->window(), config.datadir())) {
      return false;
    }
    file.Open(File::modeReadWrite | File::modeBinary);
  }
  file.Read(&arc, MAX_ARCS * sizeof(arcrec));

  bool done = false;
  do {
    curses_out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (auto& i : arc) {
      items.emplace_back(fmt::format("[{}] {}", i.extension, i.name));
    }
    ListBox list(curses_out->window(), "Select Archiver", items);

    list.selection_returns_hotkey(true);
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}});
    const auto result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_arc(result.selected + 1, &arc[result.selected]);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  // Copy first four new format archivers to oldarcsrec
  // This was the 4.24 and lower place for them.  4.31 introduced
  // the new archivers record.
  for (int j = 0; j < 4; j++) {
    auto a = config.arc(j);
    to_char_array(a.extension, arc[j].extension);
    to_char_array(a.arca, arc[j].arca);
    to_char_array(a.arce, arc[j].arce);
    to_char_array(a.arcl, arc[j].arcl);
    config.arc(j, a);
  }

  // seek to beginning of file, write arcrecs, close file
  file.Seek(0, File::Whence::begin);
  file.Write(arc, MAX_ARCS * sizeof(arcrec));
  return true;
}
