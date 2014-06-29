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
#include <cstdlib>
#include <curses.h>
#include <fcntl.h>
#include <string>
#ifdef _WIN32
#include <io.h>
#endif
#include <sys/stat.h>

#include "wwivinit.h"
#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "subacc.h"
#include "platform/incl1.h"
#include "platform/wfile.h"

extern net_networks_rec *net_networks;

void trimstr(char *s) {
  int i = strlen(s);
  while ((i >= 0) && (s[i - 1] == 32)) {
    --i;
  }
  s[i] = '\0';
}

void trimstrpath(char *s) {
  trimstr(s);

  int i = strlen(s);
  if (i && (s[i - 1] != WFile::pathSeparatorChar)) {
    // We don't have pathSeparatorString.
    s[i] = WFile::pathSeparatorChar;
    s[i + 1] = 0;
  }
}
#define MAX_SUBS_DIRS 4096

static void convert_to(int num_subs, int num_dirs) {
  int oqf, nqf, nu, i;
  char oqfn[81], nqfn[81];
  unsigned long *nqsc, *nqsc_n, *nqsc_q, *nqsc_p;
  unsigned long *oqsc, *oqsc_n, *oqsc_q, *oqsc_p;
  int l1, l2, l3, nqscn_len;

  if (num_subs % 32) {
    num_subs = (num_subs / 32 + 1) * 32;
  }
  if (num_dirs % 32) {
    num_dirs = (num_dirs / 32 + 1) * 32;
  }

  if (num_subs < 32) {
    num_subs = 32;
  }
  if (num_dirs < 32) {
    num_dirs = 32;
  }

  if (num_subs > MAX_SUBS_DIRS) {
    num_subs = MAX_SUBS_DIRS;
  }
  if (num_dirs > MAX_SUBS_DIRS) {
    num_dirs = MAX_SUBS_DIRS;
  }

  nqscn_len = 4 * (1 + num_subs + ((num_subs + 31) / 32) + ((num_dirs + 31) / 32));

  nqsc = (unsigned long *)malloc(nqscn_len);
  if (!nqsc) {
    Printf("Could not allocate %d bytes for new quickscan rec\n", nqscn_len);
    return;
  }
  memset(nqsc, 0, nqscn_len);

  nqsc_n = nqsc + 1;
  nqsc_q = nqsc_n + ((num_dirs + 31) / 32);
  nqsc_p = nqsc_q + ((num_subs + 31) / 32);

  memset(nqsc_n, 0xff, ((num_dirs + 31) / 32) * 4);
  memset(nqsc_q, 0xff, ((num_subs + 31) / 32) * 4);

  oqsc = (unsigned long *)malloc(syscfg.qscn_len);
  if (!oqsc) {
    free(nqsc);
    Printf("Could not allocate %d bytes for old quickscan rec\n", syscfg.qscn_len);
    return;
  }
  memset(oqsc, 0, syscfg.qscn_len);

  oqsc_n = oqsc + 1;
  oqsc_q = oqsc_n + ((syscfg.max_dirs + 31) / 32);
  oqsc_p = oqsc_q + ((syscfg.max_subs + 31) / 32);

  if (num_dirs < syscfg.max_dirs) {
    l1 = ((num_dirs + 31) / 32) * 4;
  } else {
    l1 = ((syscfg.max_dirs + 31) / 32) * 4;
  }

  if (num_subs < syscfg.max_subs) {
    l2 = ((num_subs + 31) / 32) * 4;
    l3 = num_subs * 4;
  } else {
    l2 = ((syscfg.max_subs + 31) / 32) * 4;
    l3 = syscfg.max_subs * 4;
  }


  sprintf(oqfn, "%suser.qsc", syscfg.datadir);
  sprintf(nqfn, "%suserqsc.new", syscfg.datadir);

  oqf = open(oqfn, O_RDWR | O_BINARY);
  if (oqf < 0) {
    free(nqsc);
    free(oqsc);
    Printf("Could not open user.qsc\n");
    return;
  }
  nqf = open(nqfn, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  if (nqf < 0) {
    free(nqsc);
    free(oqsc);
    close(oqf);
    Printf("Could not open userqsc.new\n");
    return;
  }

  nu = filelength(oqf) / syscfg.qscn_len;
  for (i = 0; i < nu; i++) {
    if (i % 10 == 0) {
      Printf("%u/%u\r", i, nu);
    }
    read(oqf, oqsc, syscfg.qscn_len);

    *nqsc = *oqsc;
    memcpy(nqsc_n, oqsc_n, l1);
    memcpy(nqsc_q, oqsc_q, l2);
    memcpy(nqsc_p, oqsc_p, l3);

    write(nqf, nqsc, nqscn_len);
  }


  close(oqf);
  close(nqf);
  unlink(oqfn);
  rename(nqfn, oqfn);

  syscfg.max_subs = num_subs;
  syscfg.max_dirs = num_dirs;
  syscfg.qscn_len = nqscn_len;
  save_config();

  free(nqsc);
  free(oqsc);
  Printf("\nDone\n");
}


void up_subs_dirs() {
  int num_subs, num_dirs;

  out->Cls();

  out->SetColor(Scheme::NORMAL);
  Printf("Current max # subs: %d\n", syscfg.max_subs);
  Printf("Current max # dirs: %d\n", syscfg.max_dirs);
  nlx(2);

  if (dialog_yn("Change # subs or # dirs")) {
    nlx();
    out->SetColor(Scheme::INFO);
    Printf("Enter the new max subs/dirs you wish.  Just hit <enter> to leave that\n");
    Printf("value unchanged.  All values will be rounded up to the next 32.\n");
    Printf("Values can range from 32-1024\n\n");

    out->SetColor(Scheme::PROMPT);
    Puts("New max subs: ");
    num_subs = input_number(4);
    if (!num_subs) {
      num_subs = syscfg.max_subs;
    }
    nlx(2);
    out->SetColor(Scheme::PROMPT);
    Puts("New max dirs: ");
    num_dirs = input_number(4);
    if (!num_dirs) {
      num_dirs = syscfg.max_dirs;
    }

    if (num_subs % 32) {
      num_subs = (num_subs / 32 + 1) * 32;
    }
    if (num_dirs % 32) {
      num_dirs = (num_dirs / 32 + 1) * 32;
    }

    if (num_subs < 32) {
      num_subs = 32;
    }
    if (num_dirs < 32) {
      num_dirs = 32;
    }

    if (num_subs > MAX_SUBS_DIRS) {
      num_subs = MAX_SUBS_DIRS;
    }
    if (num_dirs > MAX_SUBS_DIRS) {
      num_dirs = MAX_SUBS_DIRS;
    }

    if ((num_subs != syscfg.max_subs) || (num_dirs != syscfg.max_dirs)) {
      nlx();
      out->SetColor(Scheme::PROMPT);
      char text[81];
      sprintf(text, "Change to %d subs and %d dirs? ", num_subs, num_dirs);

      if (dialog_yn(text)) {
        nlx();
        out->SetColor(Scheme::INFO);
        Printf("Please wait...\n");
        convert_to(num_subs, num_dirs);
      }
    }
  }
}
