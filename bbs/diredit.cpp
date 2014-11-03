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

#include "wwiv.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "bbs/keycodes.h"


using wwiv::bbs::InputMode;

//
// Local Function Prototypes
//

void dirdata(int n, char *s);
void showdirs();
void modify_dir(int n);
void swap_dirs(int dir1, int dir2);
void insert_dir(int n);
void delete_dir(int n);


void dirdata(int n, char *s) {
  char x = 0;
  directoryrec r = directories[n];
  if (r.dar == 0) {
    x = 32;
  } else {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.dar) {
        x = static_cast< char >('A' + i);
      }
    }
  }
  sprintf(s, "|#2%4d |#9%1c   |#1%-39.39s |#2%-8s |#9%-3d %-3d %-3d %-9.9s",
          n, x, stripcolors(r.name), r.filename, r.dsl, r.age, r.maxfiles,
          r.path);
}

void showdirs() {
  char s[180], s1[21];

  bout.cls();
  bout << "|#7(|#1File Areas Editor|#7) Enter Substring: ";
  input(s1, 20, true);
  bool abort = false;
  pla("|#2##   DAR Area Description                        FileName DSL AGE FIL PATH", &abort);
  pla("|#7==== --- ======================================= -------- === --- === ---------", &abort);
  for (int i = 0; i < GetSession()->num_dirs && !abort; i++) {
    sprintf(s, "%s %s", directories[i].name, directories[i].filename);
    if (stristr(s, s1)) {
      dirdata(i, s);
      pla(s, &abort);
    }
  }
}


char* GetAttributeString(directoryrec r, char* pszAttributes) {
  char szBuffer[255];

  strcpy(szBuffer, "None.");
  if (r.dar != 0) {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.dar) {
        szBuffer[0] = static_cast< char >('A' + i);
      }
    }
    szBuffer[1] = 0;
  }
  strcpy(pszAttributes, szBuffer);
  return pszAttributes;
}

#define LAST(s) s[strlen(s)-1]


void modify_dir(int n) {
  char s[81], ch, ch2;
  char szSubNum[81];
  int i;

  directoryrec r = directories[n];
  bool done = false;
  do {
    bout.cls();
    sprintf(szSubNum, "%s %d", "|B1|15Editing File Area #", n);
    bout.bprintf("%-85s", szSubNum);
    bout.Color(0);
    bout.nl(2);
    bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
    bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
    bout << "|#9C) Path       : |#2" << r.path << wwiv::endl;
    bout << "|#9D) DSL        : |#2" << static_cast<int>(r.dsl) << wwiv::endl;
    bout << "|#9E) Min. Age   : |#2" << static_cast<int>(r.age) << wwiv::endl;
    bout << "|#9F) Max Files  : |#2" << r.maxfiles << wwiv::endl;
    bout << "|#9G) DAR        : |#2" << GetAttributeString(r, s)  << wwiv::endl;
    bout << "|#9H) Require PD : |#2" << YesNoString((r.mask & mask_PD) ? true : false) << wwiv::endl;
    bout << "|#9I) Dir Type   : |#2" <<  r.type << wwiv::endl;
    bout << "|#9J) Uploads    : |#2" << ((r.mask & mask_no_uploads) ? "Disallowed" : "Allowed") << wwiv::endl;
    bout << "|#9K) Arch. only : |#2" << YesNoString((r.mask & mask_archive) ? true : false) << wwiv::endl;
    bout << "|#9L) Drive Type : |#2" << ((r.mask & mask_cdrom) ? "|#3CD ROM" : "HARD DRIVE") << wwiv::endl;
    if (r.mask & mask_cdrom) {
      bout << "|#9M) Available  : |#2" << YesNoString((r.mask & mask_offline) ? true : false) << wwiv::endl;
    }
    bout << "|#9N) //UPLOADALL: |#2" << YesNoString((r.mask & mask_uploadall) ? true : false) << wwiv::endl;
    bout << "|#9O) WWIV Reg   : |#2" << YesNoString((r.mask & mask_wwivreg) ? true : false) << wwiv::endl;
    bout.nl();
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1M|#7,|#1[|#7,|#1]|#7) : ";
    ch = onek("QABCDEFGHIJKLMNO[]", true);
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '[':
      directories[n] = r;
      if (--n < 0) {
        n = GetSession()->num_dirs - 1;
      }
      r = directories[n];
      break;
    case ']':
      directories[n] = r;
      if (++n >= GetSession()->num_dirs) {
        n = 0;
      }
      r = directories[n];
      break;
    case 'A':
      bout.nl();
      bout << "|#2New name? ";
      Input1(s, r.name, 40, true, InputMode::MIXED);
      if (s[0]) {
        strcpy(r.name, s);
      }
      break;
    case 'B':
      bout.nl();
      bout << "|#2New filename? ";
      Input1(s, r.filename, 8, true, InputMode::FILENAME);
      if ((s[0] != 0) && (strchr(s, '.') == 0)) {
        strcpy(r.filename, s);
      }
      break;
    case 'C':
      bout.nl();
      bout << "|#9Enter new path, optionally with drive specifier.\r\n" <<
                         "|#9No backslash on end.\r\n\n" <<
                         "|#9The current path is:\r\n" <<
                         "|#1" << r.path << wwiv::endl << wwiv::endl;
      bout << " \b";
      Input1(s, r.path, 79, true, InputMode::MIXED);
      if (s[0]) {
        WFile dir(s);
        if (!dir.Exists()) {
          GetApplication()->CdHome();
          if (WWIV_make_path(s)) {
            bout << "|#6Unable to create or change to directory." << wwiv::endl;
            pausescr();
            s[0] = 0;
          }
        }
        if (s[0]) {
          if (LAST(s) != WFile::pathSeparatorChar) {
            strcat(s, WFile::pathSeparatorString);
          }
          strcpy(r.path, s);
          bout.nl(2);
          bout << "|#3The path for this directory is changed.\r\n";
          bout << "|#9If there are any files in it, you must manually move them to the new directory.\r\n";
          pausescr();
        }
      }
      break;
    case 'D': {
      bout.nl();
      bout << "|#2New DSL? ";
      input(s, 3);
      int dsl = atoi(s);
      if (dsl >= 0 && dsl < 256 && s[0]) {
        r.dsl = static_cast<unsigned char>(dsl);
      }
    }
    break;
    case 'E': {
      bout.nl();
      bout << "|#2New Min Age? ";
      input(s, 3);
      int age = atoi(s);
      if (age >= 0 && age < 128 && s[0]) {
        r.age = static_cast<unsigned char>(age);
      }
    }
    break;
    case 'F':
      bout.nl();
      bout << "|#2New max files? ";
      input(s, 4);
      i = atoi(s);
      if ((i > 0) && (i < 10000) && (s[0])) {
        r.maxfiles = static_cast<unsigned short>(i);
      }
      break;
    case 'G':
      bout.nl();
      bout << "|#2New DAR (<SPC>=None) ? ";
      ch2 = onek("ABCDEFGHIJKLMNOP ");
      if (ch2 == SPACE) {
        r.dar = 0;
      } else {
        r.dar = 1 << (ch2 - 'A');
      }
      break;
    case 'H':
      r.mask ^= mask_PD;
      break;
    case 'I':
      bout << "|#2New Dir Type? ";
      input(s, 4);
      i = atoi(s);
      if ((s[0]) && (i != r.type)) {
        r.type = static_cast<unsigned short>(i);
      }
      break;
    case 'J':
      r.mask ^= mask_no_uploads;
      break;
    case 'K':
      r.mask ^= mask_archive;
      break;
    case 'L':
      r.mask ^= mask_cdrom;
      if (r.mask & mask_cdrom) {
        r.mask |= mask_no_uploads;
      } else {
        r.mask &= ~mask_offline;
      }
      break;
    case 'M':
      if (r.mask & mask_cdrom) {
        r.mask ^= mask_offline;
      }
      break;
    case 'N':
      r.mask ^= mask_uploadall;
      break;
    case 'O':
      r.mask &= ~mask_wwivreg;
      bout.nl();
      bout << "|#5Require WWIV registration? ";
      if (yesno()) {
        r.mask |= mask_wwivreg;
      }
      break;
    }
  } while (!done && !hangup);

  directories[n] = r;
}


void swap_dirs(int dir1, int dir2) {
  SUBCONF_TYPE dir1conv = static_cast<SUBCONF_TYPE>(dir1);
  SUBCONF_TYPE dir2conv = static_cast<SUBCONF_TYPE>(dir2);

  if (dir1 < 0 || dir1 >= GetSession()->num_dirs || dir2 < 0 || dir2 >= GetSession()->num_dirs) {
    return;
  }

  update_conf(CONF_DIRS, &dir1conv, &dir2conv, CONF_UPDATE_SWAP);

  dir1 = static_cast<int>(dir1conv);
  dir2 = static_cast<int>(dir2conv);

  int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

  uint32_t *pTempQScan = static_cast<uint32_t*>(BbsAllocA(syscfg.qscn_len));
  WWIV_ASSERT(pTempQScan != nullptr);
  if (pTempQScan) {
    for (int i = 1; i <= nNumUserRecords; i++) {
      read_qscn(i, pTempQScan, true);
      uint32_t* pTempQScan_n = pTempQScan + 1;

      int i1 = 0;
      if (pTempQScan_n[dir1 / 32] & (1L << (dir1 % 32))) {
        i1 = 1;
      }

      int i2 = 0;
      if (pTempQScan_n[dir2 / 32] & (1L << (dir2 % 32))) {
        i2 = 1;
      }
      if (i1 + i2 == 1) {
        pTempQScan_n[dir1 / 32] ^= (1L << (dir1 % 32));
        pTempQScan_n[dir2 / 32] ^= (1L << (dir2 % 32));
      }
      write_qscn(i, pTempQScan, true);
    }
    close_qscn();
    free(pTempQScan);
  }
  directoryrec drt = directories[dir1];
  directories[dir1] = directories[dir2];
  directories[dir2] = drt;

  unsigned long tl = GetSession()->m_DirectoryDateCache[dir1];
  GetSession()->m_DirectoryDateCache[dir1] = GetSession()->m_DirectoryDateCache[dir2];
  GetSession()->m_DirectoryDateCache[dir2] = tl;
}


void insert_dir(int n) {
  SUBCONF_TYPE nconv = static_cast<SUBCONF_TYPE>(n);

  if (n < 0 || n > GetSession()->num_dirs) {
    return;
  }

  update_conf(CONF_DIRS, &nconv, nullptr, CONF_UPDATE_INSERT);

  n = static_cast< int >(nconv);

  int i;
  for (i = GetSession()->num_dirs - 1; i >= n; i--) {
    directories[i + 1] = directories[i];
    GetSession()->m_DirectoryDateCache[i + 1] = GetSession()->m_DirectoryDateCache[i];
  }

  directoryrec r;
  strcpy(r.name, "** NEW DIR **");
  strcpy(r.filename, "NONAME");
  strcpy(r.path, syscfg.dloadsdir);
  r.dsl = 10;
  r.age = 0;
  r.maxfiles = 50;
  r.dar = 0;
  r.type = 0;
  r.mask = 0;
  directories[n] = r;
  ++GetSession()->num_dirs;

  int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

  uint32_t* pTempQScan = static_cast<uint32_t*>(BbsAllocA(syscfg.qscn_len));
  WWIV_ASSERT(pTempQScan != nullptr);
  if (pTempQScan) {
   uint32_t* pTempQScan_n = pTempQScan + 1;

    uint32_t m1 = 1L << (n % 32);
    uint32_t m2 = 0xffffffff << ((n % 32) + 1);
    uint32_t m3 = 0xffffffff >> (32 - (n % 32));

    for (i = 1; i <= nNumUserRecords; i++) {
      read_qscn(i, pTempQScan, true);

      int i1;
      for (i1 = GetSession()->num_dirs / 32; i1 > n / 32; i1--) {
        pTempQScan_n[i1] = (pTempQScan_n[i1] << 1) | (pTempQScan_n[i1 - 1] >> 31);
      }
      pTempQScan_n[i1] = m1 | (m2 & (pTempQScan_n[i1] << 1)) | (m3 & pTempQScan_n[i1]);

      write_qscn(i, pTempQScan, true);
    }
    close_qscn();
    free(pTempQScan);
  }
}


void delete_dir(int n) {
  int i, i1;
  uint32_t *pTempQScan, *pTempQScan_n, m2, m3;
  SUBCONF_TYPE nconv;

  nconv = static_cast< SUBCONF_TYPE >(n);

  if ((n < 0) || (n >= GetSession()->num_dirs)) {
    return;
  }

  update_conf(CONF_DIRS, &nconv, nullptr, CONF_UPDATE_DELETE);

  n = static_cast<int>(nconv);

  for (i = n; i < GetSession()->num_dirs; i++) {
    directories[i] = directories[i + 1];
    GetSession()->m_DirectoryDateCache[i] = GetSession()->m_DirectoryDateCache[i + 1];
  }
  --GetSession()->num_dirs;

  int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

  pTempQScan = static_cast<uint32_t*>(BbsAllocA(syscfg.qscn_len));
  WWIV_ASSERT(pTempQScan != nullptr);
  if (pTempQScan) {
    pTempQScan_n = pTempQScan + 1;

    m2 = 0xffffffff << (n % 32);
    m3 = 0xffffffff >> (32 - (n % 32));

    for (i = 1; i <= nNumUserRecords; i++) {
      read_qscn(i, pTempQScan, true);

      pTempQScan_n[n / 32] = (pTempQScan_n[n / 32] & m3) | ((pTempQScan_n[n / 32] >> 1) & m2) |
                             (pTempQScan_n[(n / 32) + 1] << 31);

      for (i1 = (n / 32) + 1; i1 <= (GetSession()->num_dirs / 32); i1++) {
        pTempQScan_n[i1] = (pTempQScan_n[i1] >> 1) | (pTempQScan_n[i1 + 1] << 31);
      }

      write_qscn(i, pTempQScan, true);
    }
    close_qscn();
    free(pTempQScan);
  }
}


void dlboardedit() {
  int i, i1, i2, confchg = 0;
  char s[81], s1[81], ch;
  SUBCONF_TYPE iconv;

  if (!ValidateSysopPassword()) {
    return;
  }
  showdirs();
  bool done = false;
  do {
    bout.nl();
    bout << "|#7(Q=Quit) (D)elete, (I)nsert, (M)odify, (S)wapDirs : ";
    ch = onek("QSDIM?");
    switch (ch) {
    case '?':
      showdirs();
      break;
    case 'Q':
      done = true;
      break;
    case 'M':
      bout.nl();
      bout << "|#2Dir number? ";
      input(s, 4);
      i = atoi(s);
      if ((s[0] != 0) && (i >= 0) && (i < GetSession()->num_dirs)) {
        modify_dir(i);
      }
      break;
    case 'S':
      if (GetSession()->num_dirs < GetSession()->GetMaxNumberFileAreas()) {
        bout.nl();
        bout << "|#2Take dir number? ";
        input(s, 4);
        i1 = atoi(s);
        if (!s[0] || i1 < 0 || i1 >= GetSession()->num_dirs) {
          break;
        }
        bout.nl();
        bout << "|#2And put before dir number? ";
        input(s, 4);
        i2 = atoi(s);
        if ((!s[0]) || (i2 < 0) || (i2 % 32 == 0) || (i2 > GetSession()->num_dirs) || (i1 == i2)) {
          break;
        }
        bout.nl();
        if (i2 < i1) {
          i1++;
        }
        write_qscn(GetSession()->usernum, qsc, true);
        bout << "|#1Moving dir now...Please wait...";
        insert_dir(i2);
        swap_dirs(i1, i2);
        delete_dir(i1);
        confchg = 1;
        showdirs();
      } else {
        bout << "\r\n|#6You must increase the number of dirs in INIT.EXE first.\r\n";
      }
      break;
    case 'I':
      if (GetSession()->num_dirs < GetSession()->GetMaxNumberFileAreas()) {
        bout.nl();
        bout << "|#2Insert before which dir? ";
        input(s, 4);
        i = atoi(s);
        if ((s[0] != 0) && (i >= 0) && (i <= GetSession()->num_dirs)) {
          insert_dir(i);
          modify_dir(i);
          confchg = 1;
          if (dirconfnum > 1) {
            bout.nl();
            list_confs(CONF_DIRS, 0);
            i2 = select_conf("Put in which conference? ", CONF_DIRS, 0);
            if (i2 >= 0) {
              if (in_conference(i, &dirconfs[i2]) < 0) {
                iconv = (SUBCONF_TYPE) i;
                addsubconf(CONF_DIRS, &dirconfs[i2], &iconv);
                i = static_cast<int>(iconv);
              }
            }
          } else {
            if (in_conference(i, &dirconfs[0]) < 0) {
              iconv = (SUBCONF_TYPE) i;
              addsubconf(CONF_DIRS, &dirconfs[0], &iconv);
              i = static_cast<int>(iconv);
            }
          }
        }
      }
      break;
    case 'D':
      bout.nl();
      bout << "|#2Delete which dir? ";
      input(s, 4);
      i = atoi(s);
      if ((s[0] != 0) && (i >= 0) && (i < GetSession()->num_dirs)) {
        bout.nl();
        bout << "|#5Delete " << directories[i].name << "? ";
        if (yesno()) {
          strcpy(s, directories[i].filename);
          delete_dir(i);
          confchg = 1;
          bout.nl();
          bout << "|#5Delete data files (.DIR/.EXT) for dir also? ";
          if (yesno()) {
            sprintf(s1, "%s%s.dir", syscfg.datadir, s);
            WFile::Remove(s1);
            sprintf(s1, "%s%s.ext", syscfg.datadir, s);
            WFile::Remove(s1);
          }
        }
      }
      break;
    }
  } while (!done && !hangup);
  WFile dirsFile(syscfg.datadir, DIRS_DAT);
  bool bDirsOpen = dirsFile.Open(WFile::modeReadWrite | WFile::modeCreateFile | WFile::modeBinary | WFile::modeTruncate);
  if (!bDirsOpen) {
    sysoplog("!!! Unable to open DIRS.DAT for writing, some changes may have been lost", false);
  } else {
    dirsFile.Write(directories, sizeof(directoryrec) * GetSession()->num_dirs);
    dirsFile.Close();
  }
  if (confchg) {
    save_confs(CONF_DIRS, -1, nullptr);
  }
  if (!GetApplication()->GetWfcStatus()) {
    changedsl();
  }
}



