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

#include "init/init.h"
#include "localui/input.h"
#include "localui/listbox.h"
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
#include "sdk/networks.h"
#include "sdk/subxtr.h"

#define UINT(u,n)  (*((int  *)(((char *)(u))+(n))))
#define UCHAR(u,n) (*((char *)(((char *)(u))+(n))))

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
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
    long t = emailfile.GetLength() / sizeof(mailrec);
    for (int r = 0; r < t; r++) {
      mailrec m = {};
      emailfile.Seek(r * sizeof(mailrec), File::seekBegin);
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
        emailfile.Seek(r * sizeof(mailrec), File::seekBegin);
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

static void edit_net(Networks& networks, int nn) {
  static const vector<string> nettypes = {
    "WWIVnet ",
    "Fido    ",
    "Internet",
  };

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Network Configuration", 6, 76));
  net_networks_rec& n = networks.at(nn);
  char szOldNetworkName[20];
  strcpy(szOldNetworkName, n.name);

  if (static_cast<int>(n.type) >= nettypes.size()) {
    n.type = static_cast<network_type_t>(0);
  }

  const int COL1_POSITION = 14;
  EditItems items{
    new ToggleEditItem<network_type_t>(COL1_POSITION, 1, nettypes, &n.type),
    new StringEditItem<char*>(COL1_POSITION, 2, 15, n.name, false),
    new NumberEditItem<uint16_t>(COL1_POSITION, 3, &n.sysnum),
    new StringFilePathItem(COL1_POSITION, 4, 60, n.dir),
  };
  items.set_curses_io(out, window.get());

  int y = 1;
  window->PutsXY(2, y++, "Net Type  :");
  window->PutsXY(2, y++, "Net Name  :");
  window->PutsXY(2, y++, "Node #    :");
  window->PutsXY(2, y++, "Directory :");

  items.Run();

  if (strcmp(szOldNetworkName, n.name)) {
    const string input_filename = StringPrintf("%ssubs.xtr", syscfg.datadir);
    const string output_filename = StringPrintf("%ssubsxtr.new", syscfg.datadir);
    FILE *pInputFile = fopen(input_filename.c_str(), "r");
    if (pInputFile) {
      FILE *pOutputFile = fopen(output_filename.c_str(), "w");
      if (pOutputFile) {
        char buffer[255];
        while (fgets(buffer, 80, pInputFile)) {
          if (buffer[0] == '$') {
            char* ss = strchr(buffer, ' ');
            if (ss) {
              *ss = 0;
              if (strcasecmp(szOldNetworkName, buffer + 1) == 0) {
                fprintf(pOutputFile, "$%s %s", n.name, ss + 1);
              } else {
                fprintf(pOutputFile, "%s %s", buffer, ss + 1);
              }
            } else {
              fprintf(pOutputFile, "%s", buffer);
            }
          } else {
            fprintf(pOutputFile, "%s", buffer);
          }
        }
        fclose(pOutputFile);
        fclose(pInputFile);
        const string old_filename = StringPrintf("%ssubsxtr.old", syscfg.datadir);
        File::Remove(old_filename);
        File::Rename(input_filename, old_filename);
        File::Remove(input_filename);
        File::Rename(output_filename, input_filename);
      } else {
        fclose(pInputFile);
      }
    }
  }
  networks.Save();
}

static bool insert_net(Networks& networks, int nn) {
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
    long t = emailfile.GetLength() / sizeof(mailrec);
    for (int r = 0; r < t; r++) {
      mailrec m;
      emailfile.Seek(sizeof(mailrec) * r, File::seekBegin);
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
        emailfile.Seek(sizeof(mailrec) * r, File::seekBegin);
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

  edit_net(networks, nn);
  return true;
}

void networks(wwiv::sdk::Config& config) {
  Networks networks(config);

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);

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
      edit_net(networks, result.selected);
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
            insert_net(networks, net_num - 1);
          }
        }
        break;
      }
    }
  } while (!done);
  networks.Save();
}

