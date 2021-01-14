/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "wwivconfig/wwivd_ui.h"

#include "core/strings.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/wwivd_config.h"
#include <memory>
#include <string>
#include <utility>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

/**
 * EditItem that executes a std::function<T, CursesWindow*> to
 * edit the items. It is intended that this function will invoke
 * a new EditItem dialog or ListBox for editing.
 */
template <class T> class SubDialogFunction final : public SubDialog<T> {
public:
  SubDialogFunction(const Config& c, T& t,
                    std::function<void(const Config&, T&, CursesWindow*)> fn)
      : SubDialog<T>(c, t), c_(c), t__(t), fn_(std::move(fn)) {}
  ~SubDialogFunction() override = default;

  void RunSubDialog(CursesWindow* window) override { fn_(c_, t__, window); }

private:
  // For some reason GCC couldn't find config() or t_ from SubDialog.
  const Config& c_;
  T& t__;
  std::function<void(const Config&, T&, CursesWindow*)> fn_;
};

static void blocked_country_subdialog(const Config&, wwivd_blocking_t& b_, CursesWindow* window) {
  auto done = false;
  do {
    vector<ListBoxItem> items;
    for (const auto& e : b_.block_cc_countries) {
      items.emplace_back(std::to_string(e));
    }
    ListBox list(window, "Select Country Code", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
    const auto result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (items.empty()) {
          break;
        }
        if (!dialog_yn(window, StrCat("Delete '", items[result.selected].text(), "' ?"))) {
          break;
        }
        erase_at(b_.block_cc_countries, result.selected);
      } break;
      case 'I': {
        const auto code_str =
            dialog_input_string(window, "Enter ISO-3166 Numeric Country Code: ", 8);
        if (code_str.empty()) {
          break;
        }
        auto code_num = to_number<int>(code_str);
        const auto pos = result.selected;
        if (pos >= 0 && pos < ssize(items)) {
          insert_at(b_.block_cc_countries, pos, code_num);
        } else {
          b_.block_cc_countries.push_back(code_num);
        }
      } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      auto& b = b_.block_cc_countries.at(result.selected);
      const auto code_str = dialog_input_string(window, "Enter ISO-3166 Numeric Country Code: ", 8);
      if (code_str.empty()) {
        break;
      }
      b = to_number<int>(code_str);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);
}

// Base item of an editable value, this class does not use templates.
static void edit_blocking(const Config& config, wwivd_blocking_t& b, CursesWindow*) {
  EditItems items{};
  int y = 1;
  items.add(new Label("Use goodip.txt?"), new BooleanEditItem(&b.use_goodip_txt), 1, y);

  y++;
  items.add(new Label("Use badip.txt?"), new BooleanEditItem(&b.use_badip_txt), 1, y);

  y++;
  items.add(new Label("Press <ESC> for BBS?"), new BooleanEditItem(&b.mailer_mode), 1, y);

  y++;
  items.add(new Label("Use CC Server?"), new BooleanEditItem(&b.use_dns_cc), 1, y);

  y++;
  items.add(new Label("DNS CC Server:"),
            new StringEditItem<std::string&>(40, b.dns_cc_server, EditLineMode::ALL), 1, y);

  y++;
  items.add(new Label("Blocked Countries:"),
            new SubDialogFunction<wwivd_blocking_t>(config, b, blocked_country_subdialog), 1, y);

  y++;
  items.add(new Label("Max Concurrent Sessions:"),
            new NumberEditItem<int>(&b.max_concurrent_sessions), 1, y);
  y += 2;
  items.add(new Label("Enable Auto Blocking?"), new BooleanEditItem(&b.auto_blocklist), 1, y);
  y++;
  items.add(new Label("Max Sessions Before Blocking:"),
            new NumberEditItem<int>(&b.auto_bl_sessions), 1, y);
  y++;
  items.add(new Label("Max Seconds Before Blocking:"), new NumberEditItem<int>(&b.auto_bl_seconds),
            1, y);

  items.relayout_items_and_labels();
  items.Run("Blocking Configuration");
}

static void edit_matrix_entry(const Config& config, wwivd_matrix_entry_t& b) {
  EditItems items{};
  char key[2] = {b.key, 0};
  {
    int y = 1;
    items.add(new Label("Key:"), new StringEditItem<char*>(1, key, EditLineMode::ALL), 1, y);
    y++;
    items.add(new Label("Name:"),
              new StringEditItem<std::string&>(12, b.name, EditLineMode::ALL), 1, y);
    y++;
    items.add(new Label("Description:"),
              new StringEditItem<std::string&>(52, b.description, EditLineMode::ALL), 1, y);
    y++;
    items.add(new Label("Working Dir:"),
              new StringFilePathItem(52, config.root_directory(), b.working_directory), 1, y);
    y++;
    items.add(new Label("Telnet Command:"),
              new StringEditItem<std::string&>(52, b.telnet_cmd, EditLineMode::ALL), 1, y);
    y++;
    items.add(new Label("SSH Command:"),
              new StringEditItem<std::string&>(52, b.ssh_cmd, EditLineMode::ALL), 1, y);
    y++;
    items.add(new Label("Require Ansi:"), new BooleanEditItem(&b.require_ansi), 1, y);
    y++;
    items.add(new Label("Start Node:"), new NumberEditItem<int>(&b.start_node), 1, y);
    y++;
    items.add(new Label("End Node:"), new NumberEditItem<int>(&b.end_node), 1, y);
    y++;
    items.add(new Label("Local Node:"), new NumberEditItem<int>(&b.local_node), 1, y);
  }

  items.relayout_items_and_labels();
  items.Run(StrCat("Matrix Config: ", b.name));
  // Need to update key since we have no single char edit item.
  b.key = key[0];
}

static void matrix_subdialog(const Config& config, wwivd_config_t& c, CursesWindow* window) {
  auto done = false;
  do {
    vector<ListBoxItem> items;
    for (const auto& e : c.bbses) {
      items.emplace_back(e.name);
    }
    ListBox list(window, "Select BBS", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
    auto result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (items.empty()) {
          break;
        }
        if (!dialog_yn(window, StrCat("Delete '", items[result.selected].text(), "' ?"))) {
          break;
        }
        erase_at(c.bbses, result.selected);
      } break;
      case 'I': {
        const auto name = dialog_input_string(window, "Enter BBS Name: ", 8);
        if (name.empty()) {
          break;
        }
        wwivd_matrix_entry_t e{};
        e.name = name;
        e.key = name.front();
        auto pos = result.selected;
        if (pos >= 0 && pos < ssize(items)) {
          insert_at(c.bbses, pos, e);
        } else {
          c.bbses.push_back(e);
        }
      } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      auto& b = c.bbses.at(result.selected);
      edit_matrix_entry(config, b);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);
}

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

bool SaveDaemonConfig(const wwiv::sdk::Config& config, wwivd_config_t& c) { return c.Save(config); }

wwivd_config_t LoadDaemonConfig(const wwiv::sdk::Config& config) {
  wwivd_config_t c{};
  if (!c.Load(config)) {
    c.binkp_port = -1;
    c.telnet_port =
#ifdef _WIN32
        23;
#else
        2323;
#endif
    c.http_port = 8080;
    c.http_address = "127.0.0.1";
    c.binkp_cmd = File::FixPathSeparators("./networkb --receive --handle=@H");
    c.network_callout_cmd = File::FixPathSeparators("./networkb --send --net=@T --node=@N");
    c.beginday_cmd = File::FixPathSeparators("./bbs -e");

    c.bbses.push_back(CreateWWIVMatrixEntry());
  } else {
    if (c.network_callout_cmd.empty()) {
      c.network_callout_cmd = File::FixPathSeparators("./networkb --send --net=@T --node=@N");
    }
    if (c.beginday_cmd.empty()) {
      c.beginday_cmd = File::FixPathSeparators("./bbs -e");
    }
    if (c.binkp_cmd.empty()) {
      c.binkp_cmd = File::FixPathSeparators("./networkb --receive --handle=@H");
    }
  }

  if (c.bbses.empty()) {
    // If we have no entries in the Matrix, then let's add
    // a default entry for WWIV.
    c.bbses.push_back(CreateWWIVMatrixEntry());
  }
  return c;
}

void wwivd_ui(const Config& config) {
  auto c = LoadDaemonConfig(config);

  EditItems items{};
  auto y = 1;
  items.add(new Label("Telnet Port:"),
            new NumberEditItem<int>(&c.telnet_port),
            "Telnet Server Port Number (or -1 to disable).", 1, y);
  y++;
  items.add(new Label("SSH Port:"),
            new NumberEditItem<int>(&c.ssh_port),
            "SSH Server Port Number (or -1 to disable).", 1, y);
  y++;
  items.add(new Label("Status Port:"),
            new NumberEditItem<int>(&c.http_port),
            "Used for BBS node status [/status] (or -1 to disable).", 1, y);
  y++;
  items.add(
      new Label("Status Address:"),
      new StringEditItem<std::string&>(16, c.http_address, EditLineMode::ALL),
      "Network address for the BBS node status HTTP Server.", 1, y);
  y++;
  items.add(new Label("BinkP Port:"),
            new NumberEditItem<int>(&c.binkp_port),
            "BINKP Server Port Number (or -1 to disable).", 1, y);
  y++;
  items.add(new Label("Launch Minimized:"),
            new BooleanEditItem(&c.launch_minimized),
            "Should wwivd launch bbs and network commands minimized (WIN32 Only)", 1, y);
  y++;
  items.add(new Label("Run BeginDay:"),
            new BooleanEditItem(&c.do_beginday_event),
            "Should wwivd execute the beginday event for WWIV.", 1, y);
  y++;
  items.add(
      new Label("BeginDay Cmd:"),
      new StringEditItem<std::string&>(52, c.beginday_cmd, EditLineMode::ALL),
      "Command to execute for the beginday event.", 1, y);
  y++;
  items.add(new Label("Net Callouts:"),
            new BooleanEditItem(&c.do_network_callouts),
            "Command to execute to perform a network callout.", 1, y);

  y++;
  items.add(new Label("Net Callout Cmd:"),
            new StringEditItem<std::string&>(52, c.network_callout_cmd,
                                             EditLineMode::ALL),
            "Command to execute to perform a network callout.", 1, y);
  y++;
  items.add(new Label("Net receive cmd:"),
            new StringEditItem<std::string&>(52, c.binkp_cmd, EditLineMode::ALL),
            "Command to execute for an inbound network request.", 1, y);
  y++;
  items.add(
      new Label("Matrix Filename:"),
      new StringEditItem<std::string&>(12, c.matrix_filename, EditLineMode::ALL),
      "Filename to display without extension before matrix bbs menu.", 1, y);
  y++;
  items.add(new Label("Matrix Settings:"),
            new SubDialogFunction<wwivd_config_t>(config, c, matrix_subdialog),
            "Create/Edit/Delete Matrix BBS settings.", 1, y);
  y++;
  items.add(
      new Label("Blocking:"),
      new SubDialogFunction<wwivd_blocking_t>(config, c.blocking, edit_blocking),
      "IP Blocking Settings.", 1, y);

  items.relayout_items_and_labels();
  items.Run("wwivd Configuration");
  if (!SaveDaemonConfig(config, c)) {
    messagebox(items.window(), "Error saving wwivd.json");
  }
}
