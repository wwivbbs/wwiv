/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2022, WWIV Software Services           */
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
#include <string>

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


static void blocked_country_subdialog(const Config&, wwivd_blocking_t& b_, CursesWindow* window) {
  auto done = false;
  do {
    std::vector<ListBoxItem> items;
    for (const auto& e : b_.block_cc_countries) {
      items.emplace_back(std::to_string(e));
    }
    ListBox list(window, "Select Country Code", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
    if (const auto result = list.Run(); result.type == ListBoxResultType::HOTKEY) {
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
        if (const auto pos = result.selected; pos >= 0 && pos < size_int(items)) {
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
  items.add(new Label("Use goodip.txt?"), new BooleanEditItem(&b.use_goodip_txt), 
    "If enabled, use an allow-list in data/goodip.txt", 1, y);

  y++;
  items.add(new Label("Use badip.txt?"), new BooleanEditItem(&b.use_badip_txt), 
    "If enabled, use an block-list in data/badip.txt", 1, y);

  y++;
  items.add(new Label("Use CC Server?"), new BooleanEditItem(&b.use_dns_cc), 
    "Use a server to lookup country codes like (zz.countries.nerd.dk)", 1, y);

  y++;
  items.add(new Label("DNS CC Server:"),
            new StringEditItem<std::string&>(40, b.dns_cc_server, EditLineMode::ALL), 
    "DNS CC server to use (format must match that of zz.countries.nerd.dk)", 1, y);

  y++;
  items.add(new Label("Blocked Countries:"),
            new SubDialogFunction<wwivd_blocking_t>(config, b, blocked_country_subdialog), 
    "List of ISO 3166 country codes (numeric) to block", 1, y);

  y += 2;
  items.add(new Label("Enable Auto Blocking?"), new BooleanEditItem(&b.auto_blocklist), 
    "Enable auto-blocking rules", 1, y);
  y++;
  items.add(new Label("# connects needed to block:"),
            new NumberEditItem<int>(&b.auto_bl_sessions), 
    "Auto-block X connection within Y seconds", 1, y);
  items.add(new Label("Within seconds:"), new NumberEditItem<int>(&b.auto_bl_seconds),
            "Auto-block X connection within Y seconds", 3, y);
  y++;
  if (b.block_duration.empty()) {
    b.block_duration.emplace_back("15m");
  }
  if (b.block_duration.size() < 2) {
    b.block_duration.emplace_back("1h");
  }
  if (b.block_duration.size() < 3) {
    b.block_duration.emplace_back("1d");
  }
  if (b.block_duration.size() < 4) {
    b.block_duration.emplace_back("30d");
  }
  items.add(new Label("Blocking Time #1:"),
            new StringEditItem<std::string&>(4, b.block_duration[0], EditLineMode::LOWER), 
    "Amount of time to block when banning for the nth time", 1, y);
  items.add(new Label("#2:"),
            new StringEditItem<std::string&>(4, b.block_duration[1], EditLineMode::LOWER), 
    "Amount of time to block when banning for the nth time", 3, y);
  items.add(new Label("#3:"),
            new StringEditItem<std::string&>(4, b.block_duration[2], EditLineMode::LOWER), 
    "Amount of time to block when banning for the nth time", 5, y);
  items.add(new Label("#4:"),
            new StringEditItem<std::string&>(4, b.block_duration[3], EditLineMode::LOWER), 
    "Amount of time to block when banning for the nth time", 7, y);
  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run("Blocking Configuration");
}

static void edit_matrix_entry(const Config& config, wwivd_matrix_entry_t& b) {
  EditItems items{};
  char key[2] = {b.key, 0};

  const std::vector<std::pair<wwiv::sdk::wwivd_data_mode_t, std::string>> data_modes = {
      {wwivd_data_mode_t::socket, "Socket"},
      {wwivd_data_mode_t::pipe, "Named Pipe"}};

  {
    int y = 1;
    items.add(new Label("Key:"), 
      new StringEditItem<char*>(1, key, EditLineMode::ALL),
      "Key to press to select this bbs", 1, y);
    y++;
    items.add(new Label("Name:"), new StringEditItem<std::string&>(12, b.name, EditLineMode::ALL),
              "Name of the BBS (i.e WWIV)", 1, y);
    y++;
    items.add(new Label("Description:"),
              new StringEditItem<std::string&>(52, b.description, EditLineMode::ALL),
              "Longer description for this BBS", 1, y);
    y++;
    items.add(new Label("Working Dir:"),
              new StringFilePathItem(52, config.root_directory(), b.working_directory), 
              "Directory to set as working directory when launching the BBS", 1, y);
    y++;
    items.add(new Label("Telnet Command:"),
              new StringEditItem<std::string&>(52, b.telnet_cmd, EditLineMode::ALL), 
              "Commandline to use when launching the BBS for a telnet connection", 1, y);
    y++;
    items.add(new Label("SSH Command:"),
              new StringEditItem<std::string&>(52, b.ssh_cmd, EditLineMode::ALL), 
              "Commandline to use when launching the BBS for a SSH connection", 1, y);
    y++;
    items.add(new Label("Require Ansi:"), new BooleanEditItem(&b.require_ansi), 
              "Does this BBS require ANSI support", 1, y);
    y++;
    items.add(new Label("Start Node:"), new NumberEditItem<int>(&b.start_node), 
              "Starting node number to use for telnet/SSH connections", 1, y);
    y++;
    items.add(new Label("End Node:"), new NumberEditItem<int>(&b.end_node), 
              "Ending node number to use for telnet/SSH connections", 1, y);
    y++;
    items.add(new Label("Local Node:"), new NumberEditItem<int>(&b.local_node), 
              "node number to use for local BBS session", 1, y);
    y++;
    items.add(new Label("Data Mode:"),
              new ToggleEditItem<wwiv::sdk::wwivd_data_mode_t>(data_modes, &b.data_mode),
              "The way to communicate from wwivd to bbs (default is socket handle)", 1, y);
    y++;
    items.add(new Label("WWIV BBS:"), new BooleanEditItem(&b.wwiv_bbs),
      "Is this the primary WWIV BBS for this WWIVD.", 1, y);
  }

  items.relayout_items_and_labels();
  items.Run(StrCat("Matrix Config: ", b.name));
  // Need to update key since we have no single char edit item.
  b.key = key[0];
}

static void matrix_subdialog(const Config& config, wwivd_config_t& c, CursesWindow* window) {
  auto done = false;
  do {
    std::vector<ListBoxItem> items;
    for (const auto& e : c.bbses) {
      items.emplace_back(e.name);
    }
    ListBox list(window, "Select BBS", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
    if (auto result = list.Run(); result.type == ListBoxResultType::HOTKEY) {
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
        if (auto pos = result.selected; pos >= 0 && pos < size_int(items)) {
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
  e.require_ansi = false;
  e.start_node = 1;
  e.end_node = 3;
  e.local_node = 4;
#if defined(__OS2__)
  e.working_directory = File::current_directory().string();
  e.telnet_cmd = File::FixPathSeparators("bbs.exe -XP -N@N");
  e.ssh_cmd = "";
  e.data_mode = wwivd_data_mode_t::pipe;
#else
  e.telnet_cmd = File::FixPathSeparators("./bbs -XT -H@H -N@N");
  e.ssh_cmd = File::FixPathSeparators("./bbs -XS -H@H -N@N");
  e.data_mode = wwivd_data_mode_t::socket;
#endif
  return e;
}

bool SaveDaemonConfig(const wwiv::sdk::Config& config, wwivd_config_t& c) { return c.Save(config); }

wwivd_config_t LoadDaemonConfig(const wwiv::sdk::Config& config) {
  wwivd_config_t c{};
  if (!c.Load(config)) {
    c.binkp_port = -1;
    c.ssh_port = 22;
    c.telnet_port = 23;
#if defined(__unix__)
    c.telnet_port = 2323;
#elif defined(__OS2__)
    c.ssh_port = -1;
#endif
    c.http_port = 59948; // 5WWIV on the telephone keypad.
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
  items.add(new Label("SSH Port:"),
            new NumberEditItem<int>(&c.ssh_port),
            "SSH Server Port Number (or -1 to disable).", 3, y);
  y++;
  items.add(
      new Label("Status Address:"),
      new StringEditItem<std::string&>(16, c.http_address, EditLineMode::ALL),
      "Network address for the BBS node status HTTP Server.", 1, y);
  items.add(new Label("Status Port:"),
            new NumberEditItem<int>(&c.http_port),
            "Used for BBS node status [/status] (or -1 to disable).", 3, y);
  y++;
  items.add(new Label("BinkP Port:"),
            new NumberEditItem<int>(&c.binkp_port),
            "BINKP Server Port Number (or -1 to disable).", 1, y);
  y++;
  items.add(new Label("Fake Mailer?"), new BooleanEditItem(&c.blocking.mailer_mode), 
    "Emulate legacy FTN mailer with a 'Press <ESC> for BBS' prompt as a captcha.", 1, y);
  y++;
  items.add(new Label("Max Concurrent:"),
            new NumberEditItem<int>(&c.blocking.max_concurrent_sessions), 
    "Maximum number of concurrent sessions allowed per IP address.", 1, y);
  y+=2;
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

  y+=2;
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
  y+=2;
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

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run("wwivd Configuration");
  if (!SaveDaemonConfig(config, c)) {
    messagebox(items.window(), "Error saving wwivd.json");
  }
}
