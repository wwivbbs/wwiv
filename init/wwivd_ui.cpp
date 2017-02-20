/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#include "init/wwivd_ui.h"

#include <cstdint>
#include <memory>
#include <string>

#include "bbs/keycodes.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "init/init.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "localui/wwiv_curses.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/wwivd_config.h"

using std::unique_ptr;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static const int COL1_LINE = 2;
static const int COL1_POSITION = 19;

static void edit_matrix_entry(wwivd_matrix_entry_t& b) {
  EditItems items{};
  char key[2] = { b.key, 0 };
  {
    int y = 1;
    items.add(new StringEditItem<char*>(COL1_POSITION, y++, 1, key));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 12, b.name, false));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 52, b.description, false));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 52, b.telnet_cmd, false));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 52, b.ssh_cmd, false));
    items.add(new BooleanEditItem(out, COL1_POSITION, y++, &b.require_ansi));
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &b.start_node));
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &b.end_node));
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &b.local_node));
  }

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("System Paths", items.size() + 2, 76));
  items.set_curses_io(out, window.get());
  {
    int y = 1;
    window->PutsXY(COL1_LINE, y++, "Key            : ");
    window->PutsXY(COL1_LINE, y++, "Name           : ");
    window->PutsXY(COL1_LINE, y++, "Description    : ");
    window->PutsXY(COL1_LINE, y++, "Telnet Command : ");
    window->PutsXY(COL1_LINE, y++, "SSH Command    : ");
    window->PutsXY(COL1_LINE, y++, "Require ANSI   : ");
    window->PutsXY(COL1_LINE, y++, "Start Node     : ");
    window->PutsXY(COL1_LINE, y++, "End Node       : ");
    window->PutsXY(COL1_LINE, y++, "Local Node     : ");
  }

  items.Run();
  b.key = key[0];
}

// Base item of an editable value, this class does not use templates.
class MatrixSubDialog : public BaseEditItem {
public:
  MatrixSubDialog(wwivd_config_t& c, int x, int y)
    : BaseEditItem(x, y, 1), c_(c), x_(x), y_(y) {};
  virtual ~MatrixSubDialog() {}

  virtual int Run(CursesWindow* window) {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    out->footer()->ShowHelpItems(0, { { "Esc", "Exit" },{ "ENTER", "Edit Items (opens new dialog)." } });
    window->GotoXY(x_, y_);
    int ch = window->GetChar();
    if (ch == KEY_ENTER || ch == TAB || ch == 13) {
      bool done = false;
      do {
        vector<ListBoxItem> items;
        for (const auto& e : c_.bbses) {
          items.emplace_back(e.name);
        }
        ListBox list(out, window, "Select BBS",
          static_cast<int>(floor(window->GetMaxX() * 0.8)),
          std::min<int>(10, static_cast<int>(floor(window->GetMaxY() * 0.8))),
          items, out->color_scheme());

        list.selection_returns_hotkey(true);
        list.set_additional_hotkeys("DI");
        list.set_help_items({ { "Esc", "Exit" },{ "Enter", "Edit" },{ "D", "Delete" },{ "I", "Insert" } });
        ListBoxResult result = list.Run();
        if (result.type == ListBoxResultType::HOTKEY) {
          switch (result.hotkey) {
          case 'D': {
            if (items.empty()) {
              break;
            }
            if (!dialog_yn(window, StrCat("Delete '", items[result.selected].text(), "' ?"))) {
              break;
            }
            wwiv::stl::erase_at(c_.bbses, result.selected);
          } break;
          case 'I': {
            const string name = dialog_input_string(window, "Enter BBS Name: ", 8);
            if (name.empty()) { break; }
            wwivd_matrix_entry_t e{};
            e.name = name;
            e.key = name.front();
            auto pos = result.selected;
            if (pos >= 0 && pos < size_int(items)) {
              wwiv::stl::insert_at(c_.bbses, pos, e);
            }
            else {
              c_.bbses.push_back(e);
            }
          } break;
          }
        }
        else if (result.type == ListBoxResultType::SELECTION) {
          auto& b = c_.bbses.at(result.selected);
          edit_matrix_entry(b);
        }
        else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
      } while (!done);

      return 2;
    }
    else if (ch == KEY_UP || ch == KEY_BTAB) {
      return 1; // PREV
    }
    else {
      return 2;
    }
  }
  virtual void Display(CursesWindow* window) const { window->PutsXY(x_, y_, "[Enter to Edit]"); }
private:
  wwivd_config_t& c_;
  int x_ = 0;
  int y_ = 0;
};


void wwivd_ui(const wwiv::sdk::Config& config) {

  wwivd_config_t c{};
  if (!c.Load(config)) {
    messagebox(out->window(), "Unable to load or create wwivd.json");
    return;
  }

  EditItems items{};
  {
    int y = 1;
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &c.telnet_port));
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &c.ssh_port));
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &c.binkp_port));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 52, c.binkp_cmd, false));
    items.add(new NumberEditItem<int>(COL1_POSITION, y++, &c.http_port));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 16, c.http_address, false));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 12, c.matrix_filename, false));
    items.add(new MatrixSubDialog(c, COL1_POSITION, y++));
  }

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("System Paths", items.size() + 2, 76));
  items.set_curses_io(out, window.get());
  {
    int y = 1;
    window->PutsXY(COL1_LINE, y++, "Telnet Port     : ");
    window->PutsXY(COL1_LINE, y++, "SSH Port        : ");
    window->PutsXY(COL1_LINE, y++, "BinkP Port      : ");
    window->PutsXY(COL1_LINE, y++, "BinkP Cmd       : ");
    window->PutsXY(COL1_LINE, y++, "HTTP Port       : ");
    window->PutsXY(COL1_LINE, y++, "HTTP Address    : ");
    window->PutsXY(COL1_LINE, y++, "Matrix Filename : ");
    window->PutsXY(COL1_LINE, y++, "Matrix Settings : ");
  }

  items.Run();
  c.Save(config);
}
