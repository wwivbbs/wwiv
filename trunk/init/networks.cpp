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
#include "template.h"

#include <curses.h>
#include <cstdint>
#include <string>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "platform/incl1.h"
#include "subacc.h"
#include "wwivinit.h"

#define UINT(u,n)  (*((int  *)(((char *)(u))+(n))))
#define UCHAR(u,n) (*((char *)(((char *)(u))+(n))))


int read_subs() {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "%ssubs.dat", syscfg.datadir);
  int i = open(szFileName, O_RDWR | O_BINARY);
  if (i > 0) {
    subboards = (subboardrec *) bbsmalloc(filelength(i) + 1);
    if (!subboards) {
      Printf("needed %ld bytes\n", filelength(i));
      textattr(COLOR_WHITE);
      exit_init(2);
    }

    initinfo.num_subs = (read(i, subboards, (filelength(i)))) /
                        sizeof(subboardrec);
    close(i);
  } else {
    Printf("%s NOT FOUND.\n", szFileName);
    textattr(COLOR_WHITE);
    exit_init(2);
  }
  return 0;
}

void write_subs() {
  char szFileName[MAX_PATH];

  if (subboards) {
    sprintf(szFileName, "%ssubs.dat", syscfg.datadir);
    int i = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    if (i > 0) {
      write(i, (void *)&subboards[0], initinfo.num_subs * sizeof(subboardrec));
      close(i);
    }
    initinfo.num_subs = 0;
    BbsFreeMemory(subboards);
    subboards = NULL;
  }
}

void del_net(int nn) {
  int i, t, r, nu, i1, i2;
  mailrec m;
  char *u;
  postrec *p;

  read_subs();

  for (i = 0; i < initinfo.num_subs; i++) {
    if (subboards[i].age & 0x80) {
      if (subboards[i].name[40] == nn) {
        subboards[i].type = 0;
        subboards[i].age &= 0x7f;
        subboards[i].name[40] = 0;
      } else if (subboards[i].name[40] > nn) {
        subboards[i].name[40]--;
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
        p = get_post(i1);
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
    t = (int)(filelength(hFile) / sizeof(mailrec));
    for (r = 0; r < t; r++) {
      lseek(hFile, (long)(sizeof(mailrec)) * (long)(r), SEEK_SET);
      read(hFile, (void *)&m, sizeof(mailrec));
      if (((m.tosys != 0) || (m.touser != 0)) && m.fromsys) {
        if (m.status & status_source_verified) {
          i = 78;
        } else {
          i = 80;
        }
        if ((int) strlen(m.title) >= i) {
          m.title[i] = m.title[i - 1] = 0;
        }
        m.status |= status_new_net;
        if (m.title[i] == nn) {
          m.title[i] = -1;
        } else if (m.title[i] > nn) {
          m.title[i]--;
        }
        lseek(hFile, (long)(sizeof(mailrec)) * (long)(r), SEEK_SET);
        write(hFile, (void *)&m, sizeof(mailrec));
      }
    }
    close(hFile);
  }


  u = (char *)bbsmalloc(syscfg.userreclen);

  read_user(1, (userrec *)u);
  nu = number_userrecs();
  for (i = 1; i <= nu; i++) {
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

  BbsFreeMemory(u);

  for (i = nn; i < initinfo.net_num_max; i++) {
    net_networks[i] = net_networks[i + 1];
  }
  initinfo.net_num_max--;

  sprintf(szFileName, "%snetworks.dat", syscfg.datadir);
  i = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(i, (void *)net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
  close(i);
}


void insert_net(int nn) {
  int i, t, r, nu, i1, i2;
  mailrec m;
  char *u;
  postrec *p;

  read_subs();

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
        p = get_post(i1);
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
    t = (int)(filelength(hFile) / sizeof(mailrec));
    for (r = 0; r < t; r++) {
      lseek(hFile, (long)(sizeof(mailrec)) * (long)(r), SEEK_SET);
      read(hFile, (void *)&m, sizeof(mailrec));
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
        write(hFile, (void *)&m, sizeof(mailrec));
      }
    }
    close(hFile);
  }

  u = (char *)bbsmalloc(syscfg.userreclen);

  read_user(1, (userrec *)u);
  nu = number_userrecs();
  for (i = 1; i <= nu; i++) {
    read_user(i, (userrec *)u);
    if (UINT(u, syscfg.fsoffset)) {
      if (UCHAR(u, syscfg.fnoffset) >= nn) {
        UCHAR(u, syscfg.fnoffset)++;
        write_user(i, (userrec *)u);
      }
    }
  }


  BbsFreeMemory(u);

  for (i = initinfo.net_num_max; i > nn; i--) {
    net_networks[i] = net_networks[i - 1];
  }
  initinfo.net_num_max++;
  memset(&(net_networks[nn]), 0, sizeof(net_networks_rec));
  strcpy(net_networks[nn].name, "NewNet");
  sprintf(net_networks[nn].dir, "newnet.dir%c", WWIV_FILE_SEPERATOR_CHAR);

  sprintf(szFileName, "%snetworks.dat", syscfg.datadir);
  i = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(i, (void *)net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
  close(i);

  edit_net(nn);
}



#define OKAD (syscfg.fnoffset && syscfg.fsoffset && syscfg.fuoffset)

void networks() {
  char s1[81];
  bool done = false;

  if (!exist("NETWORK.EXE")) {
    out->Cls();
    nlx();
    textattr(COLOR_MAGENTA);
    Printf("WARNING\n");
    Printf("You have not installed the networking software.  Unzip netxx.zip\n");
    Printf("to the main BBS directory and re-run init.\n\n");
    Printf("Hit any key to continue.\n");
    out->GetChar();
  }

  do {
    out->Cls();
    nlx();
    for (int i = 0; i < initinfo.net_num_max; i++) {
      if (i && ((i % 23) == 0)) {
        pausescr();
      }
      Printf("%-2d. %-15s   @%-5u  %s\n", i + 1, net_networks[i].name, net_networks[i].sysnum, net_networks[i].dir);
    }
    nlx();
    textattr(COLOR_YELLOW);
    Puts("(Q=Quit) Networks: (M)odify, (D)elete, (I)nsert : ");
    textattr(COLOR_CYAN);
    char ch = onek("Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M': {
      nlx();
      sprintf(s1, "Edit which (1-%d) ? ", initinfo.net_num_max);
      textattr(COLOR_YELLOW);
      Puts(s1);
      textattr(COLOR_CYAN);
      int nNetNumber = input_number(2);
      if (nNetNumber > 0 && nNetNumber <= initinfo.net_num_max) {
        edit_net(nNetNumber - 1);
      }
    }
    break;
    case 'D':
      if (!OKAD) {
        textattr(COLOR_RED);
        Printf("You must run the BBS once to set up some variables before deleting a network.\n");
        textattr(COLOR_CYAN);
        out->GetChar();
        break;
      }
      if (initinfo.net_num_max > 1) {
        nlx();
        textattr(COLOR_YELLOW);
        sprintf(s1, "Delete which (1-%d) ? ", initinfo.net_num_max);
        textattr(COLOR_CYAN);
        Puts(s1);
        int nNetNumber = input_number(2);
        if ((nNetNumber > 0) && (nNetNumber <= initinfo.net_num_max)) {
          nlx();
          textattr(COLOR_MAGENTA);
          Puts("Are you sure? ");
          textattr(COLOR_CYAN);
          ch = onek("YN\r");
          if (ch == 'Y') {
            nlx();
            textattr(COLOR_RED);
            Puts("Are you REALLY sure? ");
            textattr(COLOR_CYAN);
            ch = onek("YN\r");
            if (ch == 'Y') {
              del_net(nNetNumber - 1);
            }
          }
        }
      } else {
        nlx();
        textattr(COLOR_RED);
        Printf("You must leave at least one network.\n");
        textattr(COLOR_CYAN);
        nlx();
        out->GetChar();
      }
      break;
    case 'I':
      if (!OKAD) {
        textattr(COLOR_YELLOW);
        Printf("You must run the BBS once to set up some variables before inserting a network.\n");
        textattr(COLOR_CYAN);
        out->GetChar();
        break;
      }
      if (initinfo.net_num_max >= MAX_NETWORKS) {
        textattr(COLOR_RED);
        Printf("Too many networks.\n");
        textattr(COLOR_CYAN);
        nlx();
        out->GetChar();
        break;
      }
      nlx();
      textattr(COLOR_YELLOW);
      sprintf(s1, "Insert before which (1-%d) ? ", initinfo.net_num_max + 1);
      Puts(s1);
      textattr(COLOR_CYAN);
      int nNetNumber = input_number(2);
      if ((nNetNumber > 0) && (nNetNumber <= initinfo.net_num_max + 1)) {
        textattr(COLOR_YELLOW);
        Puts("Are you sure? ");
        textattr(COLOR_CYAN);
        ch = onek("YN\r");
        if (ch == 'Y') {
          insert_net(nNetNumber - 1);
        }
      }
      break;
    }
  } while (!done);

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%s%cnetworks.dat", syscfg.datadir, WWIV_FILE_SEPERATOR_CHAR);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
  close(hFile);
}
