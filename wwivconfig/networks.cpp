/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
// ReSharper disable CppAssignedValueIsNeverUsed
#include "wwivconfig/networks.h"

#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/filenames.h"
#include "sdk/net/networks.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "wwivconfig/subacc.h"
#include "wwivconfig/utility.h"
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

constexpr ssize_t MAX_NETWORKS = 100;

using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

static bool del_net(const Config& config, Networks& networks, int nn) {
  wwiv::sdk::Subs subs(config.datadir(), networks.networks());
  if (!subs.Load()) {
    return false;
  }
  if (subs.subs().empty()) {
    return false;
  }

  // TODO(rushfan): Ensure that no subs are using it.
  for (auto i = 0; i < wwiv::stl::ssize(subs.subs()); i++) {
    int i2;
    for (i2 = 0; i2 < i; i2++) {
      if (subs.sub(i).filename == subs.sub(i2).filename) {
        break;
      }
    }
    if (i2 >= i) {
      iscan1(i, subs, config);
      open_sub(true);
      for (int i1 = 1; i1 <= GetNumMessagesInCurrentMessageArea(); i1++) {
        auto p = get_post(i1);
        if (!p) {
          continue;
        }
        if (!(p->status & status_post_new_net)) {
          continue;
        }
        if (p->network.network_msg.net_number == nn) {
          p->network.network_msg.net_number = static_cast<uint8_t>('\xff');
        } else if (p->network.network_msg.net_number > nn) {
          p->network.network_msg.net_number--;
        }
        write_post(i1, p.value());
      }
      close_sub();
    }
  }

  // Now we update the email.
  File emailfile(FilePath(config.datadir(), EMAIL_DAT));
  if (emailfile.Open(File::modeBinary | File::modeReadWrite)) {
    auto t = static_cast<int>(emailfile.length() / sizeof(mailrec));
    for (auto r = 0; r < t; r++) {
      mailrec m = {};
      emailfile.Seek(r * sizeof(mailrec), File::Whence::begin);
      emailfile.Read(&m, sizeof(mailrec));
      if (((m.tosys != 0) || (m.touser != 0)) && m.fromsys) {
        if (strnlen(m.title, WWIV_MESSAGE_TITLE_LENGTH) >= WWIV_MESSAGE_TITLE_LENGTH) {
          // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
          m.title[WWIV_MESSAGE_TITLE_LENGTH - 1] = 0;
        }
        m.status |= status_new_net;
        if (m.status & status_source_verified) {
          if (m.network.src_verified_msg.net_number == nn) {
            m.network.src_verified_msg.net_number = static_cast<uint8_t>('\xff');
          } else if (m.network.src_verified_msg.net_number > nn) {
            m.network.src_verified_msg.net_number--;
          }
        } else {
          if (m.network.network_msg.net_number == nn) {
            m.network.network_msg.net_number = static_cast<uint8_t>('\xff');
          } else if (m.network.network_msg.net_number > nn) {
            m.network.network_msg.net_number--;
          }
        }
        emailfile.Seek(r * sizeof(mailrec), File::Whence::begin);
        emailfile.Write(&m, sizeof(mailrec));
      }
    }
  }

  // Update the user
  userrec u{};
  const int nu = number_userrecs(config.datadir());
  for (int i = 1; i <= nu; i++) {
    read_user(config, i, &u);
    if (u.net_num == nn) {
      u.forwardsys = u.forwardusr = u.net_num = 0;
      write_user(config, i, &u);
    } else if (u.net_num > nn) {
      u.net_num--;
      write_user(config, i, &u);
    }
  }

  // Finally delete it from networks.dat
  networks.erase(nn);
  return networks.Save();
}

// Base item of an editable value, this class does not use templates.
class FidoNetworkConfigSubDialog : public SubDialog<net_networks_rec> {
public:
  FidoNetworkConfigSubDialog(const Config& config,
                             std::filesystem::path bbsdir, int x, int y,
                             net_networks_rec& d)
      : SubDialog(config, x, y, d), netdir_(std::move(bbsdir)) {}
  ~FidoNetworkConfigSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    EditItems items{};
    if (t_.type != network_type_t::ftn) {
      return;
    }
    constexpr int LABEL_WIDTH = 14;
    constexpr int SHORT_FIELD_WIDTH = 25;
    constexpr int LBL1_POSITION = 2;
    constexpr int COL1_POSITION = LBL1_POSITION + LABEL_WIDTH + 1;
    constexpr int LBL2_POSITION = COL1_POSITION + SHORT_FIELD_WIDTH;
    constexpr int COL2_POSITION = LBL2_POSITION + LABEL_WIDTH + 1;
    constexpr int MAX_STRING_LEN = 56;
    auto* n = &t_.fido;
    auto y = 1;

    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN,
                                               n->fido_address, EditLineMode::ALL));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN,
                                               n->nodelist_base, EditLineMode::ALL));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->inbound_dir));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->temp_inbound_dir));
    items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_,
                                     n->temp_outbound_dir));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->outbound_dir));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->netmail_dir));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->bad_packets_dir));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->tic_dir));
    items.add(
        new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, netdir_, n->unknown_dir));
    items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN, n->origin_line,
                                               EditLineMode::ALL));

    dy_start_ = y;
    const vector<pair<fido_mailer_t, string>> mailerlist = {
        {fido_mailer_t::flo, "BSO (FLO) [Recommended]"}, {fido_mailer_t::attach, "NetMail (ATTACH)"}};
    items.add(new ToggleEditItem<fido_mailer_t>(COL1_POSITION, y++, mailerlist, &n->mailer_type))
        ->set_help_text("Select BSO if using WWIV's Native BinkP.");

    const vector<pair<fido_transport_t, string>> transportlist = {
        {fido_transport_t::directory, "Directory"},
        {fido_transport_t::binkp, "WWIV BinkP"}};
    items.add(new ToggleEditItem<fido_transport_t>(COL1_POSITION, y++, transportlist,
                                                  &n->transport))
        ->set_help_text("This isn't currently used, but you likely want WWIV BinkP");

    // dy_start
    int dy = dy_start_;
    items.add(new BooleanEditItem(COL2_POSITION, dy++, &n->process_tic))
        ->set_help_text("Process TIC files for this network.");
    items.add(new BooleanEditItem(COL2_POSITION, dy++, &n->wwiv_heart_color_codes))
        ->set_help_text("Convert WWIV Heart codes into PIPE color codes.");
    items.add(new BooleanEditItem(COL2_POSITION, dy++, &n->wwiv_pipe_color_codes))
        ->set_help_text("Convert WWIV user color pipe codes into standard PIPE color codes.");

    window->GotoXY(x_, y_);
    y = 1;
    items.add_labels({new Label(LBL1_POSITION, y++, LABEL_WIDTH, "FTN Address:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Nodelist Base:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Inbound Dir:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Temp In Dir:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Temp Out Dir:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Outbound Dir:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "NetMail Dir:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "BadPacket Dir:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "TIC Dir      :"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Unknown Dir  :"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Origin Line:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Mailer:"),
                      new Label(LBL1_POSITION, y++, LABEL_WIDTH, "Transport:")});

    // dy = y where we start to double up.
    dy = dy_start_;
    items.add_labels({new Label(LBL2_POSITION, dy++, LABEL_WIDTH, "Process TIC  :"),
                      new Label(LBL2_POSITION, dy++, LABEL_WIDTH, "Cvt Hearts   :"),
                      new Label(LBL2_POSITION, dy++, LABEL_WIDTH, "Cvt WWIV Pipe:")
    });
    items.Run(menu_label());
    window->RedrawWin();
  }

private:
  const std::filesystem::path netdir_;
  int dy_start_{0};
};

static void edit_fido_node_config(const FidoAddress& a, fido_node_config_t& n) {
  constexpr int LBL1_POSITION = 2;
  constexpr int LABEL_WIDTH = 30;
  constexpr int COL1_POSITION = LBL1_POSITION + LABEL_WIDTH + 1;

  auto& p = n.packet_config;
  EditItems items{};
  const vector<pair<fido_packet_t, string>> packetlist = {
      {fido_packet_t::unset, "unset"}, {fido_packet_t::type2_plus, "FSC-0039 Type 2+"}};

  auto y = 1;
  auto& b = n.binkp_config;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "BinkP Host:"),
           new StringEditItem<std::string&>(COL1_POSITION, y, 40, b.host, EditLineMode::ALL))
      ->set_help_text("BinkP hostname to override default from nodelist");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "BinkP Port:"),
           new NumberEditItem<int>(COL1_POSITION, y, &b.port))
      ->set_help_text("BinkP post number to override default from nodelist");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Session PW:"),
           new StringEditItem<std::string&>(COL1_POSITION, y, 8, b.password,
                                            EditLineMode::UPPER_ONLY))
      ->set_help_text("BinkP password to use when connection to this host.");
  y += 2;

  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Routes:"),
           new StringEditItem<std::string&>(COL1_POSITION, y, 40, n.routes, EditLineMode::ALL))
      ->set_help_text("Systems who route this this host. i.e. 1:*");

  ++y;
  items.add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Packet Type:"),
            new ToggleEditItem<fido_packet_t>(COL1_POSITION, y, packetlist, &p.packet_type));
  y++;
  items.add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Compression:"),
            new StringListItem(COL1_POSITION, y, {"ZIP", "ARC", "PKT", ""}, p.compression_type));
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Packet PW:"),
           new StringEditItem<std::string&>(COL1_POSITION, y, 8, p.packet_password,
                                            EditLineMode::UPPER_ONLY))
      ->set_help_text("Password to use in the FTN packet.");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "AreaFix PW:"),
           new StringEditItem<std::string&>(COL1_POSITION, y, 8, p.areafix_password,
                                            EditLineMode::UPPER_ONLY))
      ->set_help_text("NOT IMPLEMENTED YET");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Max Arc Size:"),
           new NumberEditItem<int>(COL1_POSITION, y, &p.max_archive_size))
      ->set_help_text("NOT IMPLEMENTED YET");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Max Pkt Size:"),
           new NumberEditItem<int>(COL1_POSITION, y, &p.max_packet_size))
      ->set_help_text("NOT IMPLEMENTED YET");

  const vector<pair<fido_bundle_status_t, string>> bundlestatuslist = {
      {fido_bundle_status_t::normal, "Normal"}, {fido_bundle_status_t::crash, "Crash"},
      {fido_bundle_status_t::direct, "Direct"}, {fido_bundle_status_t::immediate, "Immediate"},
      {fido_bundle_status_t::hold, "Hold"},
  };
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Bundle Status:"),
           new ToggleEditItem<fido_bundle_status_t>(COL1_POSITION, y, bundlestatuslist,
                                                    &p.netmail_status))
      ->set_help_text("Default bundle status to use when creating FTN Bundles");
  y += 2;
  auto& c = n.callout_config;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Automatic Callouts:"),
           new BooleanEditItem(COL1_POSITION, y, &c.auto_callouts))
      ->set_help_text("Should wwivd automatically call out to this node?");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Call every N minutes:"),
           new NumberEditItem<decltype(c.call_every_x_minutes)>(COL1_POSITION, y,
                                                                &c.call_every_x_minutes))
      ->set_help_text("Frequency in minutes between callout attempts");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Call when minimum k waiting:"),
           new NumberEditItem<decltype(c.min_k)>(COL1_POSITION, y, &c.min_k))
      ->set_help_text("Attempt callout sooner if this many k of packet is waiting");

  items.Run(StrCat("Address: ", a.as_string()));
}

// Base item of an editable value, this class does not use templates.
class FidoPacketConfigSubDialog : public SubDialog<net_networks_rec> {
public:
  FidoPacketConfigSubDialog(const Config& config, std::filesystem::path bbsdir, int x, int y,
                            net_networks_rec& t)
      : SubDialog(config, x, y, t), netdir_(std::move(bbsdir)) {}
  ~FidoPacketConfigSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    window->GotoXY(x_, y_);
    FidoCallout callout(config(), t_);
    if (!callout.IsInitialized()) {
      messagebox(window, "Unable to initialize fido_callout.json.");
      return;
    }
    auto done = false;
    do {
      vector<ListBoxItem> items;
      for (const auto& e : callout.node_configs_map()) {
        items.emplace_back(e.first.as_string());
      }
      ListBox list(window, "Select Address", items);

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
          FidoAddress a(items[result.selected].text());
          callout.erase(a);
        } break;
        case 'I': {
          const string prompt = "Enter Address (Z:N/O) : ";
          const string address_string = dialog_input_string(window, prompt, 20);
          if (address_string.empty()) {
            break;
          }
          FidoAddress address(address_string);
          fido_node_config_t config{};
          config.binkp_config.port = 24554;
          edit_fido_node_config(address, config);
          callout.insert(address, config);
        } break;
        }
      } else if (result.type == ListBoxResultType::SELECTION) {
        const auto address_string = wwiv::stl::at(items, result.selected).text();
        FidoAddress address(address_string);
        auto c = callout.fido_node_config_for(address);
        edit_fido_node_config(address, c);
        callout.insert(address, c);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
    } while (!done);
    callout.Save();
  }

private:
  const std::filesystem::path netdir_;
  const std::string title_;
};

static void edit_wwivnet_node_config(const net_networks_rec& net, net_call_out_rec& c) {
  constexpr int LBL1_POSITION = 2;
  constexpr int LABEL_WIDTH = 30;
  constexpr int COL1_POSITION = LBL1_POSITION + LABEL_WIDTH + 1;
  constexpr int LBL2_POSITION = COL1_POSITION + 4 + 1;
  constexpr int LABEL2_WIDTH = 4;
  constexpr int COL2_POSITION = LBL2_POSITION + 1 + LABEL2_WIDTH;
  int y = 1;

  EditItems items{};
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Password:"),
           new StringEditItem<std::string&>(COL1_POSITION, y, 20, c.session_password,
                                            EditLineMode::ALL))
      ->set_help_text("WWIVnet password to use when connecting to this node");

  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Allow Outbound Connections:"),
           new FlagEditItem<decltype(c.options)>(COL1_POSITION, y, options_no_call, "No", "Yes",
                                                 &c.options))
      ->set_help_text("Is our system allowed to callout to this one?");
  y += 2;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Call every N minutes:"),
           new NumberEditItem<decltype(c.call_every_x_minutes)>(COL1_POSITION, y,
                                                                &c.call_every_x_minutes))
      ->set_help_text("Automatically call out every N minutes");

  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Call when minimum k waiting:"),
           new NumberEditItem<decltype(c.min_k)>(COL1_POSITION, y, &c.min_k))
      ->set_help_text("Automatically call out when K kilobytes of packet is pending");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Call between hours of:"),
           new NumberEditItem<int8_t>(COL1_POSITION, y, &c.min_hr))
      ->set_help_text("Only call automatically between the house of X and Y");
  items
      .add(new Label(LBL2_POSITION, y, LABEL2_WIDTH, "and:"),
           new NumberEditItem<int8_t>(COL2_POSITION, y, &c.max_hr))
      ->set_help_text("Only call automatically between the house of X and Y");
  y++;
  items
      .add(new Label(LBL1_POSITION, y, LABEL_WIDTH, "Hide from Pending List:"),
           new FlagEditItem<decltype(c.options)>(COL1_POSITION, y, options_hide_pend, "Yes ", "No",
                                                 &c.options))
      ->set_help_text("Hide this node from the WFC pending system display");

  items.Run(StrCat("Node: @", c.sysnum, ".", net.name));
}

// Base item of an editable value, this class does not use templates.
class CalloutNetSubDialog final : public SubDialog<net_networks_rec> {
public:
  CalloutNetSubDialog(const Config& config, std::filesystem::path bbsdir, int x, int y,
                      net_networks_rec& d)
      : SubDialog(config, x, y, d), netdir_(std::move(bbsdir)) {}
  ~CalloutNetSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    Callout callout(t_, config().max_backups());
    auto done = false;
    do {
      vector<ListBoxItem> items;
      for (const auto& e : callout.callout_config()) {
        items.emplace_back(StrCat("@", e.first));
      }
      ListBox list(window, "Select Address", items);

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
          const auto node_with_at = items[result.selected].text();
          if (node_with_at.size() > 1) {
            const auto node = to_number<uint16_t>(node_with_at.substr(1));
            callout.erase(node);
          }
        } break;
        case 'I': {
          const string prompt = "Enter Address (Node number only) : @";
          const auto address_string = dialog_input_string(window, prompt, 20);
          if (address_string.empty()) {
            break;
          }
          auto node_number = to_number<uint16_t>(address_string);
          if (0 == node_number) {
            break;
          }
          net_call_out_rec config{};
          config.sysnum = node_number;
          edit_wwivnet_node_config(t_, config);
          callout.insert(node_number, config);
        } break;
        }
      } else if (result.type == ListBoxResultType::SELECTION) {
        const auto node_with_at = items[result.selected].text();
        if (node_with_at.size() > 1) {
          const auto node = to_number<uint16_t>(node_with_at.substr(1));
          const auto* c1 = callout.net_call_out_for(node);
          if (c1 != nullptr) {
            auto c = *c1;
            edit_wwivnet_node_config(t_, c);
            callout.insert(node, c);
          }
        }
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
    } while (!done);
    callout.Save();
  }

private:
  const std::filesystem::path netdir_;
};

static void edit_net(const Config& config, Networks& networks, int nn) {
  static const vector<pair<network_type_t, string>> nettypes{
      {network_type_t::wwivnet, "WWIVnet "},
      {network_type_t::ftn, "Fido    "},
      {network_type_t::internet, "Internet"},
      {network_type_t::news, "Newsgroup (not supported yet)"}};

  Subs subs(config.datadir(), networks.networks());
  const auto subs_loaded = subs.Load();

  auto& n = wwiv::stl::at(networks, nn);
  const auto orig_network_name(n.name);

  constexpr auto LABEL1_POSITION = 2;
  constexpr auto LABEL_WIDTH = 11;
  constexpr auto COL1_POSITION = LABEL1_POSITION + LABEL_WIDTH + 1;
  auto y = 1;
  EditItems items{};
  auto net_type_pos = y++;
  auto node_number_pos = 0;
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 15, n.name, EditLineMode::ALL));
  items.add(new FileSystemFilePathItem(COL1_POSITION, y++, 60, config.root_directory(), n.dir));

  const auto net_dir = File::absolute(config.root_directory(), n.dir);
  if (n.type == network_type_t::ftn) {
    // skip over the node number
    node_number_pos = y++;
    items.add(
        new FidoNetworkConfigSubDialog(config, net_dir, COL1_POSITION, y++, n));
    items.add(
        new FidoPacketConfigSubDialog(config, net_dir, COL1_POSITION, y++, n));
  } else if (n.type == network_type_t::wwivnet) {
    items.add(new NumberEditItem<uint16_t>(COL1_POSITION, y++, &n.sysnum))
      ->set_help_text("WWIVnet node number");
    items.add(new Label(LABEL1_POSITION, y, LABEL_WIDTH, "Callout.net:"),
              new CalloutNetSubDialog(config, net_dir, COL1_POSITION, y, n));
  }

  y = 1;
  items.add_labels({new Label(LABEL1_POSITION, y++, LABEL_WIDTH, "Net Type:"),
                    new Label(LABEL1_POSITION, y++, LABEL_WIDTH, "Net Name:"),
                    new Label(LABEL1_POSITION, y++, LABEL_WIDTH, "Directory:"),
                    new Label(LABEL1_POSITION, y++, LABEL_WIDTH, "Node #:")
  });
  if (n.type == network_type_t::ftn) {
    items.add_labels({new Label(LABEL1_POSITION, y++, LABEL_WIDTH, "Settings:"),
                      new Label(LABEL1_POSITION, y++, LABEL_WIDTH, "Addresses:")});
  }

  const auto title = StrCat("Network Configuration: ", n.name, " [.", nn, "]");
  items.create_window(title);
  items.window()->PutsXY(COL1_POSITION, net_type_pos, nettypes.at(static_cast<int>(n.type)).second);
  if (n.type == network_type_t::ftn) {
    items.window()->PutsXY(COL1_POSITION, node_number_pos, "N/A");
  }
  items.Run(title);

  if (subs_loaded && orig_network_name != n.name) {
    subs.Save();
  }

  networks.Save();
}

static bool insert_net(const Config& config, Networks& networks, int nn, network_type_t type) {
  Subs subs(config.datadir(), networks.networks());
  if (!subs.Load()) {
    return false;
  }
  if (subs.subs().empty()) {
    return false;
  }

  for (auto i = 0; i < wwiv::stl::ssize(subs.subs()); i++) {
    auto i2 = 0;
    for (i2 = 0; i2 < i; i2++) {
      if (subs.sub(i).filename == subs.sub(i2).filename) {
        break;
      }
    }
    if (i2 >= i) {
      iscan1(i, subs, config);
      open_sub(true);
      for (auto i1 = 1; i1 <= GetNumMessagesInCurrentMessageArea(); i1++) {
        auto p = get_post(i1);
        if (!p) {
          continue;
        }
        if (p->status & status_post_new_net) {
          if (p->network.network_msg.net_number >= nn) {
            p->network.network_msg.net_number++;
            write_post(i1, p.value());
          }
        }
      }
      close_sub();
    }
  }

  // same as del_net, don't think we need to do this here.
  // wwiv::sdk::write_subs(config.datadir(), subboards);
  File emailfile(FilePath(config.datadir(), EMAIL_DAT));
  if (emailfile.Open(File::modeBinary | File::modeReadWrite)) {
    auto t = static_cast<int>(emailfile.length() / sizeof(mailrec));
    for (auto r = 0; r < t; r++) {
      mailrec m{};
      emailfile.Seek(sizeof(mailrec) * r, File::Whence::begin);
      emailfile.Read(&m, sizeof(mailrec));
      if ((m.tosys != 0 || m.touser != 0) && m.fromsys) {
        if (strlen(m.title) >= WWIV_MESSAGE_TITLE_LENGTH) {
          // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
          m.title[71] = 0;
        }
        m.status |= status_new_net;
        if (m.status & status_source_verified) {
          if (m.network.src_verified_msg.net_number >= nn) {
            m.network.src_verified_msg.net_number++;
          }
        } else {
          if (m.network.network_msg.net_number >= nn) {
            m.network.network_msg.net_number++;
          }
        }
        emailfile.Seek(sizeof(mailrec) * r, File::Whence::begin);
        emailfile.Write(&m, sizeof(mailrec));
      }
    }
  }

  userrec u{};
  auto nu = number_userrecs(config.datadir());
  for (auto i = 1; i <= nu; i++) {
    read_user(config, i, &u);
    if (u.net_num >= nn) {
      u.net_num++;
      write_user(config, i, &u);
    }
  }

  {
    net_networks_rec n{};
    n.type = type;
    if (type == network_type_t::ftn) {
      n.sysnum = 1;
      n.name = "New FTNNet";
      auto& f = n.fido;
      f.bad_packets_dir = File::EnsureTrailingSlash("badpackets");
      f.inbound_dir = File::EnsureTrailingSlash("in");
      f.netmail_dir = File::EnsureTrailingSlash("netmail");
      f.outbound_dir = File::EnsureTrailingSlash("out");
      f.tic_dir = File::EnsureTrailingSlash("tic");
      f.temp_outbound_dir = File::EnsureTrailingSlash("tempout");
      f.temp_inbound_dir = File::EnsureTrailingSlash("tempin");
      f.unknown_dir = File::EnsureTrailingSlash("unknown");
    } else if (type == network_type_t::wwivnet) {
      n.name = "New WWIVnet";
    }
    n.dir = File::EnsureTrailingSlash("newnet.dir");
    networks.insert(nn, n);
  }

  edit_net(config, networks, nn);
  return true;
}

void networks(const wwiv::sdk::Config& config, std::set<int>& need_network3) {
  try {
    Networks networks(config);

    bool done = false;
    do {
      vector<ListBoxItem> items;
      int num = 0;
      for (const auto& n : networks.networks()) {
        items.emplace_back(fmt::sprintf("@%-5u %-16s [.%d]", n.sysnum, n.name, num++));
      }
      auto* window = curses_out->window();
      ListBox list(window, "Select Network", items);

      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("DI");
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
      auto result = list.Run();

      if (result.type == ListBoxResultType::SELECTION) {
        edit_net(config, networks, result.selected);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
        switch (result.hotkey) {
        case 'D':
          if (networks.networks().size() > 1) {
            const auto prompt = fmt::format("Delete '{}'", networks.at(result.selected).name);
            auto yn = dialog_yn(window, prompt);
            if (yn) {
              yn = dialog_yn(window, "Are you REALLY sure? ");
              if (yn) {
                del_net(config, networks, result.selected);
              }
            }
          } else {
            messagebox(window, "You must leave at least one network.");
          }
          break;
        case 'I':
          if (networks.networks().size() >= MAX_NETWORKS) {
            messagebox(window, "Too many networks.");
            break;
          }
          const auto prompt =
              fmt::format("Insert before which (1-{}) ? ", networks.networks().size() + 1);
          const auto net_num =
              dialog_input_number(window, prompt, 1, wwiv::stl::size_int(networks.networks()) + 1);
          if (net_num > 0 && net_num <= wwiv::stl::ssize(networks.networks()) + 1) {

            static const vector<string> nettypes{
                {"WWIVnet "},
                {"Fido    "},
                {"Internet"},
                {"Newsgroup (not supported yet)"}};
            const auto net_type =
                input_select_item(window, "Select Network Type of (Q) to Quit: ", nettypes);
            if (net_type == 'Q') {
              continue;
            }
            if (dialog_yn(window, "Are you sure? ")) {
              insert_net(config, networks, net_num - 1, static_cast<network_type_t>(net_type - '0'));
              need_network3.insert(net_num - 1);
            }
          }
          break;
        }
      }
    } while (!done);
    networks.Save();
  } catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }
}
