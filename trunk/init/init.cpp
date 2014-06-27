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

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <curses.h>
#include <fcntl.h>
#include <memory>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <locale.h>
#include <sys/stat.h>

#include "archivers.h"
#include "editors.h"
#include "ifcns.h"
#include "input.h"
#include "init.h"
#include "instance_settings.h"
#include "languages.h"
#include "levels.h"
#include "networks.h"
#include "paths.h"
#include "protocols.h"
#include "regcode.h"
#include "system_info.h"
#include "user_editor.h"
#include "bbs/version.cpp"
#include "wwivinit.h"
#include "bbs/wconstants.h"
#include "platform/curses_io.h"
#include "platform/incl1.h"
#include "platform/wfndfile.h"

CursesIO* out;
char bbsdir[MAX_PATH];
char configdat[20] = "config.dat";


static void convcfg() {
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

static void printcfg() {
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
    out->GotoXY(0, 8);
    sprintf(s, "The %s directory: %s is invalid!", typeDir, dirName);
    textattr(COLOR_RED);
    out->Puts(s);
    for (unsigned int i = 0; i < strlen(s); i++) {
      Printf("\b \b");
    }
    if ((strcmp(typeDir, "Temporary") == 0) || (strcmp(typeDir, "Batch") == 0)) {
      sprintf(s, "Create %s? ", dirName);
      textattr(COLOR_GREEN);
      out->Puts(s);
      ch = out->GetChar();
      if (toupper(ch) == 'Y') {
        mkdir(dirName);
      }
      for (unsigned int j = 0; j < strlen(s); j++) {
        Printf("\b \b");
      }
    }
    textattr(COLOR_YELLOW);
    out->Puts("<ESC> when done.");
    textattr(COLOR_CYAN);
    rc = 1;
  }
  WWIV_ChangeDirTo(bbsdir);
  return rc;
}

static void show_help() {
  Printf("   -D    - Edit Directories\n");
  Printf("   -Pxxx - Password via commandline (where xxx is your password)\n");
  Printf("\n\n\n");

}

WInitApp::WInitApp() {
  out = new CursesIO();
}

WInitApp::~WInitApp() {
  delete out;
  out = nullptr;
}

int main(int argc, char* argv[]) {
  std::unique_ptr<WInitApp> app(new WInitApp());
  return app->main(argc, argv);
}

int WInitApp::main(int argc, char *argv[]) {
  char s[81], s1[81];
  int newbbs = 0, configfile, pwok = 0;
  int i;
  externalrec *oexterns;

  setlocale (LC_ALL,"");

  char *ss = getenv("WWIV_DIR");
  if (ss) {
    WWIV_ChangeDirTo(ss);
  }
  getcwd(bbsdir, MAX_PATH);

  trimstrpath(bbsdir);

  daylight = 0; // C Runtime Variable -- WHY? from init()

  out->Cls();
  textattr(COLOR_CYAN);

  configfile = open(configdat, O_RDWR | O_BINARY);
  if (configfile > 0) {
    // try to read it initially so we can process args right.
    read(configfile, &syscfg, sizeof(configrec));
    close(configfile);
  }
  for (i = 1; i < argc; ++i) {
    if (strlen(argv[i]) < 2) {
      continue;
    }

    if (argv[i][0] == '-') {
      char ch = toupper(argv[i][1]);
      switch (ch) {
      case 'S':
        printcfg();
        exit_init(0);
        break;
      case 'P': {
        if (strlen(argv[i]) > 2) {
          if (stricmp(argv[i] + 2, syscfg.systempw) == 0) {
            pwok = 1;
          }
        }
        break;
      }
      case 'D': {
        configfile = open(configdat, O_RDWR | O_BINARY);
        read(configfile, (void *)(&syscfg), sizeof(configrec));
        close(configfile);

        configfile = open("config.ovr", O_RDWR | O_BINARY);
        lseek(configfile, 0, SEEK_SET);
        read(configfile, &syscfgovr, sizeof(configoverrec));
        close(configfile);

        setpaths();
        out->Cls();
        exit_init(0);
      } break;
      case '?':
        show_help();
        exit_init(0);
        break;
      }
    }
  }

  configfile = open(configdat, O_RDWR | O_BINARY);
  if (configfile < 0) {
    textattr(COLOR_RED);
    Printf("%s NOT FOUND.\n\n", configdat);
    if (dialog_yn("Perform initial installation")) {
      new_init();
      messagebox("Your system password defaults to 'SYSOP'.");
      nlx();
      newbbs = 1;
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

  configfile = open("config.ovr", O_RDONLY | O_BINARY);
  if ((configfile > 0) && (filelength(configfile) < (int)sizeof(configoverrec))) {
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
  } else {
    lseek(configfile, 0, SEEK_SET);
    read(configfile, &syscfgovr, sizeof(configoverrec));
    close(configfile);
  }

  sprintf(s, "%sarchiver.dat", syscfg.datadir);
  int hFile = open(s, O_RDONLY | O_BINARY);
  if (hFile < 0) {
    create_arcs();
  } else {
    close(hFile);
  }

  if (!syscfg.dial_prefix[0]) {
    strcpy(syscfg.dial_prefix, "ATDT");
  }

  externs = (newexternalrec *) malloc(15 * sizeof(newexternalrec));
  editors = (editorrec *)   malloc(10 * sizeof(editorrec));
  initinfo.numeditors = initinfo.numexterns = 0;
  sprintf(s, "%snextern.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    initinfo.numexterns = (read(hFile, (void *)externs, 15 * sizeof(newexternalrec))) / sizeof(newexternalrec);
    close(hFile);
  } else {
    oexterns = (externalrec *) malloc(15 * sizeof(externalrec));
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
    free(oexterns);
  }
  over_intern = (newexternalrec *) malloc(3 * sizeof(newexternalrec));
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
    if ((status.net_version >= 31) || (status.net_version == 0)) {
      net_networks = (net_networks_rec *) malloc(MAX_NETWORKS * sizeof(net_networks_rec));
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

  languages = (languagerec*) malloc(MAX_LANGUAGES * sizeof(languagerec));
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

  if (languages->mdir[0] == 0) {
    strncpy(languages->mdir, syscfg.gfilesdir, sizeof(languages->mdir) - 1);
  }

  if (newbbs) {
    nlx();
    textattr(COLOR_CYAN);
    Printf("You will now need to enter the system password, 'SYSOP'.\n");
    nlx();
  }

  if (!pwok) {
    nlx();
    std::vector<std::string> lines { "Please enter the System Password. "};
    input_password("SY:", lines, s, 20);
    if (strcmp(s, (syscfg.systempw)) != 0) {
      textattr(COLOR_WHITE);
      out->Cls();
      nlx(2);
      textattr(COLOR_RED);
      Printf("I'm sorry, that isn't the correct system password.\n");
      textattr(COLOR_WHITE);
      exit_init(2);
    }
  }

  bool done = false;
  do {
    out->Cls();
    out->SetDefaultFooter();

    textattr(COLOR_CYAN);
    int y = 1;
    int x = 0;
    out->PutsXY(x, y++, "1. General System Configuration");
    out->PutsXY(x, y++, "2. System Paths");
    out->PutsXY(x, y++, "6. External Protocol Configuration");
    out->PutsXY(x, y++, "7. External Editor Configuration");
    out->PutsXY(x, y++, "8. Security Level Configuration");
    out->PutsXY(x, y++, "9. Auto-Validation Level Configuration");
    out->PutsXY(x, y++, "A. Archiver Configuration");
    out->PutsXY(x, y++, "I. Instance Configuration");
    out->PutsXY(x, y++, "L. Language Configuration");
    out->PutsXY(x, y++, "N. Network Configuration");
    out->PutsXY(x, y++, "R. Registration Information");
    out->PutsXY(x, y++, "U. User Editor");
    out->PutsXY(x, y++, "X. Update Sub/Directory Maximums");
    out->PutsXY(x, y++, "Q. Quit");

    werase(out->footer());
    y++;
    out->PutsXY(x, y++, "Command (1,2,6-9,A,I,L,N,Q,R,U,X) ? ");
    textattr(COLOR_CYAN);
    lines_listed = 0;
    switch (onek("Q126789AILNPRUVX\033")) {
    case 'Q':
    case '\033':
      textattr(COLOR_WHITE);
      done = true;
      break;
    case '1':
      out->SetDefaultFooter();
      sysinfo1();
      break;
    case '2':
      out->SetDefaultFooter();
      setpaths();
      break;
    case '6':
      out->SetDefaultFooter();
      extrn_prots();
      break;
    case '7':
      out->SetDefaultFooter();
      extrn_editors();
      break;
    case '8':
      out->SetDefaultFooter();
      sec_levs();
      break;
    case '9':
      out->SetDefaultFooter();
      autoval_levs();
      break;
    case 'A':
      out->SetDefaultFooter();
      edit_arc(0);
      break;
    case 'I':
      instance_editor();
      break;
    case 'L':
      out->SetDefaultFooter();
      edit_languages();
      break;
    case 'N':
      out->SetDefaultFooter();
      networks();
      break;
    case 'P':
      nlx();
      printcfg();
      out->GetChar();
      break;
    case 'R':
      out->SetDefaultFooter();
      edit_registration_code();
      break;
    case 'U':
      user_editor();
      break;
    case 'V':
      nlx();
      Printf("WWIV %s%s INIT compiled %s\n", wwiv_version, beta_version, const_cast<char*>(wwiv_date));
      out->GetChar();
      break;
    case 'X':
      out->SetDefaultFooter();
      up_subs_dirs();
      break;
    }
  } while (!done);

  // Don't leak the localIO (also fix the color when the app exits)
  delete out;
  out = nullptr;
  return 0;
}
