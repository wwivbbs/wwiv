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
// ReSharper disable CppAssignedValueIsNeverUsed
#include "sdk/net/networks.h"
#include "wwivconfig/networks.h"

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "sdk/fido/fido_callout.h"
#include "wwivconfig/subacc.h"
#include "wwivconfig/utility.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

constexpr ssize_t MAX_NETWORKS = 100;

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

static bool del_net(const Config& config, Networks& networks, int nn) {
  Subs subs(config.datadir(), networks.networks());
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
      for (auto i1 = 1; i1 <= GetNumMessagesInCurrentMessageArea(); i1++) {
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
  const auto nu = number_userrecs(config.datadir());
  for (auto i = 1; i <= nu; i++) {
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
  FidoNetworkConfigSubDialog(const Config& config, std::filesystem::path bbsdir,
                             net_networks_rec& d)
      : SubDialog(config, d), netdir_(std::move(bbsdir)) {}
  ~FidoNetworkConfigSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    EditItems items{};
    if (t_.type != network_type_t::ftn) {
      return;
    }
    constexpr int MAX_STRING_LEN = 56;
    auto* n = &t_.fido;
    auto y = 1;

    items.add(new Label("FTN Address:"),
              new FidoAddressStringEditItem(MAX_STRING_LEN, n->fido_address),
              "The FTN address for this system (example 21:9/123@fsxnet).", 1, y);
    ++y;
    items.add(new Label("Nodelist Base:"),
              new StringEditItem<std::string&>(MAX_STRING_LEN, n->nodelist_base, EditLineMode::ALL),
      "Base filename for the nodelist without path or extension. (e.g. FSXNET)", 1, y);
    ++y;
    items.add(new Label("Inbound Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->inbound_dir), 1, y);
    ++y;
    items.add(new Label("Temp In Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->temp_inbound_dir), 1, y);
    ++y;
    items.add(new Label("Temp Out Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->temp_outbound_dir), 1, y);
    ++y;
    items.add(new Label("Outbound Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->outbound_dir), 1, y);
    ++y;
    items.add(new Label("NetMail Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->netmail_dir), 1, y);
    ++y;
    items.add(new Label("BadPacket Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->bad_packets_dir), 1, y);
    ++y;
    items.add(new Label("TIC Dir:"), new StringFilePathItem(MAX_STRING_LEN, netdir_, n->tic_dir), 1,
              y);
    ++y;
    items.add(new Label("Unknown Dir:"),
              new StringFilePathItem(MAX_STRING_LEN, netdir_, n->unknown_dir), 1, y);
    ++y;
    items.add(new Label("Origin Line:"),
              new StringEditItem<std::string&>(MAX_STRING_LEN, n->origin_line, EditLineMode::ALL),
      "Origin line to add to outgoing echomail messages from this system", 
              1, y);
    ++y;
    dy_start_ = y;
    const std::vector<std::pair<fido_mailer_t, std::string>> mailerlist = {
        {fido_mailer_t::flo, "BSO (FLO) [Recommended]"},
        {fido_mailer_t::attach, "NetMail (ATTACH)"}};
    items.add(new Label("Mailer:"), new ToggleEditItem<fido_mailer_t>(mailerlist, &n->mailer_type),
              "Select BSO if using WWIV's Native BinkP.", 1, y);
    ++y;
    const std::vector<std::pair<fido_transport_t, std::string>> transportlist = {
        {fido_transport_t::directory, "Directory"}, {fido_transport_t::binkp, "WWIV BinkP"}};
    items.add(new Label("Transport:"),
              new ToggleEditItem<fido_transport_t>(transportlist, &n->transport),
              "This isn't currently used, but you likely want WWIV BinkP", 1, y);
    ++y;

    // dy_start
    auto dy = dy_start_;
    items.add(new Label("Process TIC  :"), new BooleanEditItem(&n->process_tic),
              "Process TIC files for this network.", 3, dy);
    ++dy;
    items.add(new Label("Cvt Hearts   :"), new BooleanEditItem(&n->wwiv_heart_color_codes),
              "Convert WWIV Heart codes into PIPE color codes.", 3, dy);
    ++dy;
    // HACK: We can't layout in columns with empty columns before the control.
    items.add(new Label(""), 1, dy);
    items.add(new Label(""), 2, dy);
    items.add(new Label("Cvt WWIV Pipe:"), new BooleanEditItem(&n->wwiv_pipe_color_codes),
              "Convert WWIV user color pipe codes into standard PIPE color codes.", 3, dy);
    ++dy;
    items.add(new Label(""), 1, dy);
    items.add(new Label(""), 2, dy);
    items.add(new Label("Allow Pipe Codes:"), new BooleanEditItem(&n->allow_any_pipe_codes),
              "Allow pipe codes, don't strip outbound PIPE codes.", 3, dy);
    ++dy;
    window->GotoXY(x_, y_);

    items.add_aligned_width_column(1);
    items.relayout_items_and_labels();
    items.Run(menu_label());
    window->RedrawWin();
  }

private:
  const std::filesystem::path netdir_;
  int dy_start_{0};
};

static void edit_fido_node_config(const FidoAddress& a, fido_node_config_t& n) {
  auto& p = n.packet_config;
  EditItems items{};
  const std::vector<std::pair<fido_packet_t, std::string>> packetlist = {
      {fido_packet_t::unset, "unset"}, {fido_packet_t::type2_plus, "FSC-0039 Type 2+"}};

  auto y = 1;
  auto& b = n.binkp_config;
  items.add(new Label("BinkP Host:"),
            new StringEditItem<std::string&>(40, b.host, EditLineMode::ALL),
            "BinkP hostname to override default from nodelist", 1, y);
  y++;
  items.add(new Label("BinkP Port:"), new NumberEditItem<int>(&b.port),
            "BinkP post number to override default from nodelist", 1, y);
  y++;
  items.add(new Label("Session PW:"),
            new StringEditItem<std::string&>(8, b.password, EditLineMode::UPPER_ONLY),
            "BinkP password to use when connection to this host.", 1, y);
  y += 2;

  items.add(new Label("Routes:"), new StringEditItem<std::string&>(40, n.routes, EditLineMode::ALL),
            "Systems who route this this host. i.e. 1:*", 1, y);

  ++y;
  items.add(new Label("Packet Type:"),
            new ToggleEditItem<fido_packet_t>(packetlist, &p.packet_type), 1, y);
  y++;
  items.add(new Label("Compression:"),
            new StringListItem({"ZIP", "ARC", "PKT", ""}, p.compression_type), 1, y);
  y++;
  items.add(new Label("Max Arc Size:"), new NumberEditItem<int>(&p.max_archive_size),
            "NOT IMPLEMENTED YET", 1, y);
  y++;
  items.add(new Label("Max Pkt Size:"), new NumberEditItem<int>(&p.max_packet_size),
            "NOT IMPLEMENTED YET", 1, y);

  const std::vector<std::pair<fido_bundle_status_t, std::string>> bundlestatuslist = {
      {fido_bundle_status_t::normal, "Normal"}, {fido_bundle_status_t::crash, "Crash"},
      {fido_bundle_status_t::direct, "Direct"}, {fido_bundle_status_t::immediate, "Immediate"},
      {fido_bundle_status_t::hold, "Hold"},
  };
  y++;
  items.add(new Label("Bundle Status:"),
            new ToggleEditItem<fido_bundle_status_t>(bundlestatuslist, &p.netmail_status),
            "Default bundle status to use when creating FTN Bundles", 1, y);
  y += 2;
  auto y2 = y;
  items.add(new Label("Packet PW:"),
            new StringEditItem<std::string&>(8, p.packet_password, EditLineMode::UPPER_ONLY),
            "Password to use in the FTN packet.", 1, y);
  y++;
  items.add(new Label("Tic PW:"),
            new StringEditItem<std::string&>(8, p.tic_password, EditLineMode::UPPER_ONLY),
            "If set, the password an incoming TIC file must use.", 1, y);
  y++;
  items.add(new Label("AreaFix PW:"),
            new StringEditItem<std::string&>(8, p.areafix_password, EditLineMode::UPPER_ONLY),
            "NOT IMPLEMENTED YET", 1, y);
  y = y2;

  auto& c = n.callout_config;
  items.add(new Label("Auto Callout:"), new BooleanEditItem(&c.auto_callouts),
            "Should wwivd automatically call out to this node?", 3, y);
  y++;
  items.add(new Label("Every N min:"),
            new NumberEditItem<decltype(c.call_every_x_minutes)>(&c.call_every_x_minutes),
            "Force a callout every N minutes.", 3, y);
  y++;
  items.add(new Label("Min K:"), new NumberEditItem<decltype(c.min_k)>(&c.min_k),
            "Attempt callout sooner if a packet is larger than this size in (K)ilobytes", 3, y);

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run(StrCat("Address: ", a.as_string(true, true)));
}

// Base item of an editable value, this class does not use templates.
class FidoPacketConfigSubDialog : public SubDialog<net_networks_rec> {
public:
  FidoPacketConfigSubDialog(const Config& config, std::filesystem::path bbsdir, net_networks_rec& t)
      : SubDialog(config, t), netdir_(std::move(bbsdir)) {}
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
      std::vector<ListBoxItem> items;
      for (const auto& e : callout.node_configs_map()) {
        items.emplace_back(e.first.as_string());
      }
      ListBox list(window, "Select Address", items);

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
          if (const auto o = try_parse_fidoaddr(items[result.selected].text())) {
            callout.erase(o.value());
          }
        } break;
        case 'I': {
          const std::string prompt = "Enter Address (Z:N/O) : ";
          const auto address_string = dialog_input_string(window, prompt, 20);
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
  int y = 1;

  EditItems items{};
  items.add(new Label("Password:"),
            new StringEditItem<std::string&>(20, c.session_password, EditLineMode::ALL),
            "WWIVnet password to use when connecting to this node", 1, y);

  y++;
  items.add(new Label("Allow Outbound Connections:"),
            new FlagEditItem<decltype(c.options)>(options_no_call, "No", "Yes", &c.options),
            "Is our system allowed to callout to this one?", 1, y);
  y += 2;
  items.add(new Label("Call every N minutes:"),
            new NumberEditItem<decltype(c.call_every_x_minutes)>(&c.call_every_x_minutes),
            "Automatically call out every N minutes", 1, y);

  y++;
  items.add(new Label("Call when minimum k waiting:"),
            new NumberEditItem<decltype(c.min_k)>(&c.min_k),
            "Automatically call out when K kilobytes of packet is pending", 1, y);
  y++;
  items.add(new Label("Call between hours of:"), new NumberEditItem<int8_t>(&c.min_hr),
            "Only call automatically between the house of X and Y", 1, y);
  items.add(new Label("and:"), new NumberEditItem<int8_t>(&c.max_hr),
            "Only call automatically between the house of X and Y", 3, y);
  y++;
  items.add(new Label("Hide from Pending List:"),
            new FlagEditItem<decltype(c.options)>(options_hide_pend, "Yes ", "No", &c.options),
            "Hide this node from the WFC pending system display", 1, y);

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run(StrCat("Node: @", c.sysnum, ".", net.name));
}

// Base item of an editable value, this class does not use templates.
class CalloutNetSubDialog final : public SubDialog<net_networks_rec> {
public:
  CalloutNetSubDialog(const Config& config, std::filesystem::path bbsdir, net_networks_rec& d)
      : SubDialog(config, d), netdir_(std::move(bbsdir)) {}
  ~CalloutNetSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    Callout callout(t_, config().max_backups());
    auto done = false;
    do {
      std::vector<ListBoxItem> items;
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
          const std::string prompt = "Enter Address (Node number only) : @";
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
        if (const auto node_with_at = items[result.selected].text(); node_with_at.size() > 1) {
          const auto node = to_number<uint16_t>(node_with_at.substr(1));
          if (const auto* c1 = callout.net_call_out_for(node); c1 != nullptr) {
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
  const std::vector<std::pair<network_type_t, std::string>> nettypes{
      {network_type_t::wwivnet, "WWIVnet "},
      {network_type_t::ftn, "Fido    "},
      {network_type_t::internet, "Internet"},
      {network_type_t::news, "Newsgroup (not supported yet)"}};

  Subs subs(config.datadir(), networks.networks());
  const auto subs_loaded = subs.Load();

  auto& n = wwiv::stl::at(networks, nn);
  const auto orig_network_name(n.name);

  auto y = 1;
  EditItems items{};
  items.add(new Label("Net Type:"), 1, y);
  items.add(new Label(nettypes.at(static_cast<int>(n.type)).second), 2, y)
      ->set_right_justified(false);
  y++;
  items.add(new Label("Net Name:"), new StringEditItem<std::string&>(15, n.name, EditLineMode::ALL),
            "Name of the network to use to display to bbs callers", 1, y);
  y++;
  items.add(new Label("Directory:"), new FileSystemFilePathItem(60, config.root_directory(), n.dir),
            1, y);
  y++;

  const auto net_dir = File::absolute(config.root_directory(), n.dir);
  if (n.type == network_type_t::ftn) {
    // skip over the node number
    items.add(new Label("Node #:"), 1, y);
    if (n.type == network_type_t::ftn) {
      items.add(new Label("N/A"), 2, y)->set_right_justified(false);
    }
    y++;
    items.add(new Label("Settings:"), new FidoNetworkConfigSubDialog(config, net_dir, n),
              "Edit settings for this network", 1, y);
    y++;
    items.add(new Label("Addresses:"), new FidoPacketConfigSubDialog(config, net_dir, n),
              "Configure settings for systems allowed to connect in this network", 1, y);
    y++;
  } else if (n.type == network_type_t::wwivnet) {
    items.add(new Label("Node #:"), new NumberEditItem<uint16_t>(&n.sysnum), "WWIVnet node number",
              1, y);
    y++;
    items.add(new Label("Callout.net:"), new CalloutNetSubDialog(config, net_dir, n), 
      "Edit the settings that define when to call out to a remote system", 1, y);
    y++;
  }

  const auto title = StrCat("Network Configuration: ", n.name, " [.", nn, "]");

  items.relayout_items_and_labels();
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
      std::vector<ListBoxItem> items;
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
        need_network3.insert(result.selected);
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

            const std::vector<std::string> nettypes{
                {"WWIVnet "}, {"Fido    "}, {"Internet"}, {"Newsgroup (not supported yet)"}};
            const auto net_type =
                input_select_item(window, "Select Network Type of (Q) to Quit: ", nettypes);
            if (net_type == 'Q') {
              continue;
            }
            if (dialog_yn(window, "Are you sure? ")) {
              insert_net(config, networks, net_num - 1,
                         static_cast<network_type_t>(net_type - '0'));
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
