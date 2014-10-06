/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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

#include "init/ifcns.h"
#include "init/init.h"
#include "initlib/input.h"
#include "initlib/listbox.h"
#include "core/strings.h"
#include "core/wfile.h"
#include "core/wwivport.h"
#include "core/wfile.h"
#include "init/subacc.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "sdk/filenames.h"

#define UINT(u,n)  (*((int  *)(((char *)(u))+(n))))
#define UCHAR(u,n) (*((char *)(((char *)(u))+(n))))

static void edit_net(int nn);

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::strings::StringPrintf;

static bool read_subs(CursesWindow* window) {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "%ssubs.dat", syscfg.datadir);
  int i = open(szFileName, O_RDWR | O_BINARY);
  if (i > 0) {
    subboards = (subboardrec *) malloc(filelength(i) + 1);
    initinfo.num_subs = (read(i, subboards, (filelength(i)))) /
                        sizeof(subboardrec);
    close(i);
  } else {
    messagebox(window, StringPrintf("%s NOT FOUND.\n", szFileName));
    return false;
  }
  return true;
}

static void write_subs() {
  if (subboards) {
    const string filename = StringPrintf("%ssubs.dat", syscfg.datadir);
    int i = open(filename.c_str(), O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    if (i > 0) {
      write(i, &subboards[0], initinfo.num_subs * sizeof(subboardrec));
      close(i);
    }
    initinfo.num_subs = 0;
    free(subboards);
    subboards = nullptr;
  }
}

static bool del_net(CursesWindow* window, int nn) {
  if (!read_subs(window)) {
    return false;
  }

  for (int i = 0; i < initinfo.num_subs; i++) {
    if (subboards[i].age & 0x80) {
      if (subboards[i].name[40] == nn) {
        subboards[i].type = 0;
        subboards[i].age &= 0x7f;
        subboards[i].name[40] = 0;
      } else if (subboards[i].name[40] > nn) {
        subboards[i].name[40]--;
      }
    }
    int i2;
    for (i2 = 0; i2 < i; i2++) {
      if (strcmp(subboards[i].filename, subboards[i2].filename) == 0) {
        break;
      }
    }
    if (i2 >= i) {
      iscan1(i);
      open_sub(true);
      for (int i1 = 1; i1 <= initinfo.nNumMsgsInCurrentSub; i1++) {
        postrec* p = get_post(i1);
        if (p->status & status_post_new_net) {
          if (p->title[80] == nn) {
            p->title[80] = -1;
            write_post(i1, p);
          } else if (p->title[80] > nn) {
            p->title[80]--;
            write_post(i1, p);
          }
        }
      }
      close_sub();
    }
  }

  write_subs();

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%semail.dat", syscfg.datadir);
  int hFile = open(szFileName, O_BINARY | O_RDWR);
  if (hFile != -1) {
    long t = filelength(hFile) / sizeof(mailrec);
    for (int r = 0; r < t; r++) {
      mailrec m;
      lseek(hFile, r*sizeof(mailrec), SEEK_SET);
      read(hFile, &m, sizeof(mailrec));
      if (((m.tosys != 0) || (m.touser != 0)) && m.fromsys) {
        int i = (m.status & status_source_verified) ? 78 : 80;
        if ((int) strlen(m.title) >= i) {
          m.title[i] = m.title[i - 1] = 0;
        }
        m.status |= status_new_net;
        if (m.title[i] == nn) {
          m.title[i] = -1;
        } else if (m.title[i] > nn) {
          m.title[i]--;
        }
        lseek(hFile, r*sizeof(mailrec), SEEK_SET);
        write(hFile, &m, sizeof(mailrec));
      }
    }
    close(hFile);
  }

  char* u = (char *)malloc(syscfg.userreclen);
  read_user(1, (userrec *)u);
  int nu = number_userrecs();
  for (int i = 1; i <= nu; i++) {
    read_user(i, (userrec *)u);
    if (UINT(u, syscfg.fsoffset)) {
      if (UCHAR(u, syscfg.fnoffset) == nn) {
        UINT(u, syscfg.fsoffset) = UINT(u, syscfg.fuoffset) = UCHAR(u, syscfg.fnoffset) = 0;
        write_user(i, (userrec *)u);
      } else if (UCHAR(u, syscfg.fnoffset) > nn) {
        UCHAR(u, syscfg.fnoffset)--;
        write_user(i, (userrec *)u);
      }
    }
  }

  free(u);

  for (int i = nn; i < initinfo.net_num_max; i++) {
    net_networks[i] = net_networks[i + 1];
  }
  initinfo.net_num_max--;

  sprintf(szFileName, "%snetworks.dat", syscfg.datadir);
  int f = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(f, net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
  close(f);

  return true;
}

static bool insert_net(CursesWindow* window, int nn) {
  int i, i1, i2;

  if (!read_subs(window)) {
    return false;
  }

  for (i = 0; i < initinfo.num_subs; i++) {
    if (subboards[i].age & 0x80) {
      if (subboards[i].name[40] >= nn) {
        subboards[i].name[40]++;
      }
    }
    for (i2 = 0; i2 < i; i2++) {
      if (strcmp(subboards[i].filename, subboards[i2].filename) == 0) {
        break;
      }
    }
    if (i2 >= i) {
      iscan1(i);
      open_sub(true);
      for (i1 = 1; i1 <= initinfo.nNumMsgsInCurrentSub; i1++) {
        postrec* p = get_post(i1);
        if (p->status & status_post_new_net) {
          if (p->title[80] >= nn) {
            p->title[80]++;
            write_post(i1, p);
          }
        }
      }
      close_sub();
    }
  }

  write_subs();

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%semail.dat", syscfg.datadir);
  int hFile = open(szFileName, O_BINARY | O_RDWR);
  if (hFile != -1) {
    long t = filelength(hFile) / sizeof(mailrec);
    for (int r = 0; r < t; r++) {
      mailrec m;
      lseek(hFile, sizeof(mailrec) * r, SEEK_SET);
      read(hFile, &m, sizeof(mailrec));
      if (((m.tosys != 0) || (m.touser != 0)) && m.fromsys) {
        i = (m.status & status_source_verified) ? 78 : 80;
        if ((int) strlen(m.title) >= i) {
          m.title[i] = m.title[i - 1] = 0;
        }
        m.status |= status_new_net;
        if (m.title[i] >= nn) {
          m.title[i]++;
        }
        lseek(hFile, (long)(sizeof(mailrec)) * (long)(r), SEEK_SET);
        write(hFile, &m, sizeof(mailrec));
      }
    }
    close(hFile);
  }

  char* u = (char *)malloc(syscfg.userreclen);

  read_user(1, (userrec *)u);
  int nu = number_userrecs();
  for (i = 1; i <= nu; i++) {
    read_user(i, (userrec *)u);
    if (UINT(u, syscfg.fsoffset)) {
      if (UCHAR(u, syscfg.fnoffset) >= nn) {
        UCHAR(u, syscfg.fnoffset)++;
        write_user(i, (userrec *)u);
      }
    }
  }
  free(u);

  for (i = initinfo.net_num_max; i > nn; i--) {
    net_networks[i] = net_networks[i - 1];
  }
  initinfo.net_num_max++;
  memset(&(net_networks[nn]), 0, sizeof(net_networks_rec));
  strcpy(net_networks[nn].name, "NewNet");
  sprintf(net_networks[nn].dir, "newnet.dir%c", WFile::pathSeparatorChar);

  sprintf(szFileName, "%snetworks.dat", syscfg.datadir);
  i = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(i, net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
  close(i);

  edit_net(nn);

  return true;
}

#define OKAD (syscfg.fnoffset && syscfg.fsoffset && syscfg.fuoffset)

void networks() {
  if (!WFile::Exists("NETWORK.EXE")) {
    vector<string> lines{
      "WARNING",
      "You have not installed the networking software.  Unzip netxx.zip",
      "to the main BBS directory and re-run init.",
      "",
      "Hit any key to continue."
    };
    out->Cls(ACS_CKBOARD);
    messagebox(out->window(), lines);
  }

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);

    vector<ListBoxItem> items;
    for (int i = 0; i < initinfo.net_num_max; i++) {
      items.emplace_back(StringPrintf("@%u %s", net_networks[i].sysnum, net_networks[i].name));
    }
    CursesWindow* window = out->window();
    ListBox list(out, window, "Select Network", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();

    if (result.type == ListBoxResultType::SELECTION) {
      edit_net(result.selected);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    } else if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D':
        if (!OKAD) {
          messagebox(window, { "You must run the BBS once", "to set up some variables before ", "deleting a network." });
          break;
        }
        if (initinfo.net_num_max > 1) {
          const string prompt = StringPrintf("Delete '%s'", net_networks[result.selected].name);
          bool yn = dialog_yn(window, prompt);
          if (yn) {
            yn = dialog_yn(window, "Are you REALLY sure? ");
            if (yn) {
              del_net(window, result.selected);
            }
          }
        } else {
          messagebox(window, "You must leave at least one network.");
        }
        break;
      case 'I':
        if (!OKAD) {
          vector<string> lines{ "You must run the BBS once to set up ", "some variables before inserting a network." };
          messagebox(window, lines);
          break;
        }
        if (initinfo.net_num_max >= MAX_NETWORKS) {
          messagebox(window, "Too many networks.");
          break;
        }
        const string prompt = StringPrintf("Insert before which (1-%d) ? ", initinfo.net_num_max + 1);
        int nNetNumber = dialog_input_number(window, prompt, 1, initinfo.net_num_max + 1  );
        if (nNetNumber > 0 && nNetNumber <= initinfo.net_num_max + 1) {
          bool yn = dialog_yn(window, "Are you sure? ");
          if (yn) {
            insert_net(window, nNetNumber - 1);
          }
        }
        break;
      }
    }
  } while (!done);

  WFile file (syscfg.datadir, NETWORKS_DAT);
  if (file.Open(WFile::modeReadWrite|WFile::modeCreateFile|WFile::modeTruncate|WFile::modeBinary,
    WFile::shareDenyReadWrite)) {
    file.Write(net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
  }
  file.Close();
}

static void edit_net(int nn) {

  static const vector<string> nettypes = {
    "WWIVnet ",
    "Fido    ",
    "Internet",
  };

  char szOldNetworkName[20];
  char *ss;

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Network Configuration", 6, 76));
  bool done = false;
  int cp = 1;
  net_networks_rec *n = &(net_networks[nn]);
  strcpy(szOldNetworkName, n->name);

  if (n->type >= nettypes.size()) {
    n->type = 0;
  }

  const int COL1_POSITION = 14;
  EditItems items{
    new ToggleEditItem<uint8_t>(COL1_POSITION, 1, nettypes, &n->type),
    new StringEditItem<char*>(COL1_POSITION, 2, 15, n->name, false),
    new NumberEditItem<uint16_t>(COL1_POSITION, 3, &n->sysnum),
    new FilePathItem(COL1_POSITION, 4, 60, n->dir),
  };
  items.set_curses_io(out, window.get());

  int y = 1;
  window->PutsXY(2, y++, "Net Type  :");
  window->PutsXY(2, y++, "Net Name  :");
  window->PutsXY(2, y++, "Node #    :");
  window->PutsXY(2, y++, "Directory :");

  items.Run();

  if (strcmp(szOldNetworkName, n->name)) {
    char szInputFileName[ MAX_PATH ];
    char szOutputFileName[ MAX_PATH ];
    sprintf(szInputFileName, "%ssubs.xtr", syscfg.datadir);
    sprintf(szOutputFileName, "%ssubsxtr.new", syscfg.datadir);
    FILE *pInputFile = fopen(szInputFileName, "r");
    if (pInputFile) {
      FILE *pOutputFile = fopen(szOutputFileName, "w");
      if (pOutputFile) {
        char szBuffer[ 255 ];
        while (fgets(szBuffer, 80, pInputFile)) {
          if (szBuffer[0] == '$') {
            ss = strchr(szBuffer, ' ');
            if (ss) {
              *ss = 0;
              if (strcasecmp(szOldNetworkName, szBuffer + 1) == 0) {
                fprintf(pOutputFile, "$%s %s", n->name, ss + 1);
              } else {
                fprintf(pOutputFile, "%s %s", szBuffer, ss + 1);
              }
            } else {
              fprintf(pOutputFile, "%s", szBuffer);
            }
          } else {
            fprintf(pOutputFile, "%s", szBuffer);
          }
        }
        fclose(pOutputFile);
        fclose(pInputFile);
        char szOldSubsFileName[ MAX_PATH ];
        sprintf(szOldSubsFileName, "%ssubsxtr.old", syscfg.datadir);
        unlink(szOldSubsFileName);
        rename(szInputFileName, szOldSubsFileName);
        unlink(szInputFileName);
        rename(szOutputFileName, szInputFileName);
      } else {
        fclose(pInputFile);
      }
    }
  }
}
