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

#include "bbs/keycodes.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "init/init.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/wwivd_config.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static const int COL1_LINE = 2;
static const int COL1_POSITION = 21;

// Base item of an editable value, this class does not use templates.
class BlockedCountryCodeSubDialog : public BaseEditItem {
public:
  BlockedCountryCodeSubDialog(wwivd_blocking_t& b, int x, int y) : BaseEditItem(x, y, 1), b_(b){};
  virtual ~BlockedCountryCodeSubDialog() {}

  virtual int Run(CursesWindow* window) {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"ENTER", "Edit Items (opens new dialog)."}});
    window->GotoXY(x_, y_);
    int ch = window->GetChar();
    if (ch == KEY_ENTER || ch == TAB || ch == 13) {
      bool done = false;
      do {
        vector<ListBoxItem> items;
        for (const auto& e : b_.block_cc_countries) {
          items.emplace_back(std::to_string(e));
        }
        ListBox list(out, window, "Select Country Code", items);

        list.selection_returns_hotkey(true);
        list.set_additional_hotkeys("DI");
        list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
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
            wwiv::stl::erase_at(b_.block_cc_countries, result.selected);
          } break;
          case 'I': {
            const string code_str =
                dialog_input_string(window, "Enter ISO-3166 Numeric Country Code: ", 8);
            if (code_str.empty()) {
              break;
            }
            auto code_num = to_number<int>(code_str);
            auto pos = result.selected;
            if (pos >= 0 && pos < size_int(items)) {
              wwiv::stl::insert_at(b_.block_cc_countries, pos, code_num);
            } else {
              b_.block_cc_countries.push_back(code_num);
            }
          } break;
          }
        } else if (result.type == ListBoxResultType::SELECTION) {
          auto& b = b_.block_cc_countries.at(result.selected);
          const string code_str =
              dialog_input_string(window, "Enter ISO-3166 Numeric Country Code: ", 8);
          if (code_str.empty()) {
            break;
          }
          b = to_number<int>(code_str);
        } else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
      } while (!done);

      return 2;
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return 1; // PREV
    } else {
      return 2;
    }
  }
  virtual void Display(CursesWindow* window) const { window->PutsXY(x_, y_, "[Enter to Edit]"); }

private:
  wwivd_blocking_t& b_;
  CursesIO* io_ = nullptr;
};

static void edit_blocking(wwivd_blocking_t& b) { 
  EditItems items{}; 
  int y = 1;
  items.add(new Label(COL1_LINE, y, "Use CC Server:"),
            new BooleanEditItem(COL1_POSITION, y, &b.use_dns_cc));

  y++;
  items.add(
      new Label(COL1_LINE, y, "DNS CC Server:"),
      new StringEditItem<std::string&>(COL1_POSITION, y, 52, b.dns_cc_server, false));

  y++;
  items.add(new Label(COL1_LINE, y, "Blocked Countries:"),
            new BlockedCountryCodeSubDialog(b, COL1_POSITION, y));

  y++;
  items.add(new Label(COL1_LINE, y, "Max Concurrent Sessions:"),
            new NumberEditItem<int>(COL1_POSITION, y, &b.max_concurrent_sessions));
  y++;
  items.relayout_items_and_labels();
  items.Run("Blocking Configuration");
}

// Base item of an editable value, this class does not use templates.
template<class T>
class SubDialog : public BaseEditItem {
public:
  SubDialog(int x, int y, const std::string& text, std::function<void(T&)> fn, T& t)
      : BaseEditItem(x, y, text.size() + 2), text_(text), fn_(fn), t_(t) {};
  virtual ~SubDialog() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(x_, y_);
    int ch = window->GetChar();
    if (ch == KEY_ENTER || ch == TAB || ch == 13) {
      fn_(t_);
      window->RedrawWin();
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return 1; // PREV
    } else {
      return 2; // NEXT
    }
    return 2;
  }
  virtual void Display(CursesWindow* window) const { window->PutsXY(x_, y_, text_); }

private:
  const std::string text_;
  std::function<void(T&)> fn_;
  T& t_;
};

static void edit_matrix_entry(wwivd_matrix_entry_t& b) {
  EditItems items{};
  char key[2] = {b.key, 0};
  {
    int y = 1;
    items.add(new Label(COL1_LINE, y, "Key:"), new StringEditItem<char*>(COL1_POSITION, y, 1, key));
    y++;
    items.add(new Label(COL1_LINE, y, "Name:"),
              new StringEditItem<std::string&>(COL1_POSITION, y, 12, b.name, false));
    y++;
    items.add(new Label(COL1_LINE, y, "Description:"),
              new StringEditItem<std::string&>(COL1_POSITION, y, 52, b.description, false));
    y++;
    items.add(new Label(COL1_LINE, y, "Telnet Command:"),
              new StringEditItem<std::string&>(COL1_POSITION, y, 52, b.telnet_cmd, false));
    y++;
    items.add(new Label(COL1_LINE, y, "SSH Command:"),
              new StringEditItem<std::string&>(COL1_POSITION, y, 52, b.ssh_cmd, false));
    y++;
    items.add(new Label(COL1_LINE, y, "Require Ansi:"),
              new BooleanEditItem(COL1_POSITION, y, &b.require_ansi));
    y++;
    items.add(new Label(COL1_LINE, y, "Start Node:"),
              new NumberEditItem<int>(COL1_POSITION, y, &b.start_node));
    y++;
    items.add(new Label(COL1_LINE, y, "End Node:"),
              new NumberEditItem<int>(COL1_POSITION, y, &b.end_node));
    y++;
    items.add(new Label(COL1_LINE, y, "Local Node:"),
              new NumberEditItem<int>(COL1_POSITION, y, &b.local_node));
    y++;
  }

  items.relayout_items_and_labels();
  items.Run(StrCat("Matrix Config: ", b.name));
  // Need to update key since we have no single char edit item.
  b.key = key[0];
}

// Base item of an editable value, this class does not use templates.
class MatrixSubDialog : public BaseEditItem {
public:
  MatrixSubDialog(wwivd_config_t& c, int x, int y) : BaseEditItem(x, y, 20), c_(c){};
  virtual ~MatrixSubDialog() {}

  virtual int Run(CursesWindow* window) {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"ENTER", "Edit Items (opens new dialog)."}});
    window->GotoXY(x_, y_);
    int ch = window->GetChar();
    if (ch == KEY_ENTER || ch == TAB || ch == 13) {
      bool done = false;
      do {
        vector<ListBoxItem> items;
        for (const auto& e : c_.bbses) {
          items.emplace_back(e.name);
        }
        ListBox list(out, window, "Select BBS", items);

        list.selection_returns_hotkey(true);
        list.set_additional_hotkeys("DI");
        list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
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
            if (name.empty()) {
              break;
            }
            wwivd_matrix_entry_t e{};
            e.name = name;
            e.key = name.front();
            auto pos = result.selected;
            if (pos >= 0 && pos < size_int(items)) {
              wwiv::stl::insert_at(c_.bbses, pos, e);
            } else {
              c_.bbses.push_back(e);
            }
          } break;
          }
        } else if (result.type == ListBoxResultType::SELECTION) {
          auto& b = c_.bbses.at(result.selected);
          edit_matrix_entry(b);
        } else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
      } while (!done);

      return 2;
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return 1; // PREV
    } else {
      return 2;
    }
  }
  virtual void Display(CursesWindow* window) const { window->PutsXY(x_, y_, "[Enter to Edit]"); }

private:
  wwivd_config_t& c_;
  CursesIO* io_ = nullptr;
};

static wwivd_matrix_entry_t CreateWWIVMatrixEntry() {
  wwivd_matrix_entry_t e{};
  e.key = 'W';
  e.description = "WWIV";
  e.name = "WWIV";
  e.local_node = 1;
  e.require_ansi = false;
  e.start_node = 2;
  e.end_node = 4;
  e.telnet_cmd = "./bbs -XT -H@H -N@N";
  e.ssh_cmd = "./bbs -XS -H@H -N@N";
  return e;
}

void wwivd_ui(const wwiv::sdk::Config& config) {

  wwivd_config_t c{};
  if (!c.Load(config)) {
    c.binkp_port = -1;
    c.telnet_port = 2323;
    c.http_port = 8080;
    c.http_address = "127.0.0.1";
    c.binkp_cmd = "./networkb --receive --handle=@H";

    wwivd_matrix_entry_t e = CreateWWIVMatrixEntry();
    c.bbses.push_back(e);
  }

  if (c.bbses.empty()) {
    // If we have no entries in the Matrix, then let's add
    // a default entry for WWIV.
    wwivd_matrix_entry_t e = CreateWWIVMatrixEntry();
    c.bbses.push_back(e);
  }

  EditItems items{};
  int y = 1;
  items.add(new Label(COL1_LINE, y, "Telnet Port:"),
            new NumberEditItem<int>(COL1_POSITION, y, &c.telnet_port));
  y++;
  items.add(new Label(COL1_LINE, y, "SSH Port:"),
            new NumberEditItem<int>(COL1_POSITION, y, &c.ssh_port));
  y++;
  items.add(new Label(COL1_LINE, y, "BinkP Port:"),
            new NumberEditItem<int>(COL1_POSITION, y, &c.binkp_port));
  y++;
  items.add(new Label(COL1_LINE, y, "BinkP Command:"),
            new StringEditItem<std::string&>(COL1_POSITION, y, 52, c.binkp_cmd, false));
  y++;
  items.add(new Label(COL1_LINE, y, "HTTP Port:"),
            new NumberEditItem<int>(COL1_POSITION, y, &c.http_port));
  y++;
  items.add(new Label(COL1_LINE, y, "HTTP Address:"),
            new StringEditItem<std::string&>(COL1_POSITION, y, 16, c.http_address, false));
  y++;
  items.add(new Label(COL1_LINE, y, "Matrix Filename:"),
            new StringEditItem<std::string&>(COL1_POSITION, y, 12, c.matrix_filename, false));
  y++;
  items.add(new Label(COL1_LINE, y, "Matrix Settings:"),
            new MatrixSubDialog(c, COL1_POSITION, y));
  y++;
  items.add(new Label(COL1_LINE, y, "Blocking:"),
            new SubDialog<wwivd_blocking_t>(COL1_POSITION, y, "[Enter to Edit]", edit_blocking,
                                            c.blocking));
  y++;

  items.Run("wwivd Configuration");
  if (!c.Save(config)) {
    messagebox(items.window(), "Error saving wwivd.json");
  }
}
