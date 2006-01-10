/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

/*
 * Possible future enhancements:
 *
 * duplicate users?
 * modem stuff ok
 * decrement mail waiting
 */

#pragma warn -par
void sysoplog(char *s)
{
}
void giveup_timeslice(void)
{
}
void read_status(void)
{
}
#pragma warn +par

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys\stat.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <alloc.h>
#include <time.h>
#include <stdarg.h>

#ifdef __MSDOS__
#include "conio.h"
#define TRUE  1
#define FALSE 0
typedef short int BOOL;
#endif

#ifdef __OS2__
#define huge
#endif          /* __OS2__ */

#define EXTENDED
#include "vardec.h"
#include "share.h"
#include "share.c"

#define OK  0       // "+ "
#define NOK 1       // "! "
#define QOK 2       // "? "

char* szLogTypeArray[] =
{
   "+ ",
   "! ",
   "? ",
   0
};

/************************** Other externals ****************************/
extern unsigned int     wwiv_num_version;
extern char             *wwiv_version;
extern char             *beta_version;
extern char             *wwiv_date;
extern unsigned int     bc_version;

/******************** Local Function Declarations **********************/

void savefile(char *b, long l1, messagerec * m1, char *aux);
char *readfile(messagerec * m1, char *aux, long *l);
void read_user(unsigned int un);
void write_user(unsigned int un);
void close_user(void);
void save_status(void);
int exist(char *s);
void give_up(void);
void oom(void *x, char *where, long l);
int yn(void);
void maybe_give_up(void);
int mdto(char *s);
int cdto(char *s);
void get_dir(char *s, int be);
int check_dir(char *dir, char *descr);
void check_all_dirs(void);
int CheckFile(char *fn);
void check_crit_files(void);
int trashed_str(char *s, int ml, int lower);
int trashed_user();
void fix_user();
void check_userlist(void);
void isr1(int un, char *name);
void check_nameslist(void);
void set_gat_section(int f, int section);
void save_gat(int f);
void remove_link(messagerec * m1, char *aux);
void delmail(int f, int loc);
void find_max_qscan(void);
char *describem(char *s, int subnum, int msgnum);
void check_type_1(int num, messagerec * list, char *extra, int todesc);
void check_type_2(int num, messagerec * list, char *extra, int todesc);
int open_file(char *fn);
void check_msg_consistency(void);
void ck_size(char *fn, int f, long rl);
void init(void);
void ShowBanner(void);
void ShowHelp(void);
void Print(int nType, BOOL bLogIt, char* szText, ...);
BOOL OpenLogFile(char* szFileName);
BOOL CloseLogFile();

/******************** Global Variable  Declarations **********************/

int userfile        = -1,
    configfile      = -1,
    statusfile      = -1,
    force_yes       = 0,
    usercheck       = 1,
    allow_changes   = 1,
    force_changes   = 0,
    num_dirs        = 0,
    num_subs        = 0,
    curlsub         = -1,
    debuglevel      = 0,
    incom           = 0,
    subchg          = 0,
    nummsgs,
    gat_section,
    url,
    numemailm,
    num_type_0,
    cur_users;

statusrec       status;
configrec       syscfg;
directoryrec    *directories;
subboardrec     *subboards;
messagerec      *emailm;
smalrec         *smallist,
                *new_smallist;

char *thisuser,
     *thisuser_inact,
     *emailmmm,
     cdir[81];

unsigned long max_qscan;

short gat[2048];

FILE *hLogFile;

#define GATSECLEN (4096L+2048L*512L)
#define MSG_STARTING (((long)gat_section)*GATSECLEN + 4096L)
#define NOT_BBS 0
#define malloca(x) bbsmalloc(x)
#define MAX_TO_CACHE 163
#include "subacc.c"

/****************************************************************************/
void savefile(char *b, long l1, messagerec * m1, char *aux)
{
  int f, gatp, i5, i4, gati[128], section;
  messagerec m;
  char s[81], s1[81];
  long l2;

  m = *m1;
  switch (m.storage_type) {
    case 0:
    case 1:
      m.stored_as = status.qscanptr++;
      save_status();
      ltoa(m.stored_as, s1, 16);
      strcpy(s, syscfg.msgsdir);
      if (m.storage_type == 1) {
        strcat(s, aux);
        strcat(s, "\\");
      }
      strcat(s, s1);
      f = open(s, O_RDWR | O_CREAT | O_BINARY, S_IREAD | S_IWRITE);
      write(f, (void *) b, l1);
      close(f);
      break;
    case 2:
      f = open_file(aux);
      if (f > 0) {
        for (section = 0; section < 1024; section++) {
          set_gat_section(f, section);
          gatp = 0;
          i5 = (int) ((l1 + 511L) / 512L);
          i4 = 1;
          while ((gatp < i5) && (i4 < 2048)) {
            if (gat[i4] == 0)
              gati[gatp++] = i4;
            ++i4;
          }
          if (gatp >= i5) {
            l2 = MSG_STARTING;
            gati[gatp] = -1;
            for (i4 = 0; i4 < i5; i4++) {
              lseek(f, l2 + 512L * (long) (gati[i4]), SEEK_SET);
              write(f, (void *) (&b[i4 * 512]), 512);
              gat[gati[i4]] = gati[i4 + 1];
            }
            save_gat(f);
            break;
          }
        }
        close(f);
      }
      m.stored_as = ((long) gati[0]) + ((long) gat_section) * 2048L;
      break;
    case 255:
      f = sh_open(aux, O_RDWR | O_CREAT | O_BINARY, S_IREAD | S_IWRITE);
      sh_write(f, (void *) b, l1);
      sh_close(f);
      break;
    default:
      break;
  }
  bbsfree((void *) b);
  *m1 = m;
}


char *readfile(messagerec * m1, char *aux, long *l)
{
  int f, csec;
  long l1, l2;
  char *b, s[81], s1[81];
  messagerec m;

  *l = 0L;
  m = *m1;
  switch (m.storage_type) {
    case 0:
    case 1:
      strcpy(s, syscfg.msgsdir);
      ltoa(m.stored_as, s1, 16);
      if (m.storage_type == 1) {
        strcat(s, aux);
        strcat(s, "\\");
      }
      strcat(s, s1);
      f = sh_open1(s, O_RDONLY | O_BINARY);
      if (f == -1) {
        *l = 0L;
        return (NULL);
      }
      l1 = filelength(f);
      if ((b = malloca(l1 + 2L)) == NULL) {
        sh_close(f);
        return (NULL);
      }
      sh_read(f, (void *) b, l1);
      sh_close(f);
      *l = l1;
      break;
    case 2:
      f = open_file(aux);
      set_gat_section(f, (int) (m.stored_as / 2048L));
      csec = m.stored_as % 2048;
      l1 = 0;
      while ((csec > 0) && (csec < 2048)) {
        l1 += 512L;
        csec = gat[csec];
      }
      if (!l1) {
        sh_close(f);
        return (NULL);
      }
      if ((b = malloca(l1 + 3)) == NULL) {
        sh_close(f);
        return (NULL);
      }
      csec = m.stored_as % 2048;
      l1 = 0;
      l2 = MSG_STARTING;
      while ((csec > 0) && (csec < 2048)) {
        sh_lseek(f, l2 + 512L * ((long) csec), SEEK_SET);
        l1 += (long) sh_read(f, (void *) (&(b[l1])), 512);
        csec = gat[csec];
      }
      sh_close(f);
      l2 = l1 - 512;
      while ((l2 < l1) && (b[l2] != 26))
        ++l2;
      *l = l2;
      b[l2 + 1] = 0;
      break;
    case 255:
      f = sh_open1(aux, O_RDONLY | O_BINARY);
      if (f == -1) {
        *l = 0L;
        return (NULL);
      }
      l1 = filelength(f);
      if ((b = malloca(l1 + 258L)) == NULL) {
        sh_close(f);
        return (NULL);
      }
      sh_read(f, (void *) b, l1);
      sh_close(f);
      *l = l1;
      break;
    default:
      /* illegal storage type */
      *l = 0L;
      b = NULL;
      break;
  }
  return (b);
}

void read_user(unsigned int un)
{
  long pos;
  char s[80];

  if (userfile == -1) {
    sprintf(s, "%sUSER.LST", syscfg.datadir);
    userfile = sh_open1(s, O_RDWR | O_BINARY);
    if (userfile < 0) {
      *thisuser_inact = inact_deleted;
      return;
    }
  }
  pos = ((long) url) * ((long) un);
  if (filelength(userfile) < (pos + syscfg.userreclen)) {
    *thisuser_inact = inact_deleted;
    return;
  }
  lseek(userfile, pos, SEEK_SET);
  read(userfile, thisuser, syscfg.userreclen);
}

/****************************************************************************/

void write_user(unsigned int un)
{
  long pos;
  char s[80];

  if (userfile == -1) {
    sprintf(s, "%sUSER.LST", syscfg.datadir);
    userfile = sh_open1(s, O_RDWR | O_BINARY);
    if (userfile < 0) {
      return;
    }
  }
  pos = ((long) url) * ((long) un);
  if (filelength(userfile) < (pos + syscfg.userreclen)) {
    return;
  }
  lseek(userfile, pos, SEEK_SET);
  write(userfile, thisuser, syscfg.userreclen);
}

/****************************************************************************/


void close_user(void)
{
  if (userfile != -1) {
    sh_close(userfile);
    userfile = -1;
  }
}

/****************************************************************************/

void save_status(void)
{
  char s[80];

  sprintf(s, "%sSTATUS.DAT", syscfg.datadir);
  statusfile = sh_open(s, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(statusfile, (void *) (&status), sizeof(statusrec));
  sh_close(statusfile);
  statusfile = -1;
}

/****************************************************************************/

int exist(char *s)
{
  if (!s)
  {
        // Return a False if the string passed is NULL.
        return 0;
  }
#ifdef _WIN32
  return (_access(s, 0) == 0) ? 1 : 0;
#else // if __OS2__ or DOS
  return (access(s, 0) == 0) ? 1 : 0;
#endif // _WIN32

}

/****************************************************************************/

void give_up(void)
{
  Print(NOK, TRUE, "Giving up.");
  exit(-1);
}


/****************************************************************************/

void oom(void *x, char *where, long l)
{
  if (!x) {
    Print(NOK, TRUE, "Out of memory for %s (%ld bytes).", where, l);
    give_up();
  }
}

/****************************************************************************/

int yn(void)
{
  char ch;

  if (force_yes) {
    printf("Yes\n");
    return (TRUE);
  }
  while (TRUE) {
    ch = toupper(getch());
    if (ch == 'Y') {
      printf("Yes\n");
      return (TRUE);
    }
    if ((ch == 'N') || (ch == 13)) {
      printf("No\n");
      return (FALSE);
    }
  }
}


/****************************************************************************/

void maybe_give_up(void)
{
  Print(OK, TRUE, "Future expansion might try to fix this problem.");
  give_up();
}

/****************************************************************************/

int mdto(char *s)
{
  char s1[81];
  int i, db, st;

  strcpy(s1, s);
  i = strlen(s1) - 1;
  db = (s1[i] == '\\');
  if (i == 0)
    db = 0;
  if ((i == 2) && (s1[1] == ':'))
    db = 0;
  if (db)
    s1[i] = 0;
  st = mkdir(s1);
  if (s[1] == ':') {
    i = setdisk(s[0] - 'A');
    if (i <= (s[0] - 'A'))
      st = 1;
  }
  return (st);
}


int cdto(char *s)
{
  char s1[81];
  int i, db, st;

  strcpy(s1, s);
  i = strlen(s1) - 1;
  db = (s1[i] == '\\');
  if (i == 0)
    db = 0;
  if ((i == 2) && (s1[1] == ':'))
    db = 0;
  if (db)
    s1[i] = 0;
  st = chdir(s1);
  if (s[1] == ':') {
    i = setdisk(s[0] - 'A');
    if (i <= (s[0] - 'A'))
      st = 1;
  }
  return (st);
}

/****************************************************************************/


void get_dir(char *s, int be)
{
  strcpy(s, "X:\\");
  s[0] = 'A' + getdisk();
  getcurdir(0, &(s[3]));
  if (be) {
    if (s[strlen(s) - 1] != '\\')
      strcat(s, "\\");
  }
}


/****************************************************************************/

int check_dir(char *dir, char *descr)
{
  int st;

  st = cdto(dir);
  if (st) {
    Print(NOK, FALSE, "Unable to find dir '%s'    ",dir);
    Print(NOK, FALSE, "for '%s' dir, ",descr);
    printf("   Do You wish to CREATE it (y/N)? ");
    if (yn())
    {
      st = mdto(dir);
      if (st)
        Print(NOK, TRUE, "Unable to create dir '%s' for %s dir.", dir, descr);
    } else {
      // Do Nothing.
    }
  }
  cdto(cdir);
  return (st);
}

/****************************************************************************/

// add following macro and next four functions - Build3

/* SETREC macro to set directory record */
#define SETREC(f,i)  sh_lseek(f,((long) (i))*((long)sizeof(uploadsrec)),SEEK_SET);

int numf, ed_num, ed_got, fd;
char dlfn[81], edlfn[81];
ext_desc_rec *ed_info;

void zap_ed_info(void)
{
  if (ed_info) {
    bbsfree(ed_info);
    ed_info = NULL;
  }
  ed_num = 0;
  ed_got = 0;
}

void get_ed_info(void)
{
  int f;
  long l, l1;
  ext_desc_type ed;

  if (ed_got)
    return;

  zap_ed_info();
  ed_got = 1;

  if (!numf)
    return;

  l = 0;
  f = sh_open1(edlfn, O_RDONLY | O_BINARY);
  if (f > 0) {
    l1 = filelength(f);
    if (l1 > 0) {
      ed_info = malloca((long) numf * sizeof(ext_desc_rec));
      if (ed_info == NULL) {
        sh_close(f);
        return;
      }
      ed_num = 0;
      while ((l < l1) && (ed_num < numf)) {
        sh_lseek(f, l, SEEK_SET);
        if (sh_read(f, &ed, sizeof(ext_desc_type)) == sizeof(ext_desc_type)) {
          strcpy(ed_info[ed_num].name, ed.name);
          ed_info[ed_num].offset = l;
          l += (long) ed.len + sizeof(ext_desc_type);
          ed_num++;
        }
      }
      if (l < l1)
        ed_got = 2;
    }
    f = sh_close(f);
  }
}

int check_for_extended_description(char *fn)
{
  int i;

  get_ed_info();

  if (ed_got && ed_info) {
    for (i = 0; i < ed_num; i++) {
      if (strcmp(fn, ed_info[i].name) == 0)
        return(1);
    }
  }
  return(0);
}

int dliscan1(int dn)
{
  uploadsrec u;
  int st = 0;

  sprintf(dlfn, "%s%s.DIR", syscfg.datadir, directories[dn].filename);
  fd = sh_open(dlfn, O_RDWR | O_BINARY, S_IREAD | S_IWRITE);
  numf = filelength(fd) / sizeof(uploadsrec);
  SETREC(fd, 0);
  sh_read(fd, (void *) &u, sizeof(uploadsrec));
  if (u.numbytes != numf) {
    st = 1;
    printf("\n");
    Print(NOK, TRUE, "Corrected # of files in %s.", directories[dn].name);
    u.numbytes = numf;
    SETREC(fd, 0);
    sh_write(fd, (void *) &u, sizeof(uploadsrec));
  }
  sprintf(edlfn, "%s%s.EXT", syscfg.datadir, directories[dn].filename);
  zap_ed_info();
  return st;
}

// replace following function Build3
void check_all_dirs(void)
{
  int st = 0, mrec;
  int f, i, j;
  long l;
  char ff[81];
  uploadsrec u;

  Print(OK, TRUE, "Checking Directories...");

  st |= check_dir(syscfg.msgsdir,   "Messages");
  st |= check_dir(syscfg.gfilesdir, "G-files");
  st |= check_dir(syscfg.menudir,   "Menus");
  st |= check_dir(syscfg.datadir,   "Data");
  st |= check_dir(syscfg.dloadsdir, "Default Dloads");
  st |= check_dir(syscfg.tempdir,   "Temp");

  if (st)
  {
    Print(NOK, TRUE, "One of the critical system directories is not present or not set correctly.");
    give_up();
  }
  for (i = 0; i < num_dirs; i++)
  {
    if (!(directories[i].mask & mask_cdrom))
    {
      st = check_dir(directories[i].path, directories[i].name);
      if (st)
      {
        Print(NOK, TRUE, "===================================================================");
        Print(NOK, TRUE, "The Directory for File Area '%s' is Missing", directories[i].name);
        Print(NOK, TRUE, "The Directory listed is: '%s'", directories[i].path);
        Print(NOK, TRUE, "Either create the dir, or use //DIREDIT to set it correctly.");
        Print(NOK, TRUE, "This program cannot fix this problem.");
        Print(NOK, TRUE, "===================================================================");
      } else {
#ifdef __MSDOS__
        textattr(0x03);
        cprintf("\r- Checking directory '%s'", directories[i].name);
        clreol();
#else
        printf("- Checking directory '%s'\n", directories[i].name);
#endif
        st = dliscan1(i);
        for (j = 1; j <= numf; j++) {
          mrec = 0;
          SETREC(fd, j);
          sh_read(fd, (void *) &u, sizeof(uploadsrec));
          if (check_for_extended_description(u.filename)) {
            if ((u.mask & mask_extended) == 0) {
              u.mask |= mask_extended;
              mrec = 1;
            }
          } else {
            if (u.mask & mask_extended) {
              u.mask &= ~mask_extended;
              mrec = 1;
            }
          }
          if (mrec) {
            if (!st) {
              st = 1;
              printf("\n");
            }
            Print(NOK, TRUE, "Fixed extended description for '%s'.", u.filename);
          }
          sprintf(ff, "%s%s", directories[i].path, u.filename);
          f = sh_open1(ff, O_RDONLY | O_BINARY);
          if (f > 0) {
            l = filelength(f);
            if (l != u.numbytes) {
              u.numbytes = l;
              mrec = 1;
              if (!st) {
                st = 1;
                printf("\n");
              }
              Print(NOK, TRUE, "Fixed file size for '%s'.", u.filename);
            }
            sh_close(f);
          }
          if (mrec) {
            SETREC(fd, j);
            sh_write(fd, (void *) &u, sizeof(uploadsrec));
          }
        }
        sh_close(fd);
      }
    }
  }
#ifdef __MSDOS__
  if (!st) {
    printf("\r");
    clreol();
  }
#endif
}

int CheckFile(char *fn)
{
  int rc = 0;
  char str[81];

    if (exist(fn))
      rc = 1;
    else {
      sprintf(str, "Critical file: %s is missing and needs to be replaced.", fn);
      Print(NOK, TRUE, str);
    }

    return rc;
}



void check_crit_files(void)
{
  char str[81];

    Print(OK, TRUE, "Checking Critical Files");

    CheckFile("INIT.EXE");
    CheckFile("RETURN.EXE");
    CheckFile("WWIV.INI");
    CheckFile("CHAT.INI");
    sprintf(str, "%sARCHIVER.DAT", syscfg.datadir);
    CheckFile(str);
    sprintf(str, "%sWFC.DAT", syscfg.datadir);
    CheckFile(str);
    sprintf(str, "%sCOLOR.DAT", syscfg.datadir);
    CheckFile(str);
//    sprintf(str, "%sEVENTS.DAT", syscfg.datadir);
//    CheckFile(str);
    sprintf(str, "%sBBS.STR", syscfg.gfilesdir);
    CheckFile(str);
    sprintf(str, "%sINI.STR", syscfg.gfilesdir);
    CheckFile(str);
    sprintf(str, "%sCHAT.STR", syscfg.gfilesdir);
    CheckFile(str);
    sprintf(str, "%sSYSOPLOG.STR", syscfg.gfilesdir);
    CheckFile(str);
}

/****************************************************************************/

int trashed_str(char *s, int ml, int lower)
{
  int i, i1, st = 0;

  i1 = strlen(s);
  if (i1 > ml)
    st = 1;
  else
    for (i = 0; i < i1; i++) {
      if (s[i] < 32)
        st = 1;
      if ((!lower) && (islower(s[i])))
        st = 1;
    }
  return (st);
}

/****************************************************************************/


int trashed_user()
{
  if (trashed_str(thisuser, 30, 0))
    return (1);

  return (0);
}

void fix_user()
{
  thisuser[30] = 0;
}


/****************************************************************************/


void check_userlist(void)
{
  long l;
  int nu, check_trash = 0, trashed_userlist = 0, xxx;

  url = syscfg.userreclen;
  if (!url) {
    url = syscfg.userreclen = sizeof(userrec);
  }
  read_user(1);
  if (userfile < 0)
  {
    Print(NOK, TRUE, "No userlist present.");
    give_up();
  }
  l = filelength(userfile);
  nu = (int) (l / ((long) url)) - 1;

  Print(OK, TRUE, "Checking USER.LST... Found %d user records... ", nu);

  if (nu > syscfg.maxusers)
  {
    Print(OK, TRUE, "Might be too many.");
    check_trash = 1;
  }
  else
  {
    Print(OK, TRUE, "Reasonable number.");
  }

  if (check_trash)
  {
    read_user(nu);
    if ((xxx = trashed_user()) != 0)
    {
      trashed_userlist = 1;
      Print(NOK, TRUE, "User Record #%u appears trashed (%d).", nu, xxx);
    }
    if (trashed_userlist)
    {
      Print(NOK, TRUE, "Userlist appears to be corrupted.");
    }
    else
    {
      Print(OK, TRUE, "Userlist seems OK.");
    }
  }
  if (trashed_userlist)
  {
    if (!allow_changes)
    {
      give_up();
    }

    Print(OK, TRUE, "Scanning for invalid user records...");

    while ((nu) && (trashed_user()))
    {
      read_user(--nu);
    }

    Print(OK, TRUE, "There appears to be %u 'real' user records.", nu);

    if (!force_changes)
    {
      Print(QOK, FALSE, "Truncate userlist to %u users? ", nu);
      if (!yn())
      {
        trashed_userlist = 0;
      }
    }
    if (trashed_userlist)
    {
      l = ((long) nu) * ((long) syscfg.userreclen);
      chsize(userfile, l);
      Print(NOK, TRUE, "User Record Truncated at %u users!!", nu);
    }
  }
}


/****************************************************************************/

void isr1(int un, char *name)
{
  int cp;
  smalrec sr;

  cp = 0;
  while ((cp < cur_users) && (strcmp(name, (new_smallist[cp].name)) > 0))
    ++cp;
  memmove(&(new_smallist[cp + 1]), &(new_smallist[cp]), sizeof(smalrec) * (cur_users - cp));
  strcpy(sr.name, name);
  sr.number = un;
  new_smallist[cp] = sr;
  ++cur_users;
}

/****************************************************************************/


void check_nameslist(void)
{
  char s[81];
  int st = 0, i, i1, i2, xxx;
  unsigned int status_users, names_users;

  Print(OK, TRUE, "Scanning NAMES.LST file...");
  sprintf(s, "%sNAMES.LST", syscfg.datadir);
  i = sh_open1(s, O_RDONLY | O_BINARY);
  if (i > 0) {
    if (filelength(i)) {
      smallist = (smalrec *) bbsmalloc(filelength(i) + 1);
      oom(smallist, "smallist", filelength(i));
      read(i, smallist, (unsigned) filelength(i));
      names_users = (unsigned int) (filelength(i) / sizeof(smalrec));
      status_users = status.users;
    } else
      names_users = 0;
    sh_close(i);

    if (names_users != status_users) {
      status.users = names_users;
      Print(NOK, TRUE, "STATUS.DAT contained an incorrect user count.");
      if (allow_changes) {
        Print(OK, TRUE, "Fixed user count in STATUS.DAT");
        save_status();
      } else {
        Print(OK, TRUE, "Did NOT fix user count in STATUS.DAT");
      }
    }
  } else {
    names_users = 0;
    status.users = status.users;
    Print(NOK, TRUE, "%s NOT FOUND.", s);
  }

  cur_users = 0;

  i1 = filelength(userfile) / syscfg.userreclen - 1;

  new_smallist = (smalrec *) bbsmalloc(((long) i1 + 1) * ((long) sizeof(smalrec)));
  oom(new_smallist, "new_smallist", ((long) i1) * ((long) sizeof(smalrec)));

  for (i = 1; i <= i1; i++) {
    read_user(i);
    if ((*thisuser_inact & inact_deleted) == 0) {
      if ((xxx = trashed_user()) != 0) {
#ifdef NOT_WORTH_IT
        Print(NOK, TRUE, "User #%d had trashed info (%d); patching.", i, xxx);
        fix_user();
        write_user(i);
#else
        i2 = xxx;
        xxx = i2;
#endif
      }
      isr1(i, thisuser);
    }
//    if ((i % 10) == 0)
//    {
//      Print(OK, FALSE, "%d/%d\r", i, i1);
//    }
  }
//  Print(OK, FALSE, "%d/%d", i1, i1);

  if (cur_users != names_users) {
    st = 1;
    Print(NOK, TRUE, "A different number of users was found (%d vs %d).",
           cur_users, names_users);

  } else {
    for (i = 0; i < names_users; i++) {
      if (strcmp(smallist[i].name, new_smallist[i].name))
        st = 1;
      if (smallist[i].number != new_smallist[i].number)
        st = 1;
    }
  }
  if (st) {
    Print(NOK, TRUE, "The new NAMES.LST file is different than the old one.");
    if (allow_changes) {
      status.users = cur_users;
      i = sh_open(s, O_RDWR | O_BINARY | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE);
      if (i < 0) {
        Print(NOK, TRUE, "Couldn't create %s.", s);
        give_up();
      }
      write(i, (void *) (new_smallist), (sizeof(smalrec) * status.users));
      sh_close(i);

      save_status();
      Print(OK, TRUE, "NAMES.LST file fixed.");
    } else {
      Print(NOK, TRUE, "Leaving it alone, but it should be fixed.  There is nothing bad that can");
      Print(NOK, TRUE, "happen by fixing the NAMES.LST file.");
    }
  }
  if (smallist) {
    bbsfree(smallist);
    smallist = NULL;
  }
  if (new_smallist) {
    bbsfree(new_smallist);
    new_smallist = NULL;
  }
}

/****************************************************************************/

void set_gat_section(int f, int section)
{
  long l, l1;
  int i;

  if (gat_section != section) {
    l = filelength(f);
    l1 = ((long) section) * GATSECLEN;
    if (l < l1) {
      chsize(f, l1);
      l = l1;
    }
    lseek(f, l1, SEEK_SET);
    if (l < (l1 + 4096)) {
      for (i = 0; i < 2048; i++)
        gat[i] = 0;
      write(f, (void *) gat, 4096);
    } else {
      read(f, (void *) gat, 4096);
    }
    gat_section = section;
  }
}

/****************************************************************************/


void save_gat(int f)
{
  long l;

  l = ((long) gat_section) * GATSECLEN;
  lseek(f, l, SEEK_SET);
  write(f, (void *) gat, 4096);
}

/****************************************************************************/



void remove_link(messagerec * m1, char *aux)
{
  messagerec m;
  char s[81], s1[81];
  int f;
  long csec, nsec;

  m = *m1;
  strcpy(s, syscfg.msgsdir);
  switch (m.storage_type) {
    case 0:
    case 1:
      ltoa(m.stored_as, s1, 16);
      if (m.storage_type == 1) {
        strcat(s, aux);
        strcat(s, "\\");
      }
      strcat(s, s1);
      unlink(s);
      break;
    case 2:
      f = open_file(aux);
      if (f >= 0) {
        set_gat_section(f, (int) (m.stored_as / 2048));
        csec = m.stored_as % 2048;
        while ((csec > 0) && (csec < 2048)) {
          nsec = (long) gat[csec];
          gat[csec] = 0;
          csec = nsec;
        }
        save_gat(f);
        sh_close(f);
      }
      break;
    default:
      /* illegal storage type */
      break;
  }
}

/****************************************************************************/


void delmail(int f, int loc)
{
  mailrec m, m1;
  int rm, i, t, otf;
  char s[81];

  sprintf(s, "%sEMAIL.DAT", syscfg.datadir);
  f = sh_open1(s, O_RDWR | O_BINARY);


  lseek(f, ((long) loc) * ((long) sizeof(mailrec)), SEEK_SET);
  read(f, (void *) &m, sizeof(mailrec));

  rm = 1;
  if (m.status & status_multimail) {
    t = filelength(f) / sizeof(mailrec);
    otf = 0;
    for (i = 0; i < t; i++)
      if (i != loc) {
        lseek(f, ((long) i) * ((long) sizeof(mailrec)), SEEK_SET);
        read(f, (void *) &m1, sizeof(mailrec));
        if ((m.msg.stored_as == m1.msg.stored_as) && (m.msg.storage_type == m1.msg.storage_type) && (m1.daten != 0xffffffff))
          otf = 1;
      }
    if (otf)
      rm = 0;
  }
  sh_close(f);
  if (rm)
    remove_link(&m.msg, "EMAIL");

  if (m.tosys == 0) {
#ifdef DEC_MAIL_WAITING
    read_user(m.touser, &u);
    if (u.waiting) {
      --u.waiting;
      write_user(m.touser, &u);
      close_user();
    }
#endif
  }
  f = sh_open1(s, O_RDWR | O_BINARY);
  lseek(f, ((long) loc) * ((long) sizeof(mailrec)), SEEK_SET);
  m.touser = 0;
  m.tosys = 0;
  m.daten = 0xffffffff;
  m.msg.storage_type = 0;
  m.msg.stored_as = 0xffffffff;
  write(f, (void *) &m, sizeof(mailrec));
  sh_close(f);
}

/****************************************************************************/

void find_max_qscan(void)
{
  int i, cs, r, t, f, any_screwed, here_screwed;
  unsigned long maxthissub;
  mailrec m;
  char s[81];
  postrec *p;

  max_qscan = 0L;
  any_screwed = 0;
  num_type_0 = 0;

  Print(OK, TRUE, "Scanning subboards...");

  for (cs = 0; cs < num_subs; cs++) {
    iscan1(cs, 0);
    open_sub(0);
    maxthissub = 0L;
    here_screwed = 0;
    if (nummsgs) {
      for (i = 1; i <= nummsgs; i++) {
        p = get_post(i);
        if (maxthissub >= p->qscan) {
          Print(OK,TRUE, "Scanning sub '%s'...", subboards[cs].name);
          Print(NOK, TRUE, "Message '%d': %s", i+1, p->title);
          any_screwed = 1;
          here_screwed = 1;
        }
        if (maxthissub < p->qscan)
          maxthissub = p->qscan;
        if (max_qscan < p->qscan)
          max_qscan = p->qscan;
        if (p->msg.storage_type < 2) {
          /* if (max_qscan<p->msg.stored_as) max_qscan=p->msg.stored_as; */
          if (p->msg.storage_type == 0)
            ++num_type_0;
        }
      }
      if (here_screwed)
      {
        Print(NOK, TRUE, "Sub '%s' has q-scan pointers are invalid.", subboards[cs].name);
      }
    }
    close_sub();
  }

  Print(OK, TRUE, "Scanning EMAIL.DAT file...");

  sprintf(s, "%sEMAIL.DAT", syscfg.datadir);
  f = sh_open1(s, O_BINARY | O_RDWR);
  if (f != -1) {
    t = (int) (filelength(f) / sizeof(mailrec));
    r = 0;
    numemailm = t;
    emailm = (messagerec *) bbsmalloc((t + 1) * sizeof(messagerec));
    emailmmm = (char *) bbsmalloc(t + 1);
    if (t) {
      oom(emailm, "emailm", t * sizeof(messagerec));
      oom(emailmmm, "emailmmm", t);
    }
    while (r < t) {
      lseek(f, (long) (sizeof(mailrec)) * (long) (r), SEEK_SET);
      read(f, (void *) &m, sizeof(mailrec));
      emailm[r] = m.msg;
      emailmmm[r] = m.status;
      if ((m.tosys != 0) || (m.touser != 0)) {
        if (m.msg.storage_type < 2)
          /* if (max_qscan<m.msg.stored_as) max_qscan=m.msg.stored_as; */
          if (m.msg.storage_type == 0)
            ++num_type_0;
      }
      ++r;
    }
    sh_close(f);
  }
  max_qscan++;
  if (max_qscan > status.qscanptr) {
    Print(NOK, TRUE, "Max qscan pointer trashed (%lu vs %lu).", max_qscan, status.qscanptr);
    status.qscanptr = max_qscan;
    if (allow_changes) {
      save_status();
      Print(OK, TRUE, "Fixed.");
    } else
      Print(OK, TRUE, "Leaving as-is, but it should be fixed.");
  }
  if (max_qscan >= 0x80000000) {
    Print(NOK, TRUE, "Max qscan pointer indicates qscan troubles (too big).");
    any_screwed = 1;
  }
  if ((allow_changes) && (any_screwed)) {
    if (!force_changes) {
      Print(QOK, FALSE, "Reset all qscan pointers on BBS? ", QOK);
      if (!yn())
        any_screwed = 0;
    }
    if (any_screwed) {
      Print(OK, TRUE, "Resetting qscan pointers... do //resetqscan from mainmenu also.");
      max_qscan = 1;
      status.qscanptr = max_qscan;
      for (cs = 0; cs < num_subs; cs++) {
        if (iscan1(cs, 0)) {
          open_sub(1);
          for (i = 1; i <= nummsgs; i++) {
            p = get_post(i);
            if (p) {
              p->qscan = status.qscanptr++;
              write_post(i, p);
            }
          }
          close_sub();
        }
      }
      save_status();
    }
  }
}


/****************************************************************************/


typedef struct {
  long stored_as;
  int subnum, msgnum;
} type_0;


/****************************************************************************/


char *describem(char *s, int subnum, int msgnum)
{
  mailrec me;
  postrec *mp;
  char s1[81];
  int f;

  if (subnum == -1) {                       /* email */
    sprintf(s1, "%sEMAIL.DAT", syscfg.datadir);
    f = sh_open1(s1, O_BINARY | O_RDWR);
    lseek(f, (long) (sizeof(mailrec)) * (long) (msgnum), SEEK_SET);
    read(f, (void *) &me, sizeof(mailrec));
    sh_close(f);

    if (me.fromsys)
      sprintf(s1, "user #%d @%d", me.fromuser, me.fromsys);
    else
      sprintf(s1, "user #%d", me.fromuser);

    sprintf(s, "Mail from %s to ", s1);

    if (me.tosys)
      sprintf(s1, "user #%d @%d", me.touser, me.tosys);
    else
      sprintf(s1, "user #%d", me.touser);

    strcat(s, s1);

    sprintf(s1, " '%.30s'", me.title);
    strcat(s, s1);

  } else {
    iscan1(subnum, 0);
    mp = get_post(msgnum);
    if (mp) {
      if (mp->ownersys)
        sprintf(s, "Post #%d on '%s' by user #%d @%d",
                msgnum + 1,
                subboards[subnum].name,
                mp->owneruser,
                mp->ownersys);
      else
        sprintf(s, "Post #%d on '%s' by user #%d",
                msgnum + 1,
                subboards[subnum].name,
                mp->owneruser);
    } else {
      sprintf(s, "Unreadable (?) post #%d on '%s'",
              msgnum + 1, subboards[subnum].name);
    }
  }
  return (s);
}


/****************************************************************************/


void check_type_1(int num, messagerec * list, char *extra, int todesc)
{
  char s[81], s1[81];
  int i, i1, dup, ex;
  char *already_done;

  if (!num)
    return;

  sprintf(s, "%s%s\\", syscfg.msgsdir, extra);
  already_done = (char *) bbsmalloc(num);
  sprintf(s1, "already_done(%s)", extra);
  oom(already_done, s1, num);

  for (i = 0; i < num; i++)
    already_done[i] = 0;

  for (i = 0; i < num; i++)
    if ((list[i].storage_type == 1) && (!already_done[i])) {
      sprintf(s1, "%s%lX", s, list[i].stored_as);
      ex = exist(s1);
      dup = 0;
      if (!((todesc == -1) && (emailmmm[i] & status_multimail))) {
        for (i1 = i + 1; i1 < num; i1++)
          if (list[i1].storage_type == 1)
            if (list[i1].stored_as == list[i].stored_as)
              dup = 1;
      }
      if (dup || (!ex)) {
        if (!ex)
          Print(NOK, TRUE, "File not found: '%s'", s1);
        if (dup)
          Print(NOK, TRUE, "Message file multiply referenced: '%s'", s1);
        Print(NOK, TRUE, "  for %s", describem(s1, todesc, i));

        for (i1 = i + 1; i1 < num; i1++)
          if ((list[i1].storage_type == 1) &&
              (list[i1].stored_as == list[i].stored_as)) {
            Print(NOK, TRUE,"  for %s", describem(s1, todesc, i1));
            already_done[i1] = 1;
          }
        Print(NOK, TRUE,"  No action taken.");
      }
    }
  bbsfree(already_done);
}



void check_type_2(int num, messagerec * list, char *extra, int todesc)
{
  int f, i, any, csec, csec1, any_dead, num_deadm, anything_done, numsec, sec;
  char s[81];
  int *deadm;
  short huge *gati, huge * gatint;
  long high;

  if (!num)
    return;

  f = open_file(extra);
  numsec = ((int) (filelength(f) / GATSECLEN)) + 1;
  high = ((long) numsec) * 2048;

  gati = (short *) bbsmalloc(high * 2 + 1);
  sprintf(s, "gati(%s)", extra);
  oom((void *) gati, s, high * 2);
  gatint = (short *) bbsmalloc(high * 2 + 1);
  sprintf(s, "gatint(%s)", extra);
  oom((void *) gatint, s, high * 2);


  for (i = 0; i < high; i++)
    gati[i] = 0;

  for (i = 0; i < numsec; i++) {
    set_gat_section(f, i);
    memcpy((char *) (gatint + 2048 * i), gat, 4096);
  }

  deadm = (int *) bbsmalloc(num * 2);
  sprintf(s, "deadm(%s)", extra);
  oom(deadm, s, num * 2);

  any = 0;
  any_dead = 0;
  num_deadm = 0;
  anything_done = 0;

  for (i = 0; i < num; i++)
    if (list[i].storage_type == 2) {
      if (f < 0) {
        if (!any) {
          any = 1;
          Print(NOK, TRUE, "Type 2 data file not found: '%s%s.DAT'",
                 syscfg.msgsdir, extra);
        }
        deadm[num_deadm++] = i;
        anything_done = 1;
      } else {
        sec = (list[i].stored_as / 2048) * 2048;
        csec = list[i].stored_as % 2048;
        while ((csec > 0) && (csec < 2048)) {
          if (gati[csec + sec]) {
            if (!((todesc == -1) && (emailmmm[i] & status_multimail))) {
              if (gati[csec + sec] != -2)
                gati[csec + sec] = -1;
              ++any_dead;
            }
            csec = -1;
          } else {
            if (!gatint[csec + sec]) {
              gati[csec + sec] = -2;
              ++any_dead;
              csec = -1;
            } else {
              gati[csec + sec] = i + 1;
              csec = gatint[csec + sec];
            }
          }
        }
      }
    }
  if (f < 0) {
    if (anything_done) {

      Print(NOK, TRUE, "FIX.EXE doesn't know what to do about that.");

    }
    bbsfree((void *) gati);
    bbsfree((void *) deadm);
    bbsfree((void *) gatint);
    sh_close(f);
    return;
  }
  if (any_dead) {
    anything_done = 1;
    Print(NOK, TRUE, "Errors in '%s%s.DAT':", syscfg.msgsdir, extra);
    for (i = 0; i < num; i++)
      if (list[i].storage_type == 2) {
        sec = (list[i].stored_as / 2048) * 2048;
        csec = list[i].stored_as % 2048;
        csec1 = -1;
        while ((csec > 0) && (csec < 2048)) {
          if (gati[csec + sec] < 0) {
            Print(NOK, TRUE, "  for %s", describem(s, todesc, i));
            if (gati[csec + sec] == -1) {
              Print(NOK, TRUE,"    Collided on cluster #%d", csec + sec);
              if (csec1 == -1) {
                Print(NOK, TRUE, "    First cluster of message, removing.");
                deadm[num_deadm++] = i;
              } else {
                gatint[csec1 + sec] = -1;
                Print(NOK, TRUE,"    Truncating message.");
              }
            } else
              if (gati[csec + sec] == -2) {
              Print(NOK, TRUE, "    Pointed to unallocated cluster #%d", csec + sec);
              if (csec1 == -1) {
                Print(NOK, TRUE, "    First cluster of message, removing.");
                deadm[num_deadm++] = i;
              } else {
                gatint[csec1 + sec] = -1;
                Print(NOK, TRUE, "    Truncating message.");
              }
            } else {
              Print(NOK, TRUE, "    Unknown error.");
            }
          }
          csec1 = csec;
          csec = gatint[csec + sec];
        }
      }
  }
  any_dead = 0;
  for (csec = 0; csec < numsec; csec++)
    if ((gatint[csec]) && (gati[csec] <= 0)) {
      gatint[csec] = 0;
      ++any_dead;
    }
  if (any_dead) {
    Print(NOK, TRUE, "Lost clusters recovered from '%s%s.DAT': %d",
           syscfg.msgsdir, extra, any_dead);
    anything_done = 1;
  }
  if (anything_done) {
    i = force_changes;
    if ((allow_changes) && (!i)) {
      Print(QOK, FALSE, "Write these changes to disk? ");
      i = yn();
    }
    if (i) {
      Print(OK, TRUE, "Updating '%s%s.DAT'.", syscfg.msgsdir, extra);
      for (i = 0; i < numsec; i++) {
        set_gat_section(f, i);
        memcpy(gat, (char *) (gatint + 2048 * i), 4096);
        save_gat(f);
      }
      if (num_deadm) {
        Print(OK, TRUE, "Removing messages with no text.");
        if (todesc != -1) {
          iscan1(todesc, 0);

          for (i = 0; i < num_deadm; i++)
            delete(deadm[i] - i + 1);

        } else {
          for (i = 0; i < num_deadm; i++)
            delmail(f, deadm[i]);

        }
      }
    }
  }
  sh_close(f);
  bbsfree((void *) deadm);
  bbsfree((void *) gati);
  bbsfree((void *) gatint);
}

/****************************************************************************/


int open_file(char *fn)
{
  int f;
  char s[81];

  sprintf(s, "%s%s.DAT", syscfg.msgsdir, fn);
  f = sh_open1(s, O_RDWR | O_BINARY);
  if (f < 0)
    return (-1);

  lseek(f, 0L, SEEK_SET);
  read(f, (void *) gat, 4096);
  gat_section = 0;
  return (f);
}


/****************************************************************************/

/****************************************************************************/


void check_msg_consistency(void)
{
  type_0 *x0;
  int cs, i, i1, i2, cp, ex;
  char s[81], s1[81];
  postrec *p;
  messagerec *pm;

  /* check type 0 consistency */

  if (num_type_0) {
    Print(OK, TRUE, "Checking type 0 message consistency.");
    x0 = (type_0 *) bbsmalloc(sizeof(type_0) * num_type_0);
    oom(x0, "x0", sizeof(type_0) * num_type_0);
    cp = 0;
    for (cs = 0; cs < num_subs; cs++) {
      iscan1(cs, 0);
      open_sub(0);
      for (i = 1; i <= nummsgs; i++) {
        p = get_post(i);
        if ((p->msg.storage_type == 0) && (p->msg.stored_as != (unsigned long) -1)) {
          x0[cp].stored_as = p->msg.stored_as;
          x0[cp].subnum = cs;
          x0[cp].msgnum = i + 1;
          ++cp;
        }
      }
      close_sub();
    }
    for (cs = 0; cs < numemailm; cs++)
      if ((!emailm[cs].storage_type) && (emailm[cs].stored_as != (unsigned long) -1)) {
        x0[cp].stored_as = emailm[cs].stored_as;
        x0[cp].subnum = -1;
        x0[cp].msgnum = cs;
        ++cp;
      }
    for (i = 0; i < cp; i++) {
      if (x0[i].subnum != -2) {
        sprintf(s, "%s%lX", syscfg.msgsdir, x0[i].stored_as);
        ex = exist(s);

        i2 = 0;

        for (i1 = i + 1; i1 < cp; i1++)
          if (x0[i].stored_as == x0[i1].stored_as)
            i2 = 1;

        if (i2 || (!ex)) {
          if (!ex)
            Print(NOK, TRUE, "File not found: '%s'", s);
          if (i2)
            Print(NOK, TRUE, "Message file multiply referenced: '%s'", s);
          Print(NOK, TRUE, "  for %s", describem(s1, x0[i].subnum, x0[i].msgnum));
          for (i1 = i + 1; i1 < cp; i1++)
            if (x0[i].stored_as == x0[i1].stored_as) {
              Print(NOK, TRUE, "  for %s", describem(s1, x0[i1].subnum, x0[i1].msgnum));
              x0[i1].subnum = -2;
            }
          Print(NOK, TRUE, "  No action taken.");
        }
      }
    }

    bbsfree(x0);
  }
  /* check type 1&2 consistency */

  Print(OK, TRUE, "Checking type 1&2 message consistency.");
  check_type_1(numemailm, emailm, "EMAIL", -1);
  check_type_2(numemailm, emailm, "EMAIL", -1);
  for (cs = 0; cs < num_subs; cs++) {
    iscan1(cs, 0);
    if (nummsgs) {
      pm = (messagerec *) bbsmalloc(nummsgs * sizeof(messagerec));
      open_sub(1);
      sprintf(s, "pm(%s)", subboards[cs].filename);
      oom(pm, s, nummsgs * sizeof(messagerec));
      for (i = 1; i <= nummsgs; i++) {
        pm[i - 1] = get_post(i)->msg;
      }
      check_type_1(nummsgs, pm, subboards[cs].filename, cs);
      check_type_2(nummsgs, pm, subboards[cs].filename, cs);
      close_sub();
      bbsfree(pm);
    }
  }
}


/****************************************************************************/


void ck_size(char *fn, int f, long rl)
{
  long l;

  l = filelength(f);
  if (l < rl) {
    Print(NOK, TRUE, "%s too short (%ld<%ld).", fn, l, rl);
    give_up();
  }
  if (l > rl) {
    Print(NOK, TRUE, "%s too long (%ld>%ld).", fn, l, rl);
    if (allow_changes)
      Print(NOK, TRUE, "Attempting to continue.");
    else
      give_up();
  }
}

/****************************************************************************/

void init(void)
{
  char s[81], date[9], log[12];
  int i, chng = 0;
  struct date d;
  struct time t;
  long val;

  // open CONFIG.DAT
  strcpy(s, "CONFIG.DAT");
  configfile = sh_open1(s, O_RDWR | O_BINARY);

  if (configfile < 0) {
    Print(NOK, TRUE, "%s NOT FOUND.", s);
    give_up();
  }

  // read in the data from the file and close it
  ck_size(s, configfile, sizeof(configrec));
  Print(OK, TRUE, "Reading %s...", s);
  read(configfile, (void *) (&syscfg), sizeof(configrec));
  sh_close(configfile);

  // build current path
  strcpy(cdir, "X:\\");
  cdir[0] = 'A' + getdisk();
  getcurdir(0, &(cdir[3]));

  // if no data directory is here, we can't continue
  if (check_dir(syscfg.datadir, "Data")) {
    Print(NOK, TRUE, "Must find data directory to continue.");
    give_up();
  }

  if (!syscfg.userreclen)
    syscfg.userreclen = sizeof(userrec);

  thisuser = (char *) bbsmalloc(syscfg.userreclen + 1);
  if (!thisuser) {
    Print(NOK, TRUE, "Could not allocate %d bytes for userrec.", syscfg.userreclen);
    give_up();
  }
  thisuser_inact = thisuser + syscfg.inactoffset;

  // open STATUS.DAT
  sprintf(s, "%sSTATUS.DAT", syscfg.datadir);
  statusfile = sh_open1(s, O_RDWR | O_BINARY);
  if (statusfile < 0) {
    // if it's not found, create a new one
    Print(NOK, TRUE, "%s NOT FOUND.", s);
    if (allow_changes) {
      Print(OK, TRUE, "Re-creating STATUS.DAT file.");
      strcpy(status.date1, "00/00/00");
      strcpy(status.date2, status.date1);
      strcpy(status.date3, status.date1);
      strcpy(status.log1, "000000.LOG");
      strcpy(status.log2, status.log1);
      strcpy(status.gfiledate, "00/00/00");
      status.users = 0;
      status.callernum = 65535;
      status.callstoday = 0;
      status.msgposttoday = 0;
      status.localposts = 0;
      status.emailtoday = 0;
      status.fbacktoday = 0;
      status.uptoday = 0;
      status.activetoday = 0;
      status.qscanptr = 0L;
      status.amsganon = 0;
      status.amsguser = 0;
      status.callernum1 = 0L;
      status.net_edit_stuff = 0;
      status.wwiv_version = 0;
      status.net_version = 0;
      status.net_bias = 0.001;
    } else
      give_up();
  } else {

    // read in the data from the file and close it
    ck_size(s, statusfile, sizeof(statusrec));
    Print(OK, TRUE, "Reading %s...", s);
    read(statusfile, (void *) (&status), sizeof(statusrec));
    sh_close(statusfile);

    // do a version check
    if (status.wwiv_version > wwiv_num_version) {
      Print(NOK, TRUE, "Incorrect version of FIX.EXE (this is for %d, you need %d)",
             wwiv_num_version, status.wwiv_version);
      give_up();
    }

    // get date and time
    getdate(&d);
    gettime(&t);

    // check todays date and correct if necessary
    _strdate(date);
    if (strcmp(status.date1, date) != 0) {
      strcpy(status.date1, date);
      chng = 1;
      Print(OK, TRUE, "Date error in STATUS.DAT (status.date1) corrected.");
    }

    // build yesterdays date
    val = dostounix(&d, &t);
    val -= 86400L;
    unixtodos(val, &d, &t);
    sprintf(date, "%02d/%02d/%02d",
                   d.da_mon,
                   d.da_day,
                   d.da_year % 100);

    // check yesterdays date and correct if necessary
    if (strcmp(status.date2, date) != 0) {
      strcpy(status.date2, date);
      chng = 1;
      Print(OK, TRUE, "Date error in STATUS.DAT (status.date2) corrected.");
    }

    // build yesterdays log filename
    sprintf(log,"%02d%02d%02d.LOG",
                   d.da_year % 100,
                   d.da_mon,
                   d.da_day);
    // check todays log filename and correct if necessary
    if (strcmp(status.log1, log) != 0) {
      strcpy(status.log1, log);
      chng = 1;
      Print(OK, TRUE, "Log filename error in STATUS.DAT (status.log1) corrected.");
    }

    // build day before yesterdays date
    val -= 86400L;
    unixtodos(val, &d, &t);
    sprintf(date, "%02d/%02d/%02d",
                   d.da_mon,
                   d.da_day,
                   d.da_year % 100);

    // check day before yesterdays date and correct if necessary
    if (strcmp(status.date3, date) != 0) {
      strcpy(status.date3, date);
      chng = 1;
      Print(OK, TRUE, "Date error in STATUS.DAT (status.date3) corrected.");
    }

    // build day before yesterdays log filename
    sprintf(log,"%02d%02d%02d.LOG",
                   d.da_year % 100,
                   d.da_mon,
                   d.da_day);

    // check day before yesterdays log filename and correct if necessary
    if (strcmp(status.log2, log) != 0) {
      strcpy(status.log2, log);
      chng = 1;
      Print(OK, TRUE, "Log filename error in STATUS.DAT (status.log2) corrected.");
    }
    // if we made changes, save STATUS.DAT
    if (chng)
      save_status();
  }


  sprintf(s, "%sDIRS.DAT", syscfg.datadir);
  Print(OK, TRUE, "Reading %s...", s);
  i = sh_open1(s, O_RDWR | O_BINARY);
  if (i < 0) {
    Print(NOK, TRUE, "%s NOT FOUND.", s);
    maybe_give_up();
  } else {
    directories = (directoryrec *) bbsmalloc(filelength(i) + 1);
    if (!directories) {
      Print(NOK, TRUE, "Couldn't allocate %ld bytes for %s.", filelength(i), s);
      give_up();
    }
    num_dirs = (read(i, directories, (filelength(i)))) /
        sizeof(directoryrec);
    sh_close(i);
  }

  sprintf(s, "%sSUBS.DAT", syscfg.datadir);
  Print(OK, TRUE, "Reading %s...", s);
  i = sh_open1(s, O_RDWR | O_BINARY);
  if (i < 0) {
    Print(NOK, TRUE, "%s NOT FOUND.", s);
    maybe_give_up();
  } else {
    subboards = (subboardrec *) bbsmalloc(filelength(i) + 1);
    if (!subboards) {
      Print(NOK, TRUE, "Couldn't allocate %ld bytes for %s.", filelength(i), s);
      give_up();
    }
    num_subs = (read(i, subboards, (filelength(i)))) /
        sizeof(subboardrec);
    sh_close(i);

  }
}


void ShowBanner(void)
{
#ifdef __MSDOS__
    clrscr();
    textattr(0x0b);
    cprintf("WWIV Bulletin Board System - %s\r\n", wwiv_version);
    textattr(0x0a);
    cprintf("Copyright (c) 1998-2004, WWIV Software Services.\r\n");
    textattr(0x0d);
    cprintf("All Rights Reserved.\r\n\r\n");
    textattr(0x0f);
    cprintf("Compile Time : %s\r\n",wwiv_date);
    textattr(0x07);
#else
    printf("WWIV Bulletin Board System - %s\n", wwiv_version);
    printf("Copyright (c) 1998-2004, WWIV Software Services.\n");
    printf("All Rights Reserved.\n\n");
    printf("Compile Time : %s\n\n",wwiv_date);
#endif

}


void ShowHelp(void)
{
    printf("Command Line Usage:\n\n");
    printf("\t-Y\t= Force Yes to All Prompts\n");
    printf("\t-U\t= Skip User Record Check\n");
    printf("\n");
    exit(0);
}


void Print(int nType, BOOL bLogIt, char* szText, ...)
{
   char szBuffer[256];
   va_list ap;

    bLogIt = bLogIt; // %%TODO Remove this warning kludge!

   va_start(ap, szText);
   vsprintf(szBuffer, szText, ap);
#ifdef __MSDOS__
   switch(nType)
   {
    case NOK:
    {
        textattr(0x0c);
    } break;
    case QOK:
    {
        textattr(0x0a);
    } break;
    case OK:
    default:
    {
        textattr(0x03);
    } break;
   }
   cprintf("%s%s\r\n", szLogTypeArray[nType], szBuffer);
#else
   printf("%s%s\n", szLogTypeArray[nType], szBuffer);
#endif
   va_end(ap);

   if ((bLogIt) && (hLogFile!=NULL))
   {
      struct tm *time_now;
      time_t secs_now;
      char str[81];
      tzset();
      time(&secs_now);
      time_now = localtime(&secs_now);
      strftime(str, 80, "%H:%M:%S", time_now);
      fprintf(hLogFile, "%s%s  %s\n", szLogTypeArray[nType], str, szBuffer);
      fflush(hLogFile);
   }
}


BOOL OpenLogFile(char* szFileName)
{
   struct tm *time_now;
   time_t secs_now;
   char str[81];

   if ((hLogFile = fopen(szFileName, "a+t")) == NULL)
   {
      Print(NOK, FALSE, "Cannot open Log File %s", szFileName);
      return FALSE;
   }

   tzset();
   time(&secs_now);
   time_now = localtime(&secs_now);
   strftime(str, 80, "%a %d %b %Y", time_now);
   fprintf(hLogFile, "\n----------  %s, FIX.EXE %s\n", str, wwiv_version);

   return TRUE;
}


BOOL CloseLogFile()
{
   if (hLogFile != NULL)
   {
      fclose(hLogFile);
   }
   return TRUE;
}

/****************************************************************************/


int main(int argc, char *argv[])
{
  int i;
  char *ss;
  time_t startTime;
  time_t endTime;

  ShowBanner();
  for (i = 1; i < argc; i++) {
    ss = argv[i];
    if ((*ss == '/') || (*ss == '-')) {
      switch (toupper(ss[1])) {
    case 'Y':
      force_yes = 1;
      break;
    case 'U':
      usercheck = 0;
      break;
    case '?':
      ShowHelp();
      break;
      }
    } else {
      Print(NOK, FALSE, "Unknown argument: '%s'", ss);
    }
  }

  // get the current time in seconds
  startTime = time(NULL);
  // open the log
  OpenLogFile("FIX.LOG");

  if (getenv("BBS")) {
    printf("Fix should only be run OUTSIDE the BBS.\n\n");
    exit(-1);
  }
  // read in all critical data
  init();

  check_all_dirs();
  check_crit_files();
  check_userlist();
  if (usercheck)
    check_nameslist();
  find_max_qscan();
  check_msg_consistency();

  endTime = time(NULL);

  Print(OK, TRUE, "FIX Completed.  Time elapsed: %d seconds\n\n", (endTime-startTime));
  textattr(0x07);
  cprintf("\r\n");

  return 0;
}
