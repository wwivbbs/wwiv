/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "wwivconfig/protocols.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "fmt/format.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

static std::string prot_name(const vector<newexternalrec>& externs, int pn) {
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
    if (pn > 5 || pn < (ssize(externs) + 6)) {
      return externs[pn - 6].description;
    }
  }
  return ">NONE<";
}

static void load_protocols(const std::string& datadir, vector<newexternalrec>& externs, vector<newexternalrec>& over_intern) {
  externs.clear();
  over_intern.clear();
  if (auto file = DataFile<newexternalrec>(FilePath(datadir, NEXTERN_DAT),
                                           File::modeBinary | File::modeReadWrite)) {
    file.ReadVector(externs, 15);
  }
  if (auto file = DataFile<newexternalrec>(FilePath(datadir, NINTERN_DAT),
                                File::modeBinary | File::modeReadWrite)) {
    file.ReadVector(over_intern, 3);
  } else {
    for (auto i = 0; i < 3; i++) {
      over_intern.push_back({});
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
    to_char_array(c.description, prot_name(over_intern, n));
  }

  auto y = 1;
  EditItems items{};
  items.add(new Label("Description:"), 1, y);
  if (n >= 4) {
    items.add(new StringEditItem<char*>(45, c.description, EditLineMode::ALL), 2, 1);
  } else {
    items.add(new Label(c.description), 2, 1)->set_right_justified(false);
  }
  y++;
  items.add(new Label("Xfer OK code:"), new NumberEditItem<uint16_t>(&c.ok1), 1, y++);
  items.add(new Label("Receive command line:"), 1, y);
  items.add(new Label(""), 2, y++);  // hack to fix layout
  items.add(new CommandLineItem(70, c.receivefn), 1, y++);
  items.add(new Label("Send command line:"), 1, y);
  items.add(new Label(""), 2, y++);  // hack to fix layout
  items.add(new CommandLineItem(70, c.sendfn), 1, y++);
  items.add(new Label("Receive batch command line:"), 1, y);
  items.add(new Label(""), 2, y++);  // hack to fix layout
  const auto receive_batch_pos = y++;
  items.add(new Label("Send batch command line:"), 1, y);
  items.add(new Label(""), 2, y++);  // hack to fix layout
  const auto send_batch_pos = y++;
  // Not else since we want the n >= 4 to be invoked.
  if (n >= 6) {
    items.add(new CommandLineItem(70, c.receivebatchfn), 1, receive_batch_pos);
    items.add(new CommandLineItem(70, c.sendbatchfn), 1, send_batch_pos);
  } else if (n >= 4) {
    items.add(new CommandLineItem(70, c.sendbatchfn), 1, send_batch_pos);
    items.add(new Label("-- N/A --"), 1, receive_batch_pos)->set_right_justified(false);
  } else {
    items.add(new Label("-- N/A --"), 1, send_batch_pos)->set_right_justified(false);
    items.add(new Label("-- N/A --"), 1, receive_batch_pos)->set_right_justified(false);
  }
  ++y;
  items.add(new MultilineLabel(R""""(%1 = com port baud rate
%2 = port number
%3 = filename to transfer, filename list to send for batch
%4 = modem speed
%5 = filename list to receive for batch UL
NOTE: Batch protocols >MUST< correctly support DSZLOG.)""""), 1, y);
//  items.cell(y, 1).colspan_ = 2;
  items.relayout_items_and_labels();
  items.Run("Protocol Configuration");

  if (n >= 6) {
    externs[n - 6] = c;
  } else {
    if (c.receivefn[0] || c.sendfn[0] || (c.sendbatchfn[0] && n == 4)) {
      c.othr |= othr_override_internal;
    } else {
      c.othr &= ~othr_override_internal;
    }
    over_intern[n - 2] = c;
  }
}

void extrn_prots(const std::string& datadir) {
  vector<newexternalrec> externs;
  vector<newexternalrec> over_interns;
  load_protocols(datadir, externs, over_interns);

  bool done = false;
  do {
    curses_out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    items.emplace_back("2. XModem (Internal)", 0, 2);
    items.emplace_back("X. XModem CRC (Internal)", 0, 3);
    items.emplace_back("Y. YModem (Internal)", 0, 4);
    for (auto i = 0; i < wwiv::stl::ssize(externs); i++) {
      items.emplace_back(fmt::format("{}. {} (External)", i + 6, prot_name(externs, i+6)), 0, i+6);
    }
    CursesWindow* window(curses_out->window());
    ListBox list(window, "Select Protocol", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    auto result = list.Run();
    const auto max_protocol_number = size_int(externs) -1 + 6;

    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (!externs.empty()) {
          if (items[result.selected].data() < 6) {
            messagebox(curses_out->window(), "You can only delete external protocols.");
            break;
          }
          auto prompt = fmt::format("Delete '{}' ?", items[result.selected].text());
          auto yn = dialog_yn(window, prompt);
          if (!yn) {
            break;
          }
          auto pos = result.selected - 3; // 3 is the number of internal protocols listed.
          erase_at(externs, pos);
        }
      } break;
      case 'I': {
        if (externs.size() >= 15) {
          messagebox(curses_out->window(), "Too many external protocols.");
          break;
        }
        auto prompt = fmt::format("Insert before which (6-{}) ? ", max_protocol_number + 1);
        auto pos = dialog_input_number(curses_out->window(), prompt, 2, max_protocol_number + 1);
        if (pos >= 6 && pos <= wwiv::stl::ssize(externs) + 6) {
          size_t extern_pos = pos - 6;
          newexternalrec e{};
          memset(&e, 0, sizeof(newexternalrec));
          if (extern_pos > externs.size()) {
            externs.push_back(e);
          } else {
            insert_at(externs, extern_pos, e);
          }
          edit_prot(externs, over_interns, pos);
        } else {
          messagebox(curses_out->window(), fmt::format("Invalid entry: {}", pos));
        }
      } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_prot(externs, over_interns, items[result.selected].data());
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  DataFile<newexternalrec> newexternfile(FilePath(datadir, NEXTERN_DAT),
    File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate);
  if (newexternfile) {
    newexternfile.WriteVector(externs);
  }
  newexternfile.Close();

  if ((over_interns[0].othr | over_interns[1].othr | over_interns[2].othr)&othr_override_internal) {
    DataFile<newexternalrec> internfile(FilePath(datadir, NINTERN_DAT),
      File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate);
    if (internfile) {
      internfile.WriteVector(over_interns);
    }
  } else {
    File::Remove(FilePath(datadir, NINTERN_DAT));
  }
}

