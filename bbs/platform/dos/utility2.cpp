/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"


void show_files(char *fn, char *dir)
/* Displays list of files matching filespec fn in directory dir. */
{
    int i;
    char s[120], s1[120], c;
    char drive[MAX_DRIVE], direc[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];
    
    c = (okansi()) ? '\xCD' : '=';
    nl();
    
    _splitpath(dir, drive, direc, file, ext);
    sprintf(s, "%s%s", "|09[|B1|15 FileSpec: ", strupr(stripfn(fn)));
    strcat(s, "    Dir: ");
    strcat(s, drive);
    strcat(s, direc);
    strcat(s, " |B0|09]");
    i = (GetSession()->thisuser.screenchars - 1) / 2 - strlen(stripcolors(s)) / 2;
    npr("|09%s", charstr(i, c));
    npr(s);
    i = GetSession()->thisuser.screenchars - 1 - i - strlen(stripcolors(s));
    npr("|09%s", charstr(i, c));
    
    sprintf(s1, "%s%s", dir, strupr(stripfn(fn)));
	WFindFile fnd;
	bool bFound = fnd.open(s1, 0);
    while (bFound) 
    {
        strcpy(s, fnd.GetFileName());
        align(s);
        sprintf(s1, "|09[|14%s|09]|11 ", s);
        if (GetApplication()->GetLocalIO()->WhereX() > (GetSession()->thisuser.screenchars - 15))
        {
            nl();
        }
        npr(s1);
        bFound = fnd.next();
    }
    
    nl();
    ansic(7);
    pl(charstr(GetSession()->thisuser.screenchars - 1, c));
    nl();
}



// Used only by WWIV_make_abs_cmd

char *exts[] = {"", ".COM", ".EXE", ".BAT", ".BTM", ".CMD", 0};



char *WWIV_make_abs_cmd(char *out)
{
	// %%TODO: Make this platform specific
  char s[161], s1[161], s2[161], *ss, *ss1;
  char szTempBuf[MAX_PATH];
  int i;

  char *pszHome = GetApplication()->GetHomeDir();

  strcpy(s1, out);

  if (s1[1] == ':') {
    if (s1[2] != '\\') {
	  _getdcwd(upcase(s1[0]) - 'A' + 1, s, 161);
      if (s[0])
        sprintf(s1, "%c:\\%s\\%s", s1[0], s, out + 2);
      else
        sprintf(s1, "%c:\\%s", s1[0], out + 2);
    }
  } else
    if (s1[0] == '\\') {
    sprintf(s1, "%c:%s", *pszHome, out);
  } else {
    strcpy(s2, s1);
    strtok(s2, " \t");
    if (strchr(s2, '\\')) {
      sprintf(s1, "%s%s", GetApplication()->GetHomeDir(), out);
    }
  }

  ss = strchr(s1, ' ');
  if (ss) {
    *ss = 0;
    sprintf(s2, " %s", ss + 1);
  } else {
    s2[0] = 0;
  }
  for (i = 0; exts[i]; i++) {
    if (i == 0) {
      ss1 = strrchr(s1, '\\');
      if (!ss1)
        ss1 = s1;
      if (strchr(ss1, '.') == 0)
        continue;
    }
    sprintf(s, "%s%s", s1, exts[i]);
    if (s1[1] == ':') {
      if (exist(s)) {
        sprintf(out, "%s%s", s, s2);
        goto got_cmd;
      }
    } else {
      if (exist(s)) {
        sprintf(out, "%s%s%s", GetApplication()->GetHomeDir(), s, s2);
        goto got_cmd;
      } else {
		_searchenv(s, "PATH", szTempBuf);
        ss1 = szTempBuf;
        if ((ss1) && (strlen(ss1)>0)) {
          sprintf(out, "%s%s", ss1, s2);
          goto got_cmd;
        }
      }
    }
  }

  sprintf(out, "%s%s%s", GetApplication()->GetHomeDir(), s1, s2);

got_cmd:
  return (out);
}

#define LAST(s)	s[strlen(s)-1]

int WWIV_make_path(char *s)
{
    char drive, current_path[MAX_PATH], current_drive, *p, *flp;

    p = flp = strdup(s);
    _getdcwd(0, current_path, MAX_PATH);
    current_drive = *current_path - '@';
    if (LAST(p) == WWIV_FILE_SEPERATOR_CHAR)
        LAST(p) = 0;
    if (p[1] == ':') {
        drive = toupper(p[0]) - 'A' + 1;
        if (_chdrive(drive)) {
            chdir(current_path);
            _chdrive(current_drive);
            return -2;  
        }
        p += 2;
    }
    if (*p == WWIV_FILE_SEPERATOR_CHAR) {
        chdir(WWIV_FILE_SEPERATOR_STRING);
        p++;
    }
    for (; (p = strtok(p, WWIV_FILE_SEPERATOR_STRING)) != 0; p = 0) {
        if (chdir(p)) {
            if (mkdir(p)) {
                chdir(current_path);
                _chdrive(current_drive);
                return -1;
            }
            chdir(p);
        }   
    }
    chdir(current_path);
    if (flp)
        free(flp);
    return 0;
}

#if defined (LAST)
#undef LAST
#endif

void WWIV_Delay(unsigned long usec)
{
  delay(usec*1000);
}
