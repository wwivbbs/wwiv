/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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

#include <conio.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filenames.h"
#include "core/wwivport.h"
#include "core/wfile.h"
#include "vardec.h"
#include "version.cpp"

#define VER 0x0101

char datadir[81];
static char *Strs;
static char **StrIdx;
int iNumStrs;

void print(int color, char *fmt, ...) {
  va_list ap;
  char s[512];

  va_start(ap, fmt);
  vsprintf(s, fmt, ap);
  va_end(ap);

  fputs(" þ ", stdout);
  puts(s);
  color = color;
}

void stripnl(char *instr) {
  unsigned int i;

  if (strlen(instr) == 0) {
    return;
  }

  i = 0;
  do {
    if ((instr[i] == '\n') || (instr[i] == '\r')) {
      instr[i] = '\0';
    }
    i++;
  } while (i < strlen(instr));
}

void ReadChainText() {
  FILE *fp;
  char szTemp[MAX_PATH];
  int iX;

  sprintf(szTemp, "CHAIN.TXT");
  fp = fopen(szTemp, "rt");
  if (!fp) {
    print(LIGHTRED, "");
    print(LIGHTRED, "ERR: Unable to open chain.txt");
    exit(1);
  }
  for (iX = 0; iX <= 17; iX++) {
    fgets(datadir, 80, fp);
  }

  stripnl(datadir);
}

void ReadMenuSetup() {

  if (Strs == NULL) {
    char szTemp[MAX_PATH];
    FILE *fp;
    int iAmt, *index, iLen, iX;

    sprintf(szTemp, "%s%s", datadir, MENUCMDS_DAT);
    fp = fopen(szTemp, "rb");
    if (!fp) {
      print(LIGHTRED, "");
      print(LIGHTRED, "ERR: Unable to open menucmds.dat");
      exit(1);
    }
    fseek(fp, 0, SEEK_END);
    iLen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    fread(&iAmt, 2, 1, fp);
    if (iAmt == 0) {
      print(LIGHTRED, "");
      print(LIGHTRED, "ERR: No menu strings found in menucmds.dat");
      exit(1);
    }
    iNumStrs = iAmt;

    index = (int *)bbsmalloc(sizeof(int) * iAmt);
    fread(index, 2, iAmt, fp);
    iLen -= ftell(fp);
    Strs = (char *)bbsmalloc(iLen);
    StrIdx = (char **)bbsmalloc(sizeof(char **) * iAmt);
    fread(Strs, iLen, 1, fp);
    fclose(fp);

    for (iX = 0; iX < iAmt; iX++) {
      StrIdx[iX] = Strs + index[iX];
    }

    free(index);
  }
}

void Usage() {
  /* Long line split to avoid line wrap */
  print(LIGHTGREEN, "");
  print(LIGHTGREEN, "Purpose:");
  print(LIGHTGREEN, "");
  print(LIGHTCYAN, "   Program to give you the string number of a"
        " menu command in MENUCMDS.DAT");
  print(LIGHTCYAN, "Makes modding the Asylum Menus a lot easier."
        "  If you've ever added a new");
  print(LIGHTCYAN, "command to MENUCMDS.DAT (and MENU.C) you'll probably"
        " find this useful.");
  print(LIGHTGREEN, "");
  print(LIGHTGREEN, "Usage:");
  print(LIGHTGREEN, "");
  print(LIGHTCYAN, "   MenuTell.exe <MenuStringVar>");
  exit(1);
}

int main(int argc, char *argv[]) {
  int retval = 0;
  int strnum = -1;
  int iX;

  print(LIGHTBLUE, "MenuTell v%d.%02d for %s", VER / 256, VER % 256,
        wwiv_version);
  ReadChainText();
  ReadMenuSetup();
  if (argc < 2) {
    Usage();
  }
  for (iX = 0; iX <= iNumStrs; iX++) {
    if (!WWIV_STRICMP(StrIdx[iX], argv[1])) {
      strnum = iX;
    }
  }

  if (strnum < 0) {
    print(LIGHTRED, "");
    print(LIGHTRED, "Unable to find string %s in MENUCMDS.DAT", argv[1]);
    retval = 1;
  } else {
    print(LIGHTCYAN, "");
    print(LIGHTCYAN, "%s = StrIdx[%d]", StrIdx[strnum], strnum);
  }
  return (retval);
}
