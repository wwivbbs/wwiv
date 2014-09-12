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
#include <cstdlib>
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
#include "core/strings.h"
#include "core/wwivport.h"
#include "utility.h"
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

static void edit_prot(CursesWindow* window, int n) {
  char s[81], s1[81];
  newexternalrec c;

  out->Cls();
  if (n >= 6) {
    c = externs[n - 6];
  } else {
    if (n == 5) {
      // This is the "Batch" protocol which has no override.
      return;
    }
    c = over_intern[n - 2];
    strcpy(c.description, prot_name(n));
    strcpy(c.receivebatchfn, "-- N/A --");
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
  window->Printf("Description          : %s\n", c.description);
  window->Printf("Xfer OK code         : %s\n", s);
  window->Printf("Require MNP/LAPM     : %s\n", s1);
  window->Printf("Receive command line:\n%s\n", c.receivefn);
  window->Printf("Send command line:\n%s\n", c.sendfn);
  window->Printf("Receive batch command line:\n%s\n", c.receivebatchfn);
  window->Printf("Send batch command line:\n%s\n", c.sendbatchfn);
  window->SetColor(SchemeId::PROMPT);
  window->Puts("\n<ESC> when done.\n\n");
  window->SetColor(SchemeId::NORMAL);
  window->Puts("%1 = com port baud rate\n");
  window->Puts("%2 = port number\n");
  window->Puts("%3 = filename to send/receive, filename list to send for batch\n");
  window->Puts("%4 = modem speed\n");
  window->Puts("%5 = filename list to receive for batch UL and bi-directional batch\n");
  nlx();
  window->SetColor(SchemeId::WARNING);
  window->Printf("NOTE: Batch protocols >MUST< correctly support DSZLOG.\n");
  window->SetColor(SchemeId::NORMAL);

  do {
    if (cp < 3) {
      window->GotoXY(23, cp);
    } else {
      window->GotoXY(0, cp * 2 - 2);
    }
    switch (cp) {
    case 0:
      if (n >= 6) {
        editline(window, c.description, 50, ALL, &i1, "");
        StringTrimEnd(c.description);
      }
      break;
    case 1:
      editline(window, s, 3, NUM_ONLY, &i1, "");
      StringTrimEnd(s);
      c.ok1 = atoi(s);
      sprintf(s, "%u", c.ok1);
      window->Puts(s);
      break;
    case 2:
      if (n >= 6) {
        editline(window, s1, 1, UPPER_ONLY, &i1, "");
        if (s1[0] != 'Y') {
          s1[0] = 'N';
        }
        s1[1] = 0;
        if (s1[0] == 'Y') {
          c.othr |= othr_error_correct;
        } else {
          c.othr &= (~othr_error_correct);
        }
        window->Puts(s1);
      }
      break;
    case 3:
      editline(window, c.receivefn, 78, ALL, &i1, "");
      StringTrimEnd(c.receivefn);
      if (c.sendfn[0] == 0) {
        strcpy(c.sendfn, c.receivefn);
      }
      if (c.sendbatchfn[0] == 0) {
        strcpy(c.sendbatchfn, c.sendfn);
      }
      if (c.receivebatchfn[0] == 0) {
        strcpy(c.receivebatchfn, c.receivefn);
      }
      break;
    case 4:
      editline(window, c.sendfn, 78, ALL, &i1, "");
      StringTrimEnd(c.sendfn);
      break;
    case 5:
      if (n >= 6) {
        editline(window, c.receivebatchfn, 78, ALL, &i1, "");
        StringTrimEnd(c.receivebatchfn);
      }
      break;
    case 6:
      if (n >= 4) {
        editline(window, c.sendbatchfn, 78, ALL, &i1, "");
        StringTrimEnd(c.sendbatchfn);
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
  CursesWindow* window(out->window());
  do {
    out->Cls();
    nlx();
    for (int i = 2; i < 6 + initinfo.numexterns; i++) {
      if (i == 5) {
        continue;
      }
      window->Printf("%c. %s\n", (i < 10) ? (i + '0') : (i - 10 + BASE_CHAR), prot_name(i));
    }
    int nMaxProtocolNumber = initinfo.numexterns + 6;
    nlx();
    window->SetColor(SchemeId::PROMPT);
    window->Puts("Externals: M:odify, D:elete, I:nsert, Q:uit : ");
    window->SetColor(SchemeId::NORMAL);
    char ch = onek(window, "Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M': {
      nlx();
      window->SetColor(SchemeId::PROMPT);
      window->Printf("Edit which (2-%d) ? ", nMaxProtocolNumber);
      window->SetColor(SchemeId::NORMAL);
      int i = input_number(window, 2);
      if ((i > -1) && (i < initinfo.numexterns + 6)) {
        edit_prot(window, i);
      }
    }
    break;
    case 'D':
      if (initinfo.numexterns) {
        nlx();
        window->SetColor(SchemeId::PROMPT);
        window->Printf("Delete which (6-%d) ? ", nMaxProtocolNumber);
        window->SetColor(SchemeId::NORMAL);
        int i = input_number(window, 2);
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
        window->SetColor(SchemeId::ERROR_TEXT);
        window->Printf("Too many external protocols.\n");
        window->SetColor(SchemeId::NORMAL);
        nlx();
        break;
      }
      nlx();
      window->SetColor(SchemeId::PROMPT);
      window->Printf("Insert before which (6-%d) ? ", nMaxProtocolNumber);
      window->SetColor(SchemeId::NORMAL);
      int i = input_number(window, 2);
      if ((i > -1) && (i <= initinfo.numexterns + 6)) {
        for (int i1 = initinfo.numexterns; i1 > i - 6; i1--) {
          externs[i1] = externs[i1 - 1];
        }
        ++initinfo.numexterns;
        memset(externs + i - 6, 0, sizeof(newexternalrec));
        edit_prot(window, i);
      } else {
        window->Printf("Invalid entry: %d", i);
        window->GetChar();
      }
      break;
    }
  } while (!done);
  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%snextern.dat", syscfg.datadir);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, externs, initinfo.numexterns * sizeof(newexternalrec));
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
