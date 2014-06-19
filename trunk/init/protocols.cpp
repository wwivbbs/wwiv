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
#include "template.h"

#include <curses.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "platform/incl1.h"
#include "wwivinit.h"


static const char *prot_name(int pn) {
  switch (pn) {
  case 1:
    return "ASCII";
  case 2:
    return "Xmodem";
  case 3:
    return "Xmodem-CRC";
  case 4:
    return "Ymodem";
  case 5:
    return "Batch";
  default:
    if (pn > 5 || pn < (initinfo.numexterns + 6)) {
      return externs[pn - 6].description;
    }
  }
  return ">NONE<";
}

static void edit_prot(int n) {
  char s[81], s1[81];
  newexternalrec c;

  out->Cls();
  if (n >= 6) {
    c = externs[n - 6];
  } else {
    c = over_intern[n - 2];
    strcpy(c.description, prot_name(n));
    strcpy(c.receivebatchfn, "-- N/A --");
    strcpy(c.bibatchfn, "-- N/A --");
    if (n != 4) {
      strcpy(c.sendbatchfn, "-- N/A --");
    }
  }
  bool done = false;
  int cp = 0;
  int i1 = NEXT;
  sprintf(s, "%u", c.ok1);
  if (c.othr & othr_error_correct) {
    strcpy(s1, "Y");
  } else {
    strcpy(s1, "N");
  }
  Printf("Description          : %s\n", c.description);
  Printf("Xfer OK code         : %s\n", s);
  Printf("Require MNP/LAPM     : %s\n", s1);
  Printf("Receive command line:\n%s\n", c.receivefn);
  Printf("Send command line:\n%s\n", c.sendfn);
  Printf("Receive batch command line:\n%s\n", c.receivebatchfn);
  Printf("Send batch command line:\n%s\n", c.sendbatchfn);
  Printf("Bi-directional transfer command line:\n%s\n", c.bibatchfn);
  textattr(COLOR_YELLOW);
  Puts("\n<ESC> when done.\n\n");
  textattr(11);
  Printf("%%1 = com port baud rate\n");
  Printf("%%2 = port number\n");
  Printf("%%3 = filename to send/receive, filename list to send for batch\n");
  Printf("%%4 = modem speed\n");
  Printf("%%5 = filename list to receive for batch UL and bi-directional batch\n");
  nlx();
  textattr(COLOR_MAGENTA);
  Printf("NOTE: Batch protocols >MUST< correctly support DSZLOG.\n");
  textattr(COLOR_CYAN);

  do {
    if (cp < 3) {
      out->GotoXY(23, cp);
    } else {
      out->GotoXY(0, cp * 2 - 2);
    }
    switch (cp) {
    case 0:
      if (n >= 6) {
        editline(c.description, 50, ALL, &i1, "");
        trimstr(c.description);
      }
      break;
    case 1:
      editline(s, 3, NUM_ONLY, &i1, "");
      trimstr(s);
      c.ok1 = atoi(s);
      sprintf(s, "%u", c.ok1);
      Puts(s);
      break;
    case 2:
      if (n >= 6) {
        editline(s1, 1, UPPER_ONLY, &i1, "");
        if (s1[0] != 'Y') {
          s1[0] = 'N';
        }
        s1[1] = 0;
        if (s1[0] == 'Y') {
          c.othr |= othr_error_correct;
        } else {
          c.othr &= (~othr_error_correct);
        }
        out->Puts(s1);
      }
      break;
    case 3:
      editline(c.receivefn, 78, ALL, &i1, "");
      trimstr(c.receivefn);
      if (c.sendfn[0] == 0) {
        strcpy(c.sendfn, c.receivefn);
      }
      if (c.sendbatchfn[0] == 0) {
        strcpy(c.sendbatchfn, c.receivefn);
      }
      if (c.receivebatchfn[0] == 0) {
        strcpy(c.receivebatchfn, c.receivefn);
      }
      break;
    case 4:
      editline(c.sendfn, 78, ALL, &i1, "");
      trimstr(c.sendfn);
      break;
    case 5:
      if (n >= 6) {
        editline(c.receivebatchfn, 78, ALL, &i1, "");
        trimstr(c.receivebatchfn);
      }
      break;
    case 6:
      if (n >= 4) {
        editline(c.sendbatchfn, 78, ALL, &i1, "");
        trimstr(c.sendbatchfn);
      }
      break;
    case 7:
      if (n >= 6) {
        editline(c.bibatchfn, 78, ALL, &i1, "");
        trimstr(c.bibatchfn);
      }
      break;

    }
    cp = GetNextSelectionPosition(0, 7, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);

  if (n >= 6) {
    externs[n - 6] = c;
  } else {
    if (c.receivefn[0] || c.sendfn[0] || (c.sendbatchfn[0] && (n == 4))) {
      c.othr |= othr_override_internal;
    } else {
      c.othr &= ~othr_override_internal;
    }
    over_intern[n - 2] = c;
  }
}

#define BASE_CHAR '!'
#define END_CHAR (BASE_CHAR+10)

void extrn_prots() {
  bool done = false;
  do {
    out->Cls();
    nlx();
    for (int i = 2; i < 6 + initinfo.numexterns; i++) {
      if (i == 5) {
        continue;
      }
      Printf("%c. %s\n", (i < 10) ? (i + '0') : (i - 10 + BASE_CHAR), prot_name(i));
    }
    int nMaxProtocolNumber = initinfo.numexterns + 6;
    nlx();
    textattr(COLOR_YELLOW);
    Puts("Externals: M:odify, D:elete, I:nsert, Q:uit : ");
    textattr(COLOR_CYAN);
    char ch = onek("Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M': {
      nlx();
      textattr(COLOR_YELLOW);
      Printf("Edit which (2-%d) ? ", nMaxProtocolNumber);
      textattr(COLOR_CYAN);
      int i = input_number(2);
      if ((i > -1) && (i < initinfo.numexterns + 6)) {
        edit_prot(i);
      }
    }
    break;
    case 'D':
      if (initinfo.numexterns) {
        nlx();
        textattr(COLOR_YELLOW);
        Printf("Delete which (6-%d) ? ", nMaxProtocolNumber);
        textattr(COLOR_CYAN);
        int i = input_number(2);
        if (i > 0) {
          i -= 6;
        }
        if ((i > -1) && (i < initinfo.numexterns)) {
          for (int i1 = i; i1 < initinfo.numexterns; i1++) {
            externs[i1] = externs[i1 + 1];
          }
          --initinfo.numexterns;
        }
      }
      break;
    case 'I':
      if (initinfo.numexterns >= 15) {
        textattr(COLOR_RED);
        Printf("Too many external protocols.\n");
        textattr(COLOR_CYAN);
        nlx();
        break;
      }
      nlx();
      textattr(COLOR_YELLOW);
      Printf("Insert before which (6-%d) ? ", nMaxProtocolNumber);
      textattr(COLOR_CYAN);
      int i = input_number(2);
      if ((i > -1) && (i <= initinfo.numexterns + 6)) {
        for (int i1 = initinfo.numexterns; i1 > i - 6; i1--) {
          externs[i1] = externs[i1 - 1];
        }
        ++initinfo.numexterns;
        memset(externs + i - 6, 0, sizeof(newexternalrec));
        edit_prot(i);
      } else {
        Printf("Invalid entry: %d", i);
        out->GetChar();
      }
      break;
    }
  } while (!done);
  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%snextern.dat", syscfg.datadir);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, (void *)externs, initinfo.numexterns * sizeof(newexternalrec));
  close(hFile);

  sprintf(szFileName, "%snintern.dat", syscfg.datadir);
  if ((over_intern[0].othr | over_intern[1].othr | over_intern[2].othr)&othr_override_internal) {
    hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    if (hFile > 0) {
      write(hFile, over_intern, 3 * sizeof(newexternalrec));
      close(hFile);
    }
  } else {
    unlink(szFileName);
  }
}
