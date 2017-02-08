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
#include "init/networks.h"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>
#include <curses.h>

#include "bbs/keycodes.h"
#include "init/init.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/scope_exit.h"
#include "core/wwivport.h"
#include "core/file.h"
#include "init/subacc.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "init/subacc.h"
#include "sdk/filenames.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/networks.h"
#include "sdk/subxtr.h"

#define UINT(u,n)  (*((int  *)(((char *)(u))+(n))))
#define UCHAR(u,n) (*((char *)(((char *)(u))+(n))))

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
  for (size_t i = 0; i < subs.subs().size(); i++) {
    size_t i2;
    for (i2 = 0; i2 < i; i2++) {
      if (subs.sub(i).filename == subs.sub(i2).filename) {
        break;
      }
    }
    if (i2 >= i) {
      iscan1(i, subs, config);
      open_sub(true);
      for (int i1 = 1; i1 <= GetNumMessagesInCurrentMessageArea(); i1++) {
        postrec* p = get_post(i1);
        if (p->status & status_post_new_net) {
          if (p->network.network_msg.net_number == nn) {
            p->network.network_msg.net_number = static_cast<uint8_t>('\xff');
            write_post(i1, p);
          } else if (p->network.network_msg.net_number > nn) {
            p->network.network_msg.net_number--;
            write_post(i1, p);
          }
        }
      }
      close_sub();
    }
  }

  // Now we update the email.
  File emailfile(config.datadir(), EMAIL_DAT);
  if (emailfile.Open(File::modeBinary|File::modeReadWrite)) {
    auto t = emailfile.length() / sizeof(mailrec);
    for (size_t r = 0; r < t; r++) {
      mailrec m = {};
      emailfile.Seek(r * sizeof(mailrec), File::Whence::begin);
      emailfile.Read(&m, sizeof(mailrec));
      if (((m.tosys != 0) || (m.touser != 0)) && m.fromsys) {
        if (strlen(m.title) >= WWIV_MESSAGE_TITLE_LENGTH) {
          // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
          m.title[WWIV_MESSAGE_TITLE_LENGTH] = 0;
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
  unique_ptr<char[]> u(new char[syscfg.userreclen]);
  read_user(config.datadir(), 1, reinterpret_cast<userrec*>(u.get()));
  int nu = number_userrecs(config.datadir());
  for (int i = 1; i <= nu; i++) {
    read_user(config.datadir(), i, reinterpret_cast<userrec*>(u.get()));
    if (UINT(u.get(), syscfg.fsoffset)) {
      if (UCHAR(u.get(), syscfg.fnoffset) == nn) {
        UINT(u.get(), syscfg.fsoffset) = UINT(u.get(), syscfg.fuoffset) = UCHAR(u.get(), syscfg.fnoffset) = 0;
        write_user(config.datadir(), i, reinterpret_cast<userrec*>(u.get()));
      } else if (UCHAR(u.get(), syscfg.fnoffset) > nn) {
        UCHAR(u.get(), syscfg.fnoffset)--;
        write_user(config.datadir(), i, reinterpret_cast<userrec*>(u.get()));
      }
    }
  }

  // Finally delete it from networks.dat
  networks.erase(nn);
  return networks.Save();
}

// Base item of an editable value, this class does not use templates.
class FidoNetworkConfigSubDialog : public BaseEditItem {
public:
  FidoNetworkConfigSubDialog(const std::string& bbsdir, int x, int y, const std::string& title, int width, net_networks_rec& d)
      : BaseEditItem(x, y, 1), bbsdir_(bbsdir), title_(title), width_(width), d_(d), x_(x), y_(y) {};
  virtual ~FidoNetworkConfigSubDialog() {}

  virtual int Run(CursesWindow* window) {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"ENTER", "Edit Items (opens new dialog)."}});
    EditItems items{};
    switch (d_.type) {
    case network_type_t::wwivnet: return 2;
    case network_type_t::internet: return 2;
    case network_type_t::ftn: {
      constexpr int LABEL_WIDTH = 15;
      constexpr int SHORT_FIELD_WIDTH = 25;
      constexpr int LBL1_POSITION = 2;
      constexpr int COL1_POSITION = LBL1_POSITION + LABEL_WIDTH;
      constexpr int LBL2_POSITION = COL1_POSITION + SHORT_FIELD_WIDTH;
      constexpr int COL2_POSITION = LBL2_POSITION + LABEL_WIDTH;
      constexpr int MAX_STRING_LEN = 56;
      fido_network_config_t* n = &d_.fido;
      int y = 1;
      
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN, n->fido_address, false));
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN, n->nodelist_base, false));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, bbsdir_, n->inbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, bbsdir_, n->temp_inbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, bbsdir_, n->temp_outbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, bbsdir_, n->outbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, bbsdir_, n->netmail_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, bbsdir_, n->bad_packets_dir));
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN, n->origin_line, false));

      dy_start_ = y;
      vector<pair<fido_mailer_t, string>> mailerlist = {{fido_mailer_t::flo, "FLO"}, {fido_mailer_t::attach, "NetMail (ATTACH)"}};
      items.add(new ToggleEditItem<fido_mailer_t>(out, COL1_POSITION, y++, mailerlist, &n->mailer_type));

      vector<pair<fido_transport_t, string>> transportlist = {
        {fido_transport_t::directory, "Directory"}, {fido_transport_t::binkp, "WWIV BinkP (N/A)"}
      };
      items.add(new ToggleEditItem<fido_transport_t>(out, COL1_POSITION, y++, transportlist, &n->transport));

      vector<pair<fido_packet_t, string>> packetlist = {
        {fido_packet_t::type2_plus, "FSC-0039 Type 2+"}
      };
      items.add(new ToggleEditItem<fido_packet_t>(out, COL1_POSITION, y++, packetlist, &n->packet_config.packet_type));

      items.add(new StringListItem(out, COL1_POSITION, y++, {"", "ZIP", "ARC", "PKT"}, n->packet_config.compression_type));
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, n->packet_config.packet_password, true));
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, n->packet_config.areafix_password, true));

      // dy_start
      int dy = dy_start_;
      items.add(new NumberEditItem<int>(COL2_POSITION, dy++, &n->packet_config.max_archive_size));
      items.add(new NumberEditItem<int>(COL2_POSITION, dy++, &n->packet_config.max_packet_size));

      vector<pair<fido_bundle_status_t, string>> bundlestatuslist = {
        {fido_bundle_status_t::normal, "Normal"},
        {fido_bundle_status_t::crash, "Crash"},
        {fido_bundle_status_t::direct, "Immediate"},
        {fido_bundle_status_t::hold, "Hold"},
      };
      items.add(new ToggleEditItem<fido_bundle_status_t>(out, COL2_POSITION, dy++, bundlestatuslist, &n->packet_config.netmail_status));

      window->GotoXY(x_, y_);
      int ch = window->GetChar();
      if (ch == KEY_ENTER || ch == TAB || ch == 13) {
        unique_ptr<CursesWindow> sw(out->CreateBoxedWindow(title_, y + 1, width_));
        items.set_curses_io(CursesIO::Get(), sw.get());
        y = 1;
        sw->PutsXY(LBL1_POSITION, y++, "FTN Address  :");
        sw->PutsXY(LBL1_POSITION, y++, "Nodelist Base:");
        sw->PutsXY(LBL1_POSITION, y++, "Inbound Dir  :");
        sw->PutsXY(LBL1_POSITION, y++, "Temp In Dir  :");
        sw->PutsXY(LBL1_POSITION, y++, "Temp Out Dir :");
        sw->PutsXY(LBL1_POSITION, y++, "Outbound Dir :");
        sw->PutsXY(LBL1_POSITION, y++, "NetMail Dir  :");
        sw->PutsXY(LBL1_POSITION, y++, "BadPacket Dir:");
        sw->PutsXY(LBL1_POSITION, y++, "Origin Line  :");

        sw->PutsXY(LBL1_POSITION, y++, "Mailer       :");
        sw->PutsXY(LBL1_POSITION, y++, "Transport    :");
        sw->PutsXY(LBL1_POSITION, y++, "Packet Type  :");
        sw->PutsXY(LBL1_POSITION, y++, "Compression  :");
        sw->PutsXY(LBL1_POSITION, y++, "Packet PW    :");
        sw->PutsXY(LBL1_POSITION, y++, "AreaFix PW   :");

        // dy = y where we start to double up.
        dy = dy_start_;
        sw->PutsXY(LBL2_POSITION, dy++, "Max Arc Size :");
        sw->PutsXY(LBL2_POSITION, dy++, "Max Pkt Size :");
        sw->PutsXY(LBL2_POSITION, dy++, "Bundle Status:");
        items.Run();
        window->RedrawWin();
        return 2;
      } else if (ch == KEY_UP || ch == KEY_BTAB) {
        return 1; // PREV
      } else {
        return 2;
      }
    } break;
    };
    return 2;
  }
  virtual void Display(CursesWindow* window) const { window->PutsXY(x_, y_, "[Enter to Edit]"); }
private:
  const std::string bbsdir_;
  const std::string title_;
  int width_ = 40;
  net_networks_rec& d_;
  int x_ = 0;
  int y_ = 0;
  int dy_start_ = 0;
};

static void edit_fido_node_config(const FidoAddress& a, fido_node_config_t& n) {
  constexpr int LBL1_POSITION = 2;
  constexpr int COL1_POSITION = 17;
  int y = 1;

  auto& p = n.packet_config;
  EditItems items{};
  vector<pair<fido_packet_t, string>> packetlist = {
    {fido_packet_t::unset, "unset"}, {fido_packet_t::type2_plus, "FSC-0039 Type 2+"}
  };

  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 40, n.routes, false));
  items.add(new ToggleEditItem<fido_packet_t>(out, COL1_POSITION, y++, packetlist, &p.packet_type));
  items.add(new StringListItem(out, COL1_POSITION, y++, {"ZIP", "ARC", "PKT", ""}, p.compression_type));
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, p.packet_password, true));
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, p.areafix_password, true));
  items.add(new NumberEditItem<int>(COL1_POSITION, y++, &p.max_archive_size));
  items.add(new NumberEditItem<int>(COL1_POSITION, y++, &p.max_packet_size));

  vector<pair<fido_bundle_status_t, string>> bundlestatuslist = {
    {fido_bundle_status_t::normal, "Normal"},
    {fido_bundle_status_t::crash, "Crash"},
    {fido_bundle_status_t::direct, "Immediate"},
    {fido_bundle_status_t::hold, "Hold"},
  };
  items.add(new ToggleEditItem<fido_bundle_status_t>(out, COL1_POSITION, y++, bundlestatuslist, &p.netmail_status));

  auto& b = n.binkp_config;
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 40, b.host, false));
  items.add(new NumberEditItem<int>(COL1_POSITION, y++, &b.port));
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, b.password, true));

  const string title = StrCat("Address: ", a.as_string());
  unique_ptr<CursesWindow> sw(out->CreateBoxedWindow(title, items.size() + 2, 61));
  items.set_curses_io(out, sw.get());

  y = 1;
  sw->PutsXY(LBL1_POSITION, y++, "Routes       :");
  sw->PutsXY(LBL1_POSITION, y++, "Packet Type  :");
  sw->PutsXY(LBL1_POSITION, y++, "Compression  :");
  sw->PutsXY(LBL1_POSITION, y++, "Packet PW    :");
  sw->PutsXY(LBL1_POSITION, y++, "AreaFix PW   :");
  sw->PutsXY(LBL1_POSITION, y++, "Max Arc Size :");
  sw->PutsXY(LBL1_POSITION, y++, "Max Pkt Size :");
  sw->PutsXY(LBL1_POSITION, y++, "Bundle Status:");

  sw->PutsXY(LBL1_POSITION, y++, "BinkP Host   :");
  sw->PutsXY(LBL1_POSITION, y++, "BinkP Port   :");
  sw->PutsXY(LBL1_POSITION, y++, "Session PW   :");

  items.Run();
}

// Base item of an editable value, this class does not use templates.
class FidoPacketConfigSubDialog: public BaseEditItem {
public:
  FidoPacketConfigSubDialog(const std::string& bbsdir, int x, int y, const std::string& title, int width, const Config& config, net_networks_rec& d)
    : BaseEditItem(x, y, 1), bbsdir_(bbsdir), title_(title), width_(width), config_(config), d_(d), x_(x), y_(y) {};
  virtual ~FidoPacketConfigSubDialog() {}

  virtual int Run(CursesWindow* window) {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"},{"ENTER", "Edit Items (opens new dialog)."}});
    window->GotoXY(x_, y_);
    int ch = window->GetChar();
    if (ch == KEY_ENTER || ch == TAB || ch == 13) {
      wwiv::sdk::fido::FidoCallout callout(config_, d_);
      if (!callout.IsInitialized()) {
        messagebox(window, "Unable to initialize fido_callout.json.");
        return 2;
      }
      bool done = false;
      do {
        vector<ListBoxItem> items;
        for (const auto& e : callout.node_configs_map()) {
          items.emplace_back(e.first.as_string());
        }
        ListBox list(out, window, "Select Address",
          static_cast<int>(floor(window->GetMaxX() * 0.8)),
          std::min<int>(10, static_cast<int>(floor(window->GetMaxY() * 0.8))),
          items, out->color_scheme());

        list.selection_returns_hotkey(true);
        list.set_additional_hotkeys("DI");
        list.set_help_items({{"Esc", "Exit"},{"Enter", "Edit"},{"D", "Delete"},{"I", "Insert"}});
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
            wwiv::sdk::fido::FidoAddress a(items[result.selected].text());
            callout.erase(a);
          } break;
          case 'I': {
            const string prompt = "Enter Address (Z:N/O) : ";
            const string address_string = dialog_input_string(window, prompt, 20);
            if (address_string.empty()) { break; }
            wwiv::sdk::fido::FidoAddress address(address_string);
            fido_node_config_t config{};
            config.binkp_config.port = 24554;
            edit_fido_node_config(address, config);
            callout.insert(address, config);
          } break;
          }
        } else if (result.type == ListBoxResultType::SELECTION) {
          const string address_string = items.at(result.selected).text();
          FidoAddress address(address_string);
          fido_node_config_t c = callout.fido_node_config_for(address);
          edit_fido_node_config(address, c);
          callout.insert(address, c);
        } else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
      } while (!done);
      callout.Save();

      return 2;
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return 1; // PREV
    } else {
      return 2;
    }
  }
  virtual void Display(CursesWindow* window) const { window->PutsXY(x_, y_, "[Enter to Edit]"); }
private:
  const std::string bbsdir_;
  const std::string title_;
  int width_ = 40;
  const Config& config_;
  net_networks_rec& d_;
  int x_ = 0;
  int y_ = 0;
};


static void edit_net(const Config& config, Networks& networks, int nn) {
  static const vector<pair<network_type_t, string>> nettypes = {
    {network_type_t::wwivnet, "WWIVnet "},
    {network_type_t::ftn, "Fido    "},
    {network_type_t::internet, "Internet"}
  };

  Subs subs(config.datadir(), networks.networks());
  bool subs_loaded = subs.Load();

  net_networks_rec& n = networks.at(nn);
  const string orig_network_name(n.name);

  const int COL1_POSITION = 14;
  int y = 1;
  EditItems items{
    new ToggleEditItem<network_type_t>(out, COL1_POSITION, y++, nettypes, &n.type),
    new StringEditItem<char*>(COL1_POSITION, y++, 15, n.name, false),
    new NumberEditItem<uint16_t>(COL1_POSITION, y++, &n.sysnum),
    new StringFilePathItem(COL1_POSITION, y++, 60, config.root_directory(), n.dir)
  };
  if (n.type == network_type_t::ftn) {
    auto net_dir = File::absolute(config.root_directory(), n.dir);
    items.add(new FidoNetworkConfigSubDialog(net_dir, COL1_POSITION, y++, "Network Settings", 76, n));
    items.add(new FidoPacketConfigSubDialog(net_dir, COL1_POSITION, y++, "Node Settings", 76, config, n));
  }

  const string title = StrCat("Network Configuration; Net #", nn);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow(title, items.size() + 2, 76));
  items.set_curses_io(out, window.get());

  y = 1;
  window->PutsXY(2, y++, "Net Type  :");
  window->PutsXY(2, y++, "Net Name  :");
  window->PutsXY(2, y++, "Node #    :");
  window->PutsXY(2, y++, "Directory :");
  if (n.type == network_type_t::ftn) {
    window->PutsXY(2, y++, "Settings  :");
    window->PutsXY(2, y++, "Addresses :");
  }
  items.Run();

  if (subs_loaded && orig_network_name != n.name) {
    subs.Save();
  }

  networks.Save();
}

static bool insert_net(const Config& config, Networks& networks, int nn) {
  wwiv::sdk::Subs subs(config.datadir(), networks.networks());
  if (!subs.Load()) {
    return false;
  }
  if (subs.subs().empty()) {
    return false;
  }

  for (size_t i = 0; i < subs.subs().size(); i++) {
    size_t i2 = 0;
    for (i2 = 0; i2 < i; i2++) {
      if (subs.sub(i).filename == subs.sub(i2).filename) {
        break;
      }
    }
    if (i2 >= i) {
      iscan1(i, subs, config);
      open_sub(true);
      for (int i1 = 1; i1 <= GetNumMessagesInCurrentMessageArea(); i1++) {
        postrec* p = get_post(i1);
        if (p->status & status_post_new_net) {
          if (p->network.network_msg.net_number >= nn) {
            p->network.network_msg.net_number++;
            write_post(i1, p);
          }
        }
      }
      close_sub();
    }
  }

  // same as del_net, don't think we need to do this here.
  // wwiv::sdk::write_subs(config.datadir(), subboards);
  File emailfile(config.datadir(), EMAIL_DAT);
  if (emailfile.Open(File::modeBinary|File::modeReadWrite)) {
    auto t = emailfile.length() / sizeof(mailrec);
    for (size_t r = 0; r < t; r++) {
      mailrec m;
      emailfile.Seek(sizeof(mailrec) * r, File::Whence::begin);
      emailfile.Read(&m, sizeof(mailrec));
      if (((m.tosys != 0) || (m.touser != 0)) && m.fromsys) {
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

  unique_ptr<char[]> u(new char[syscfg.userreclen]);
  read_user(config.datadir(), 1, reinterpret_cast<userrec*>(u.get()));
  int nu = number_userrecs(config.datadir());
  for (int i = 1; i <= nu; i++) {
    read_user(config.datadir(), i, reinterpret_cast<userrec*>(u.get()));
    if (UINT(u.get(), syscfg.fsoffset)) {
      if (UCHAR(u.get(), syscfg.fnoffset) >= nn) {
        UCHAR(u.get(), syscfg.fnoffset)++;
        write_user(config.datadir(), i, reinterpret_cast<userrec*>(u.get()));
      }
    }
  }

  {
    net_networks_rec n{};
    to_char_array(n.name, "NetNet");
    n.dir = StrCat("newnet.dir", File::pathSeparatorChar);
    networks.insert(nn, n);
  }

  edit_net(config, networks, nn);
  return true;
}

void networks(wwiv::sdk::Config& config) {
  try {
    Networks networks(config);

    bool done = false;
    do {
      vector<ListBoxItem> items;
      for (const auto& n : networks.networks()) {
        items.emplace_back(StringPrintf("@%u %s", n.sysnum, n.name));
      }
      CursesWindow* window = out->window();
      ListBox list(out, window, "Select Network", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
          static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("DI");
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
      ListBoxResult result = list.Run();

      if (result.type == ListBoxResultType::SELECTION) {
        edit_net(config, networks, result.selected);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
        switch (result.hotkey) {
        case 'D':
          if (!(syscfg.fnoffset && syscfg.fsoffset && syscfg.fuoffset)) {
            messagebox(window, { "You must run the BBS once", "to set up some variables before ", "deleting a network." });
            break;
          }
          if (networks.networks().size() > 1) {
            const string prompt = StringPrintf("Delete '%s'", networks.at(result.selected).name);
            bool yn = dialog_yn(window, prompt);
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
          if (!(syscfg.fnoffset && syscfg.fsoffset && syscfg.fuoffset)) {
            vector<string> lines{ "You must run the BBS once to set up ", "some variables before inserting a network." };
            messagebox(window, lines);
            break;
          }
          if (networks.networks().size() >= MAX_NETWORKS) {
            messagebox(window, "Too many networks.");
            break;
          }
          const string prompt = StringPrintf("Insert before which (1-%d) ? ", networks.networks().size() + 1);
          const size_t net_num = dialog_input_number(window, prompt, 1, networks.networks().size() + 1  );
          if (net_num > 0 && net_num <= networks.networks().size() + 1) {
            if (dialog_yn(window, "Are you sure? ")) {
              insert_net(config, networks, net_num - 1);
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

