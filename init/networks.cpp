/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2016 WWIV Software Services            */
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

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

static bool del_net(
    Networks& networks, int nn) {
  wwiv::sdk::Subs subs(syscfg.datadir, networks.networks());
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
      iscan1(i, subs);
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
  File emailfile(syscfg.datadir, EMAIL_DAT);
  if (emailfile.Open(File::modeBinary|File::modeReadWrite)) {
    auto t = emailfile.GetLength() / sizeof(mailrec);
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
  read_user(1, reinterpret_cast<userrec*>(u.get()));
  int nu = number_userrecs();
  for (int i = 1; i <= nu; i++) {
    read_user(i, reinterpret_cast<userrec*>(u.get()));
    if (UINT(u.get(), syscfg.fsoffset)) {
      if (UCHAR(u.get(), syscfg.fnoffset) == nn) {
        UINT(u.get(), syscfg.fsoffset) = UINT(u.get(), syscfg.fuoffset) = UCHAR(u.get(), syscfg.fnoffset) = 0;
        write_user(i, reinterpret_cast<userrec*>(u.get()));
      } else if (UCHAR(u.get(), syscfg.fnoffset) > nn) {
        UCHAR(u.get(), syscfg.fnoffset)--;
        write_user(i, reinterpret_cast<userrec*>(u.get()));
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
  FidoNetworkConfigSubDialog(int x, int y, const std::string& title, int width, net_networks_rec& d)
      : BaseEditItem(x, y, 1), title_(title), width_(width), d_(d), x_(x), y_(y) {};
  virtual ~FidoNetworkConfigSubDialog() {}

  virtual int Run(CursesWindow* window) {
    EditItems items{};
    switch (d_.type) {
    case network_type_t::wwivnet: return 2;
    case network_type_t::internet: return 2;
    case network_type_t::ftn: {
      const int COL1_POSITION = 17;
      const int MAX_STRING_LEN = 56;
      fido_network_config_t* n = &d_.fido;
      int y = 1;
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, MAX_STRING_LEN, n->fido_address, false));
      items.add(new NumberEditItem<uint16_t>(COL1_POSITION, y++, &n->fake_outbound_node));
      items.add(new ToggleEditItem<fido_mailer_t>(COL1_POSITION, y++, {"unset", "FLO", "NetMail (ATTACH)"}, &n->mailer_type));
      items.add(new ToggleEditItem<fido_transport_t>(COL1_POSITION, y++, {"DIRECTORY", "WWIV BINKP (Not Implemented Yet)"}, &n->transport));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, n->inbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, n->temp_inbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, n->temp_outbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, n->outbound_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, n->netmail_dir));
      items.add(new StringFilePathItem(COL1_POSITION, y++, MAX_STRING_LEN, n->bad_packets_dir));
      items.add(new ToggleEditItem<fido_packet_t>(COL1_POSITION, y++, {"unset", "FSC-0039 Type 2+"}, &n->packet_config.packet_type));
      items.add(new StringListItem(COL1_POSITION, y++, {"", "ZIP", "ARC", "PKT"}, n->packet_config.compression_type));
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, n->packet_config.packet_password, true));
      items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, n->packet_config.areafix_password, true));
      items.add(new NumberEditItem<int>(COL1_POSITION, y++, &n->packet_config.max_archive_size));
      items.add(new NumberEditItem<int>(COL1_POSITION, y++, &n->packet_config.max_packet_size));
      window->GotoXY(x_, y_);
      int ch = window->GetChar();
      if (ch == KEY_ENTER || ch == TAB || ch == 13) {
        unique_ptr<CursesWindow> sw(out->CreateBoxedWindow(title_, items.size() + 2, width_));
        items.set_curses_io(CursesIO::Get(), sw.get());
        y = 1;
        sw->PutsXY(2, y++, "FTN Address  :");
        sw->PutsXY(2, y++, "Fake Outbound:");
        sw->PutsXY(2, y++, "Mailer       :");
        sw->PutsXY(2, y++, "Transport    :");
        sw->PutsXY(2, y++, "Inbound Dir  :");
        sw->PutsXY(2, y++, "Temp In Dir  :");
        sw->PutsXY(2, y++, "Temp Out Dir :");
        sw->PutsXY(2, y++, "Outbound Dir :");
        sw->PutsXY(2, y++, "NetMail Dir  :");
        sw->PutsXY(2, y++, "BadPacket Dir:");
        sw->PutsXY(2, y++, "Packet Type  :");
        sw->PutsXY(2, y++, "Compression  :");
        sw->PutsXY(2, y++, "Packet PW    :");
        sw->PutsXY(2, y++, "AreaFix PW   :");
        sw->PutsXY(2, y++, "Max Arc Size :");
        sw->PutsXY(2, y++, "Max Pkt Size :");
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
  const std::string title_;
  int width_ = 40;
  net_networks_rec& d_;
  int x_ = 0;
  int y_ = 0;
};

void edit_packet_config(const Config& config, const FidoAddress& a, fido_packet_config_t& p) {
  const int COL1_POSITION = 17;
  int y = 1;
  EditItems items{};
  items.add(new ToggleEditItem<fido_packet_t>(COL1_POSITION, y++, {"unset", "FSC-0039 Type 2+"}, &p.packet_type));
  items.add(new StringListItem(COL1_POSITION, y++, {"ZIP", "ARC", "PKT", ""}, p.compression_type));
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, p.packet_password, true));
  items.add(new StringEditItem<std::string&>(COL1_POSITION, y++, 8, p.areafix_password, true));
  items.add(new NumberEditItem<int>(COL1_POSITION, y++, &p.max_archive_size));
  items.add(new NumberEditItem<int>(COL1_POSITION, y++, &p.max_packet_size));

  const string title = StrCat("Address: ", a.as_string());
  unique_ptr<CursesWindow> sw(out->CreateBoxedWindow(title, items.size() + 2, 76));
  items.set_curses_io(out, sw.get());

  y = 1;
  sw->PutsXY(2, y++, "Packet Type  :");
  sw->PutsXY(2, y++, "Compression  :");
  sw->PutsXY(2, y++, "Packet PW    :");
  sw->PutsXY(2, y++, "AreaFix PW   :");
  sw->PutsXY(2, y++, "Max Arc Size :");
  sw->PutsXY(2, y++, "Max Pkt Size :");
  items.Run();
}

// Base item of an editable value, this class does not use templates.
class FidoPacketConfigSubDialog: public BaseEditItem {
public:
  FidoPacketConfigSubDialog(int x, int y, const std::string& title, int width, const Config& config, net_networks_rec& d)
    : BaseEditItem(x, y, 1), title_(title), width_(width), config_(config), d_(d), x_(x), y_(y) {};
  virtual ~FidoPacketConfigSubDialog() {}

  virtual int Run(CursesWindow* window) {
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
        for (const auto& e : callout.node_configs()) {
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
            fido_packet_config_t config{};
            edit_packet_config(config_, address, config);
            callout.insert(address, config);
          } break;
          }
        } else if (result.type == ListBoxResultType::SELECTION) {
          const string address_string = items.at(result.selected).text();
          FidoAddress address(address_string);
          fido_packet_config_t c = callout.packet_override_for(address);
          edit_packet_config(config_, address, c);
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
  const std::string title_;
  int width_ = 40;
  const Config& config_;
  net_networks_rec& d_;
  int x_ = 0;
  int y_ = 0;
};


static void edit_net(const Config& config, Networks& networks, int nn) {
  static const vector<string> nettypes{
    "WWIVnet ",
    "Fido    ",
    "Internet",
  };

  Subs subs(syscfg.datadir, networks.networks());
  bool subs_loaded = subs.Load();

  net_networks_rec& n = networks.at(nn);
  const string orig_network_name(n.name);

  if (static_cast<int>(n.type) >= nettypes.size()) {
    n.type = static_cast<network_type_t>(0);
  }

  const int COL1_POSITION = 14;
  int y = 1;
  EditItems items{
    new ToggleEditItem<network_type_t>(COL1_POSITION, y++, nettypes, &n.type),
    new StringEditItem<char*>(COL1_POSITION, y++, 15, n.name, false),
    new NumberEditItem<uint16_t>(COL1_POSITION, y++, &n.sysnum),
    new StringFilePathItem(COL1_POSITION, y++, 60, n.dir)
  };
  if (n.type == network_type_t::ftn) {
    items.add(new FidoNetworkConfigSubDialog(COL1_POSITION, y++, "Network Settings", 76, n));
    items.add(new FidoPacketConfigSubDialog(COL1_POSITION, y++, "Node Settings", 76, config, n));
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
  wwiv::sdk::Subs subs(syscfg.datadir, networks.networks());
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
      iscan1(i, subs);
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
  // wwiv::sdk::write_subs(syscfg.datadir, subboards);
  File emailfile(syscfg.datadir, EMAIL_DAT);
  if (emailfile.Open(File::modeBinary|File::modeReadWrite)) {
    auto t = emailfile.GetLength() / sizeof(mailrec);
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
  read_user(1, reinterpret_cast<userrec*>(u.get()));
  int nu = number_userrecs();
  for (int i = 1; i <= nu; i++) {
    read_user(i, reinterpret_cast<userrec*>(u.get()));
    if (UINT(u.get(), syscfg.fsoffset)) {
      if (UCHAR(u.get(), syscfg.fnoffset) >= nn) {
        UCHAR(u.get(), syscfg.fnoffset)++;
        write_user(i, reinterpret_cast<userrec*>(u.get()));
      }
    }
  }

  {
    net_networks_rec n{};
    strcpy(n.name, "NetNet");
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
                del_net(networks, result.selected);
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

