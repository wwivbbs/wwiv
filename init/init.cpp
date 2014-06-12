/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#define _DEFINE_GLOBALS_

#include <curses.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "regcode.h"
#include "version.cpp"
#include "wwivinit.h"
#include "wconstants.h"
#include "platform/curses_io.h"
#include "platform/incl1.h"
#include "platform/wfndfile.h"

char **mdm_desc;
int mdm_count = 0, mdm_cur;

autosel_data *autosel_info;
int num_autosel;

resultrec *result_codes;
int num_result_codes;

#define MODEM_NOTES_LEN 4096
char modem_notes[MODEM_NOTES_LEN];

char bbsdir[MAX_PATH];

int inst = 1;
char configdat[20] = "config.dat";
char statusdat[20] = "status.dat";
char modemdat[20] = "modem.dat";

const char* g_pszCopyrightString = "Copyright (c) 1998-2014, WWIV Software Services";

void load_modems() {
  if (mdm_count == 0) {
    Printf("Please wait...\n");
    get_descriptions(syscfg.datadir, &mdm_desc, &mdm_count, &autosel_info, &num_autosel);

    for (int i = 0; i < mdm_count; i++) {
      if (strncmp(mdm_desc[i], syscfgovr.modem_type, strlen(syscfgovr.modem_type)) == 0) {
        mdm_cur = i;
        break;
      }
    }
  }
}


int set_modem_info(const char *mt, bool bPause) {
  nlx();
  char szModemDatFileName[ MAX_PATH ];
  char szRawFileName[ MAX_PATH ];
  sprintf(szRawFileName, "%s%s.mdm", syscfg.datadir, mt);
  sprintf(szModemDatFileName, "%s%s", syscfg.datadir, modemdat);
  unsigned int nModemBaud;
  char szConfigLine[161];
  if (compile(szRawFileName, szModemDatFileName, modem_notes, MODEM_NOTES_LEN, szConfigLine, &nModemBaud)) {
    strcpy(syscfgovr.modem_type, mt);
    Printf("Modem info '%s' compiled.\n", syscfgovr.modem_type);
    save_config();
    if (modem_notes[0]) {
      // there are some notes about the modem
      Printf("%s\n", modem_notes);
      if (bPause) {
        textattr(COLOR_MAGENTA);
        Printf("\n[PAUSE]");
        textattr(COLOR_CYAN);
        app->localIO->getchd();
        Printf("\n");
      }
    }
    return 1;
  } else {
    textattr(COLOR_RED);
    Printf("Modem info for '%s' does not exist.\n", mt);
    textattr(COLOR_CYAN);
    nlx();
    return 0;
  }
}



/****************************************************************************/


char *mdm_name(int mdm_num) {
  static char s_szModemName[ 255 ];

  strcpy(s_szModemName, mdm_desc[mdm_num]);
  char *pszDelim = strchr(s_szModemName, ' ');
  if (pszDelim) {
    *pszDelim = '\0';
  }

  return s_szModemName;
}


// select modem type
void select_modem() {
  app->localIO->LocalCls();
  load_modems();
  app->localIO->LocalCls();

  if (mdm_count == 0) {
    textattr(COLOR_RED);
    Printf("No modems defined.\n");
    textattr(COLOR_MAGENTA);
    Printf("Press Any Key");
    app->localIO->getchd();
    return;
  }

  textattr(COLOR_CYAN);
  Printf("Select modem type: ");
  textattr(COLOR_YELLOW);
  Printf("<UP>,<DN>,<P-UP>,<P-DN> ");
  textattr(COLOR_BLUE);
  Printf("to scroll, ");
  textattr(COLOR_YELLOW);
  Printf("<CR>");
  textattr(COLOR_BLUE);
  Printf("=select, ");
  textattr(COLOR_YELLOW);
  Printf("<ESC>");
  textattr(COLOR_BLUE);
  Printf("=abort\n");
  textattr(COLOR_YELLOW);
  Printf("Current: ");
  textattr(COLOR_GREEN);
  Printf("%s\n", mdm_desc[mdm_cur]);
  textattr(COLOR_CYAN);

  int nModemNumber = select_strings(mdm_desc, mdm_count, mdm_cur, 3, 24, 0, 79);

  if (nModemNumber != -1) {
    app->localIO->LocalCls();
    if (set_modem_info(mdm_name(nModemNumber), true)) {
      mdm_cur = nModemNumber;
      save_config();
    }
  }
}

static const char *nettypes[] = {
  "WWIVnet ",
  "Fido    ",
  "Internet",
};

static const int MAX_NETTYPES = sizeof(nettypes)/sizeof(nettypes[0]);

void edit_net(int nn) {
  char szOldNetworkName[20];
  char *ss;

  app->localIO->LocalCls();
  bool done = false;
  int cp = 1;
  net_networks_rec *n = &(net_networks[nn]);
  strcpy(szOldNetworkName, n->name);

  if (n->type >= MAX_NETTYPES) {
    n->type = 0;
  }

  Printf("Network type   : %s\n\n", nettypes[n->type]);

  Printf("Network name   : %s\n", n->name);
  Printf("Node number    : %u\n", n->sysnum);
  Printf("Data Directory : %s\n", n->dir);
  textattr(COLOR_YELLOW);
  Puts("\n<ESC> when done.\n\n");
  textattr(COLOR_CYAN);
  do {
    if (cp) {
      app->localIO->LocalGotoXY(17, cp + 1);
    } else {
      app->localIO->LocalGotoXY(17, cp);
    }
    int nNext = 0;
    switch (cp) {
    case 0:
      n->type = toggleitem(n->type, nettypes, MAX_NETTYPES, &nNext);
      break;
    case 1: {
      editline(n->name, 15, ALL, &nNext, "");
      trimstr(n->name);
      ss = strchr(n->name, ' ');
      if (ss) {
        *ss = 0;
      }
      Puts(n->name);
      Puts("                  ");
    }
    break;
    case 2: {
      char szTempBuffer[ 255 ];
      sprintf(szTempBuffer, "%u", n->sysnum);
      editline(szTempBuffer, 5, NUM_ONLY, &nNext, "");
      trimstr(szTempBuffer);
      n->sysnum = atoi(szTempBuffer);
      sprintf(szTempBuffer, "%u", n->sysnum);
      Puts(szTempBuffer);
    }
    break;
    case 3: {
      editline(n->dir, 60, UPPER_ONLY, &nNext, "");
      trimstrpath(n->dir);
      Puts(n->dir);
    }
    break;
    }
    cp = GetNextSelectionPosition(0, 3, cp, nNext);
    if (nNext == DONE) {
      done = true;
    }
  } while (!done && !hangup);

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
              if (stricmp(szOldNetworkName, szBuffer + 1) == 0) {
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

#define OKAD (syscfg.fnoffset && syscfg.fsoffset && syscfg.fuoffset)

void networks() {
  char s1[81];
  bool done = false;

  if (!exist("NETWORK.EXE")) {
    app->localIO->LocalCls();
    nlx();
    textattr(COLOR_RED);
    Printf("You have not installed the networking software.  Unzip NETxx.ZIP\n");
    Printf("to the main BBS directory and re-run INIT.\n\n");
    textattr(COLOR_MAGENTA);
    Printf("Hit any key to return to the Main Menu.\n");
    app->localIO->getchd();
    return;
  }

  do {
    app->localIO->LocalCls();
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
        app->localIO->getchd();
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
        app->localIO->getchd();
      }
      break;
    case 'I':
      if (!OKAD) {
        textattr(COLOR_YELLOW);
        Printf("You must run the BBS once to set up some variables before inserting a network.\n");
        textattr(COLOR_CYAN);
        app->localIO->getchd();
        break;
      }
      if (initinfo.net_num_max >= MAX_NETWORKS) {
        textattr(COLOR_RED);
        Printf("Too many networks.\n");
        textattr(COLOR_CYAN);
        nlx();
        app->localIO->getchd();
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

void tweak_dir(char *s) {
  if (inst == 1) {
    return;
  }

  int i = strlen(s);
  if (i == 0) {
    sprintf(s, "temp%d", inst);
  } else {
    char *lcp = s + i - 1;
    while ((((*lcp >= '0') && (*lcp <= '9')) || (*lcp == WWIV_FILE_SEPERATOR_CHAR)) && (lcp >= s)) {
      lcp--;
    }
    sprintf(lcp + 1, "%d%c", inst, WWIV_FILE_SEPERATOR_CHAR);
  }
}

/****************************************************************************/


void convcfg() {
  arcrec arc[MAX_ARCS];

  int hFile = open(configdat, O_RDWR | O_BINARY);
  if (hFile > 0) {
    textattr(COLOR_YELLOW);
    Printf("Converting config.dat to 4.30/5.00 format...\n");
    textattr(COLOR_CYAN);
    read(hFile, (void *)(&syscfg), sizeof(configrec));
    sprintf(syscfg.menudir, "%smenus%c", syscfg.gfilesdir, WWIV_FILE_SEPERATOR_CHAR);
    strcpy(syscfg.logoff_c, " ");
    strcpy(syscfg.v_scan_c, " ");

    for (int i = 0; i < MAX_ARCS; i++) {
      if ((syscfg.arcs[i].extension[0]) && (i < 4)) {
        strncpy(arc[i].name, syscfg.arcs[i].extension, 32);
        strncpy(arc[i].extension, syscfg.arcs[i].extension, 4);
        strncpy(arc[i].arca, syscfg.arcs[i].arca, 32);
        strncpy(arc[i].arce, syscfg.arcs[i].arce, 32);
        strncpy(arc[i].arcl, syscfg.arcs[i].arcl, 32);
      } else {
        strncpy(arc[i].name, "New Archiver Name", 32);
        strncpy(arc[i].extension, "EXT", 4);
        strncpy(arc[i].arca, "archive add command", 32);
        strncpy(arc[i].arce, "archive extract command", 32);
        strncpy(arc[i].arcl, "archive list command", 32);
      }
    }
    lseek(hFile, 0L, SEEK_SET);
    write(hFile, (void *)(&syscfg), sizeof(configrec));
    close(hFile);

    char szFileName[ MAX_PATH ];
    sprintf(szFileName, "%sarchiver.dat", syscfg.datadir);
    hFile = open(szFileName, O_WRONLY | O_BINARY | O_CREAT);
    if (hFile < 0) {
      Printf("Couldn't open '%s' for writing.\n", szFileName);
      Printf("Creating new file....");
      create_arcs();
      Printf("\n");
      hFile = open(szFileName, O_WRONLY | O_BINARY | O_CREAT);
    }
    write(hFile, (void *)arc, MAX_ARCS * sizeof(arcrec));
    close(hFile);
  }
}


void printcfg() {
  int hFile = open(configdat, O_RDWR | O_BINARY);
  if (hFile > 0) {
    read(hFile, (void *)(&syscfg), sizeof(configrec));
    Printf("syscfg.newuserpw            = %s\n", syscfg.newuserpw);
    Printf("syscfg.systempw             = %s\n", syscfg.systempw);
    Printf("syscfg.msgsdir              = %s\n", syscfg.msgsdir);
    Printf("syscfg.gfilesdir            = %s\n", syscfg.gfilesdir);
    Printf("syscfg.dloadsdir            = %s\n", syscfg.dloadsdir);
    Printf("syscfg.menudir              = %s\n", syscfg.menudir);

    for (int i = 0; i < 4; i++) {
      Printf("syscfg.arcs[%d].extension   = %s\n", i, syscfg.arcs[i].extension);
      Printf("syscfg.arcs[%d].arca        = %s\n", i, syscfg.arcs[i].arca);
      Printf("syscfg.arcs[%d].arce        = %s\n", i, syscfg.arcs[i].arce);
      Printf("syscfg.arcs[%d].arcl        = %s\n", i, syscfg.arcs[i].arcl);
    }
  }
  close(hFile);
}


int verify_dir(char *typeDir, char *dirName) {
  int rc = 0;
  char s[81], ch;

  WFindFile fnd;
  fnd.open(dirName, 0);

  if (fnd.next() && fnd.IsDirectory()) {
    app->localIO->LocalGotoXY(0, 8);
    sprintf(s, "The %s directory: %s is invalid!", typeDir, dirName);
    textattr(COLOR_RED);
    app->localIO->LocalPuts(s);
    for (unsigned int i = 0; i < strlen(s); i++) {
      Printf("\b \b");
    }
    if ((strcmp(typeDir, "Temporary") == 0) || (strcmp(typeDir, "Batch") == 0)) {
      sprintf(s, "Create %s? ", dirName);
      textattr(COLOR_GREEN);
      app->localIO->LocalPuts(s);
      ch = app->localIO->getchd();
      if (toupper(ch) == 'Y') {
        mkdir(dirName);
      }
      for (unsigned int j = 0; j < strlen(s); j++) {
        Printf("\b \b");
      }
    }
    textattr(COLOR_YELLOW);
    app->localIO->LocalPuts("<ESC> when done.");
    textattr(COLOR_CYAN);
    rc = 1;
  }
  WWIV_ChangeDirTo(bbsdir);
  return rc;
}

void show_help() {
  Printf("   ,x    - Specify Instance (where x is the Instance)\n");
  Printf("   -C    - Recompile modem configuration\n");
  Printf("   -D    - Edit Directories\n");
  Printf("   -L    - Rebuild LOCAL.MDM info\n");
  Printf("   -Pxxx - Password via commandline (where xxx is your password)\n");
  Printf("\n\n\n");

}

int main(int argc, char* argv[]) {
  app = new WInitApp();
  return app->main(argc, argv);
}


int WInitApp::main(int argc, char *argv[]) {
  localIO = new CursesIO();

  char s[81], s1[81], ch;
  int i1, newbbs = 0, configfile, pwok = 0;
  int i;
  int max_inst = 2;
  int hFile = -1;
  externalrec *oexterns;

  strcpy(str_yes, "Yes");
  strcpy(str_no, "No");

  char *ss = getenv("BBS");
  if (ss && (strncmp(ss, "WWIV", 4) == 0)) {
    textattr(COLOR_RED);
    Printf("\n\nYou can not run the initialization program from a subshell of the BBS.\n");
    Printf("You must exit the BBS or run INIT from a new Command Shell\n");
    textattr(COLOR_WHITE);
    exit_init(2);
  }
  ss = getenv("WWIV_DIR");
  if (ss) {
    WWIV_ChangeDirTo(ss);
  }
  getcwd(bbsdir, MAX_PATH);

  trimstrpath(bbsdir);

  init();

  app->localIO->LocalCls();
  textattr((16 * COLOR_BLUE) + COLOR_WHITE);
  Printf("WWIV %s%s Initialization/Configuration Program.", wwiv_version, beta_version);
  app->localIO->LocalClrEol();
  nlx();
  Printf(g_pszCopyrightString);
  app->localIO->LocalClrEol();
  textattr(COLOR_WHITE);
  nlx(2);
  textattr(COLOR_CYAN);

  char *pszInstanceNumber = getenv("WWIV_INSTANCE");
  if (pszInstanceNumber) {
    inst = atoi(pszInstanceNumber);
  }

  for (i = 1; i < argc; ++i) {
    if (i == 1 && argv[i][0] == '?') {
      show_help();
      exit_init(0);
    }

    if (argv[i][0] == ',') {
      int nInstanceNumber = atoi(argv[i] + 1);
      if (nInstanceNumber >= 1 && nInstanceNumber < 1000) {
        inst = nInstanceNumber;
      }
      break;
    }
    if (argv[i][0] == 'P' || argv[i][0] == 'p') {
      printcfg();
      break;
    }

    if ((argv[i][1] == 'D') || (argv[i][1] == 'd')) {
      configfile = open(configdat, O_RDWR | O_BINARY);
      read(configfile, (void *)(&syscfg), sizeof(configrec));

      configfile = open("config.ovr", O_RDWR | O_BINARY);
      lseek(configfile, (inst - 1)*sizeof(configoverrec), SEEK_SET);
      read(configfile, &syscfgovr, sizeof(configoverrec));

      setpaths();
      app->localIO->LocalCls();
      exit_init(0);
    }
  }

  if (inst < 1 || inst >= 1000) {
    inst = 1;
  }
  if (inst != 1) {
    sprintf(modemdat, "modem.%03.3d", inst);
  }

  bool done = false;
  configfile = open(configdat, O_RDWR | O_BINARY);
  if (configfile < 0) {
    textattr(COLOR_RED);
    Printf("%s NOT FOUND.\n\n", configdat);
    inst = 1;
    strcpy(modemdat, "modem.dat");
    textattr(COLOR_YELLOW);
    Puts("Perform initial installation? ");
    textattr(COLOR_CYAN);
    if (yn()) {
      if (inst == 1) {
        new_init();
        nlx(1);
        textattr(COLOR_YELLOW);
        Printf("Your system password defaults to 'SYSOP'.\n");
        textattr(COLOR_CYAN);
        nlx();
        newbbs = 1;
      }
      configfile = open(configdat, O_RDWR | O_BINARY);
    } else {
      textattr(COLOR_WHITE);
      exit_init(1);
    }
  }

  if (filelength(configfile) != sizeof(configrec)) {
    close(configfile);
    convcfg();
    configfile = open(configdat, O_RDWR | O_BINARY);
  }

  read(configfile, &syscfg, sizeof(configrec));
  close(configfile);

  max_inst = 999;

  configfile = open("config.ovr", O_RDONLY | O_BINARY);
  if ((configfile > 0) && (filelength(configfile) < inst * (int)sizeof(configoverrec))) {
    close(configfile);
    configfile = -1;
  }
  if (configfile < 0) {
    // slap in the defaults
    for (i = 0; i < 4; i++) {
      syscfgovr.com_ISR[i + 1] = syscfg.com_ISR[i + 1];
      syscfgovr.com_base[i + 1] = syscfg.com_base[i + 1];
      syscfgovr.com_ISR[i + 5] = syscfg.com_ISR[i + 1];
      syscfgovr.com_base[i + 5] = syscfg.com_base[i + 1];
    }

    syscfgovr.com_ISR[0] = syscfg.com_ISR[1];
    syscfgovr.com_base[0] = syscfg.com_base[1];

    syscfgovr.primaryport = syscfg.primaryport;
    strcpy(syscfgovr.modem_type, syscfg.modem_type);
    strcpy(syscfgovr.tempdir, syscfg.tempdir);
    strcpy(syscfgovr.batchdir, syscfg.batchdir);
    if (syscfg.sysconfig & sysconfig_high_speed) {
      syscfgovr.comflags |= comflags_buffered_uart;
    }
    tweak_dir(syscfgovr.tempdir);
    tweak_dir(syscfgovr.batchdir);
  } else {
    lseek(configfile, (inst - 1)*sizeof(configoverrec), SEEK_SET);
    read(configfile, &syscfgovr, sizeof(configoverrec));
    close(configfile);
  }

  sprintf(s, "%sarchiver.dat", syscfg.datadir);
  hFile = open(s, O_RDONLY | O_BINARY);
  if (hFile < 0) {
    create_arcs();
  } else {
    close(hFile);
  }

  if (inst > 1) {
    strcpy(s, syscfgovr.tempdir);
    i = strlen(s);
    if (s[0] == 0) {
      i1 = 1;
    } else  {
      // This is on because on UNIX we are not going to have :/ in the filename.
      // This is just to trim the trailing slash unless it's a path like "C:\"
      if ((s[i - 1] == WWIV_FILE_SEPERATOR_CHAR) && (s[i - 2] != ':')) {
        s[i - 1] = 0;
      }
      i1 = chdir(s);
    }
    if (i1 != 0) {
      mkdir(s);
    } else {
      WWIV_ChangeDirTo(bbsdir);
    }

    strcpy(s, syscfgovr.batchdir);
    i = strlen(s);
    if (s[0] == 0) {
      i1 = 1;
    } else {
      // See comment above.
      if ((s[i - 1] == WWIV_FILE_SEPERATOR_CHAR) && (s[i - 2] != ':')) {
        s[i - 1] = 0;
      }
      i1 = chdir(s);
    }
    if (i1) {
      mkdir(s);
    } else {
      WWIV_ChangeDirTo(bbsdir);
    }
  }

  result_codes = (resultrec *) bbsmalloc(40 * sizeof(resultrec));
  sprintf(s, "%sresults.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    num_result_codes = (read(hFile, result_codes, 40 * sizeof(resultrec))) / sizeof(resultrec);
    close(hFile);
  } else {
#ifdef ASFD
    Printf("\nConverting result codes...\n");
    convert_result_codes();
#endif
  }
  if (!syscfg.dial_prefix[0]) {
    strcpy(syscfg.dial_prefix, "ATDT");
  }

  thisuser.screenchars = 80;
  thisuser.screenlines = 25;
  thisuser.sysstatus = 0L;
  externs = (newexternalrec *) bbsmalloc(15 * sizeof(newexternalrec));
  editors = (editorrec *)   bbsmalloc(10 * sizeof(editorrec));
  initinfo.numeditors = initinfo.numexterns = 0;
  sprintf(s, "%snextern.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    initinfo.numexterns = (read(hFile, (void *)externs, 15 * sizeof(newexternalrec))) / sizeof(newexternalrec);
    close(hFile);
  } else {
    oexterns = (externalrec *) bbsmalloc(15 * sizeof(externalrec));
    sprintf(s1, "%sextern.dat", syscfg.datadir);
    hFile = open(s1, O_RDONLY | O_BINARY);
    if (hFile > 0) {
      initinfo.numexterns = (read(hFile, (void *)oexterns, 15 * sizeof(externalrec))) / sizeof(externalrec);
      close(hFile);
      memset(externs, 0, 15 * sizeof(newexternalrec));
      for (i = 0; i < initinfo.numexterns; i++) {
        strcpy(externs[i].description, oexterns[i].description);
        strcpy(externs[i].receivefn, oexterns[i].receivefn);
        strcpy(externs[i].sendfn, oexterns[i].sendfn);
        externs[i].ok1 = oexterns[i].ok1;
      }
      hFile = open(s, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
      if (hFile > 0) {
        write(hFile, externs, initinfo.numexterns * sizeof(newexternalrec));
        close(hFile);
      }
    }
    BbsFreeMemory(oexterns);
  }
  over_intern = (newexternalrec *) bbsmalloc(3 * sizeof(newexternalrec));
  memset(over_intern, 0, 3 * sizeof(newexternalrec));
  sprintf(s, "%snintern.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    read(hFile, over_intern, 3 * sizeof(newexternalrec));
    close(hFile);
  }
  sprintf(s, "%seditors.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    initinfo.numeditors = (read(hFile, (void *)editors, 10 * sizeof(editorrec))) / sizeof(editorrec);
    initinfo.numeditors = initinfo.numeditors;
    close(hFile);
  }

  bool bDataDirectoryOk = read_status();
  if (bDataDirectoryOk) {
    sprintf(s, "%s%s", syscfg.datadir, modemdat);
    if (!exist(s) && (inst == 1)) {
      if (syscfgovr.modem_type[0]) {
        if (!set_modem_info(syscfgovr.modem_type, true)) {
          convert_modem_info("LOCAL");
          set_modem_info("LOCAL", true);
        }
      } else {
        convert_modem_info("LOCAL");
        set_modem_info("LOCAL", true);
      }
      save_config();
    }

    if ((status.net_version >= 31) || (status.net_version == 0)) {
      net_networks = (net_networks_rec *) bbsmalloc(MAX_NETWORKS * sizeof(net_networks_rec));
      if (!net_networks) {
        Printf("needed %d bytes\n", MAX_NETWORKS * sizeof(net_networks_rec));
        textattr(COLOR_WHITE);
        exit_init(2);
      }
      memset(net_networks, 0, MAX_NETWORKS * sizeof(net_networks_rec));

      sprintf(s, "%snetworks.dat", syscfg.datadir);
      hFile = open(s, O_RDONLY | O_BINARY);
      if (hFile > 0) {
        initinfo.net_num_max = filelength(hFile) / sizeof(net_networks_rec);
        if (initinfo.net_num_max > MAX_NETWORKS) {
          initinfo.net_num_max = MAX_NETWORKS;
        }
        if (initinfo.net_num_max) {
          read(hFile, net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
        }
        close(hFile);
      }

      /******************************
      if (!initinfo.net_num_max) {
      initinfo.net_num_max=1;
      strcpy(net_networks->name,"NewNet");
      strcpy(net_networks->dir, bbsdir);
      strcat(net_networks->dir, "NEWNET\\");
      net_networks->sysnum=9999;
      hFile=open(s,O_RDWR|O_BINARY|O_CREAT,S_IREAD|S_IWRITE);
      if (hFile) {
      write(i,net_networks,initinfo.net_num_max*sizeof(net_networks_rec));
      close(i);
      }
      }
      ******************************/
    }
  }

  languages = (languagerec*) bbsmalloc(MAX_LANGUAGES * sizeof(languagerec));
  if (!languages) {
    Printf("needed %d bytes\n", MAX_LANGUAGES * sizeof(languagerec));
    textattr(COLOR_WHITE);
    exit_init(2);
  }

  sprintf(s, "%slanguage.dat", syscfg.datadir);
  i = open(s, O_RDONLY | O_BINARY);
  if (i >= 0) {
    initinfo.num_languages = filelength(i) / sizeof(languagerec);
    read(i, languages, initinfo.num_languages * sizeof(languagerec));
    close(i);
  }
  if (!initinfo.num_languages) {
    initinfo.num_languages = 1;
    strcpy(languages->name, "English");
    strncpy(languages->dir, syscfg.gfilesdir, sizeof(languages->dir) - 1);
    strncpy(languages->mdir, syscfg.gfilesdir, sizeof(languages->mdir) - 1);
  }
  c_setup();

  if (languages->mdir[0] == 0) {
    strncpy(languages->mdir, syscfg.gfilesdir, sizeof(languages->mdir) - 1);
  }

  for (i = 1; i < argc; i++) {
    strcpy(s, argv[i]);
    if ((s[0] == '-') || (s[0] == '/')) {
      ch = upcase(s[1]);
      switch (ch) {
      case 'C': // Compile modem info
        if (s[2]) {
          strcpy(s1, s + 2);
          s1[8] = 0;
          for (i1 = 0; s1[i1]; i1++) {
            s1[i1] = upcase(s1[i1]);
          }
        } else {
          strcpy(s1, syscfgovr.modem_type);
        }
        set_modem_info(s1, false);
        save_config();
        textattr(COLOR_WHITE);
        app->localIO->LocalCls();
        nlx(2);
        Printf("Modem configured....\n");
        nlx(2);
        exit_init(0);
        break;
      case 'L': // re-build local modem info
        convert_modem_info("LOCAL");
        set_modem_info("LOCAL", false);
        save_config();
        textattr(COLOR_WHITE);
        exit_init(0);
      case 'P': // Enter password on commandline
        if (stricmp(s + 2, syscfg.systempw) == 0) {
          pwok = 1;
        }
        break;
      case '?':   // Display usage information
        show_help();
        break;
      }
    }
  }

  if (newbbs) {
    nlx();
    textattr(COLOR_CYAN);
    Printf("You will now need to enter the system password, 'SYSOP'.\n");
    nlx();
  }

  if (!pwok) {
    nlx();
    textattr(COLOR_CYAN);
    Puts("SY: ");
    input_password(s, 20);
    if (strcmp(s, (syscfg.systempw)) != 0) {
      textattr(COLOR_WHITE);
      app->localIO->LocalCls();
      nlx(2);
      textattr(COLOR_RED);
      Printf("I'm sorry, that isn't the correct system password.\n");
      textattr(COLOR_WHITE);
      exit_init(2);
    }
  }

  if (bDataDirectoryOk) {
    if (c_IsUserListInOldFormat()) {
      nlx();
      if (c_check_old_struct()) {
        textattr(COLOR_RED);
        Printf("You have a non-standard userlist.\n");
        Printf("Please update the convert.c file with your old userrec, make any\n");
        Printf("modifications necessary to copy over new fields, then compile and\n");
        Printf("run it.\n");
        textattr(COLOR_MAGENTA);
        Puts("[PAUSE]");
        textattr(COLOR_CYAN);
        app->localIO->getchd();
        nlx();
      } else {
        textattr(COLOR_YELLOW);
        Puts("Convert your userlist to the v4.30 format? ");
        textattr(COLOR_CYAN);
        if (yn()) {
          textattr(COLOR_MAGENTA);
          Printf("Please wait...\n");
          c_old_to_new();
          nlx();
          textattr(COLOR_MAGENTA);
          Puts("[PAUSE]");
          textattr(COLOR_CYAN);
          app->localIO->getchd();
          nlx();
        }
      }
    }
  }

  do {
    app->localIO->LocalCls();
    textattr((16 * COLOR_BLUE) | COLOR_WHITE);
    Printf("WWIV %s%s Initialization/Configuration Program.", wwiv_version, beta_version);
    app->localIO->LocalClrEol();
    nlx();
    Puts(g_pszCopyrightString);
    app->localIO->LocalClrEol();
    textattr(COLOR_CYAN);
    int y = 3;
    int x = 0;
    app->localIO->LocalXYPuts(x, y++, "1. General System Configuration");
    app->localIO->LocalXYPuts(x, y++, "2. System Paths");
#if !defined (_unix__)
    app->localIO->LocalXYPuts(x, y++, "3. Communications Port Configuration");
    char szTempBuffer[ 255 ];
    sprintf(szTempBuffer, "5. Manually Select Modem Type (now %s)",
            !syscfgovr.modem_type[0] ? ">UNSET<" : syscfgovr.modem_type);
    app->localIO->LocalXYPuts(x, y++, szTempBuffer);
#endif
    app->localIO->LocalXYPuts(x, y++, "6. External Protocol Configuration");
    app->localIO->LocalXYPuts(x, y++, "7. External Editor Configuration");
    app->localIO->LocalXYPuts(x, y++, "8. Security Level Configuration");
    app->localIO->LocalXYPuts(x, y++, "9. Auto-Validation Level Configuration");
    app->localIO->LocalXYPuts(x, y++, "A. Archiver Configuration");
    app->localIO->LocalXYPuts(x, y++, "L. Language Configuration");
    app->localIO->LocalXYPuts(x, y++, "N. Network Configuration");
    app->localIO->LocalXYPuts(x, y++, "R. Registration Information");
    app->localIO->LocalXYPuts(x, y++, "U. Update Sub/Directory Maximums");
    app->localIO->LocalXYPuts(x, y++, "Q. Quit");

    y++;
    sprintf(szTempBuffer, "Instance %d: Which (1-9, A,L,N,R,U,Q) ? ", inst);
    app->localIO->LocalXYPuts(x, y++, szTempBuffer);
    textattr(COLOR_CYAN);
    lines_listed = 0;
    switch (onek("Q123456789ALNPRUVZ\033")) {
    case 'Q':
    case '\033':
      textattr(COLOR_WHITE);
      done = true;
      break;
    case '1':
      sysinfo1();
      break;
    case '2':
      setpaths();
      break;
    case '3':
#if !defined (__Unix__)
      setupcom();
#endif
      break;
    case '5':
#if !defined (__unix__)
      select_modem();
#endif
      break;
    case '6':
      extrn_prots();
      break;
    case '7':
      extrn_editors();
      break;
    case '8':
      sec_levs();
      break;
    case '9':
      autoval_levs();
      break;
    case 'A':
      edit_arc(0);
      break;
    case 'N':
      networks();
      break;
    case 'U':
      up_subs_dirs();
      break;
    case 'L':
      up_langs();
      break;
    case 'P':
      nlx();
      printcfg();
      app->localIO->getchd();
      break;
    case 'R':
      edit_registration_code();
      break;
    case 'V':
      nlx();
      Printf("WWIV %s%s INIT compiled %s\n", wwiv_version, beta_version, const_cast<char*>(wwiv_date));
      app->localIO->getchd();
      break;
    }
  } while (!done);

  app->localIO->LocalCls();
  // Don't leak the localIO (also fix the color when the app exits)
  delete app->localIO;
  return 0;
}

WInitApp* GetApplication() {
  return app;
}
