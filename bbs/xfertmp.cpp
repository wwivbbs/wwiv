/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include <functional>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include "bbs/batch.h"
#include "bbs/bbsovl3.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "core/wfndfile.h"
#include "bbs/printfile.h"
#include "bbs/xfer_common.h"
#include "sdk/filenames.h"

// the archive type to use
#define ARC_NUMBER 0

using std::function;
using std::string;
using std::vector;
using namespace wwiv::strings;

bool bad_filename(const char *pszFileName) {
  // strings not to allow in a .zip file to extract from
  static const vector<string> bad_words = {
      "COMMAND",
      "..",
      "CMD"
      "4DOS"
      "4OS2"
      "4NT"
      "PKZIP",
      "PKUNZIP",
      "ARJ",
      "RAR",
      "LHA",
      "LHARC",
      "PKPAK",
    };

  for (const auto& bad_word : bad_words) {
    if (strstr(pszFileName, bad_word.c_str())) {
      bout << "Can't extract from that because it has " << pszFileName << wwiv::endl;
      return true;
    }
  }
  if (!okfn(pszFileName)) {
    bout << "Can't extract from that because it has " << pszFileName << wwiv::endl;
    return true;
  }
  return false;
}

struct arch {
  unsigned char type;
  char name[13];
  int32_t len;
  int16_t date, time, crc;
  int32_t size;
};

int check_for_files_arc(const char *pszFileName) {
  File file(pszFileName);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    arch a;
    long lFileSize = file.GetLength();
    long lFilePos = 1;
    file.Seek(0, File::seekBegin);
    file.Read(&a, 1);
    if (a.type != 26) {
      file.Close();
      bout << stripfn(pszFileName) << " is not a valid .ARC file.";
      return 1;
    }
    while (lFilePos < lFileSize) {
      file.Seek(lFilePos, File::seekBegin);
      int nNumRead = file.Read(&a, sizeof(arch));
      if (nNumRead == sizeof(arch)) {
        lFilePos += sizeof(arch);
        if (a.type == 1) {
          lFilePos -= 4;
          a.size = a.len;
        }
        if (a.type) {
          lFilePos += a.len;
          ++lFilePos;
          char szArcFileName[ MAX_PATH ];
          strncpy(szArcFileName, a.name, 13);
          szArcFileName[13] = 0;
          strupr(szArcFileName);
          if (bad_filename(szArcFileName)) {
            file.Close();
            return 1;
          }
        } else {
          lFilePos = lFileSize;
        }
      } else {
        file.Close();
        if (a.type != 0) {
          bout << stripfn(pszFileName) << " is not a valid .ARC file.";
          return 1;
        } else {
          lFilePos = lFileSize;
        }
      }
    }

    file.Close();
    return 0;
  }
  bout << "File not found: " << stripfn(pszFileName) << wwiv::endl;
  return 1;
}

// .ZIP structures and defines
#define ZIP_LOCAL_SIG 0x04034b50
#define ZIP_CENT_START_SIG 0x02014b50
#define ZIP_CENT_END_SIG 0x06054b50

struct zip_local_header {
  uint32_t   signature;                  // 0x04034b50
  uint16_t  extract_ver;
  uint16_t  flags;
  uint16_t  comp_meth;
  uint16_t  mod_time;
  uint16_t  mod_date;
  uint32_t   crc_32;
  uint32_t   comp_size;
  uint32_t   uncomp_size;
  uint16_t  filename_len;
  uint16_t  extra_length;
};

struct zip_central_dir {
  uint32_t   signature;                  // 0x02014b50
  uint16_t  made_ver;
  uint16_t  extract_ver;
  uint16_t  flags;
  uint16_t  comp_meth;
  uint16_t  mod_time;
  uint16_t  mod_date;
  uint32_t   crc_32;
  uint32_t   comp_size;
  uint32_t   uncomp_size;
  uint16_t  filename_len;
  uint16_t  extra_len;
  uint16_t  comment_len;
  uint16_t  disk_start;
  uint16_t  int_attr;
  uint32_t   ext_attr;
  uint32_t   rel_ofs_header;
};

struct zip_end_dir {
  uint32_t   signature;                  // 0x06054b50
  uint16_t  disk_num;
  uint16_t  cent_dir_disk_num;
  uint16_t  total_entries_this_disk;
  uint16_t  total_entries_total;
  uint32_t   central_dir_size;
  uint32_t   ofs_cent_dir;
  uint16_t  comment_len;
};

int check_for_files_zip(const char *pszFileName) {
  zip_local_header zl;
  zip_central_dir zc;
  zip_end_dir ze;
  char s[MAX_PATH];

#define READ_FN( ln ) { file.Read( s, ln ); s[ ln ] = '\0'; }

  File file(pszFileName);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    long l = 0;
    long len = file.GetLength();
    while (l < len) {
      long sig = 0;
      file.Seek(l, File::seekBegin);
      file.Read(&sig, 4);
      file.Seek(l, File::seekBegin);
      switch (sig) {
      case ZIP_LOCAL_SIG:
        file.Read(&zl, sizeof(zl));
        READ_FN(zl.filename_len);
        strupr(s);
        if (bad_filename(s)) {
          file.Close();
          return 1;
        }
        l += sizeof(zl);
        l += zl.comp_size + zl.filename_len + zl.extra_length;
        break;
      case ZIP_CENT_START_SIG:
        file.Read(&zc, sizeof(zc));
        READ_FN(zc.filename_len);
        strupr(s);
        if (bad_filename(s)) {
          file.Close();
          return 1;
        }
        l += sizeof(zc);
        l += zc.filename_len + zc.extra_len;
        break;
      case ZIP_CENT_END_SIG:
        file.Read(&ze, sizeof(ze));
        file.Close();
        return 0;
      default:
        file.Close();
        bout << "Error examining that; can't extract from it.\r\n";
        return 1;
      }
    }
    file.Close();
    return 0;
  }
  bout << "File not found: " << stripfn(pszFileName) << wwiv::endl;
  return 1;
}


struct lharc_header {
  unsigned char     checksum;
  char              ctype[5];
  long              comp_size;
  long              uncomp_size;
  unsigned short    time;
  unsigned short    date;
  unsigned short    attr;
  unsigned char     fn_len;
};

int check_for_files_lzh(const char *pszFileName) {
  lharc_header a;

  File file(pszFileName);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "File not found: " << stripfn(pszFileName) << wwiv::endl;
    return 1;
  }
  long lFileSize = file.GetLength();
  unsigned short nCrc;
  int err = 0;
  for (long l = 0; l < lFileSize;
       l += a.fn_len + a.comp_size + sizeof(lharc_header) + file.Read(&nCrc, sizeof(nCrc)) + 1) {
    file.Seek(l, File::seekBegin);
    char flag;
    file.Read(&flag, 1);
    if (!flag) {
      l = lFileSize;
      break;
    }
    int nNumRead = file.Read(&a, sizeof(lharc_header));
    if (nNumRead != sizeof(lharc_header)) {
      bout << stripfn(pszFileName) << " is not a valid .LZH file.";
      err = 1;
      break;
    }
    char szBuffer[256];
    nNumRead = file.Read(szBuffer, a.fn_len);
    if (nNumRead != a.fn_len) {
      bout << stripfn(pszFileName) << " is not a valid .LZH file.";
      err = 1;
      break;
    }
    szBuffer[a.fn_len] = '\0';
    strupr(szBuffer);
    if (bad_filename(szBuffer)) {
      err = 1;
      break;
    }
  }
  file.Close();
  return err;
}

int check_for_files_arj(const char *pszFileName) {
  File file(pszFileName);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    long lFileSize = file.GetLength();
    long lCurPos = 0;
    file.Seek(0L, File::seekBegin);
    while (lCurPos < lFileSize) {
      file.Seek(lCurPos, File::seekBegin);
      unsigned short sh;
      int nNumRead = file.Read(&sh, 2);
      if (nNumRead != 2 || sh != 0xea60) {
        file.Close();
        bout << stripfn(pszFileName) << " is not a valid .ARJ file.";
        return 1;
      }
      lCurPos += nNumRead + 2;
      file.Read(&sh, 2);
      unsigned char s1;
      file.Read(&s1, 1);
      file.Seek(lCurPos + 12, File::seekBegin);
      long l2;
      file.Read(&l2, 4);
      file.Seek(lCurPos + static_cast<long>(s1), File::seekBegin);
      char szBuffer[256];
      file.Read(szBuffer, 250);
      szBuffer[250] = '\0';
      if (strlen(szBuffer) > 240) {
        file.Close();
        bout << stripfn(pszFileName) << " is not a valid .ARJ file.";
        return 1;
      }
      lCurPos += 4 + static_cast<long>(sh);
      file.Seek(lCurPos, File::seekBegin);
      file.Read(&sh, 2);
      lCurPos += 2;
      while ((lCurPos < lFileSize) && sh) {
        lCurPos += 6 + static_cast<long>(sh);
        file.Seek(lCurPos - 2, File::seekBegin);
        file.Read(&sh, 2);
      }
      lCurPos += l2;
      strupr(szBuffer);
      if (bad_filename(szBuffer)) {
        file.Close();
        return 1;
      }
    }

    file.Close();
    return 0;
  }

  bout << "File not found: " << stripfn(pszFileName);
  return 1;
}

struct arc_testers {
  const char *  arc_name;
  function<int(const char*)> func;
};

static vector<arc_testers> arc_t = {
  { "ZIP", check_for_files_zip },
  { "ARC", check_for_files_arc },
  { "LZH", check_for_files_lzh },
  { "ARJ", check_for_files_arj },
};

static bool check_for_files(const char *pszFileName) {
  const char * ss = strrchr(pszFileName, '.');
  if (ss) {
    ss++;
    for (const auto& t : arc_t) {
      if (wwiv::strings::IsEqualsIgnoreCase(ss, t.arc_name)) {
        return t.func(pszFileName) == 0;
      }
    }
  } else {
    // no extension?
    bout << "No extension.\r\n";
    return 1;
  }
  return 0;
}

bool download_temp_arc(const char *pszFileName, bool count_against_xfer_ratio) {
  bout << "Downloading " << pszFileName << "." << arcs[ARC_NUMBER].extension << ":\r\n\r\n";
  if (count_against_xfer_ratio && !ratio_ok()) {
    bout << "Ratio too low.\r\n";
    return false;
  }
  char szDownloadFileName[ MAX_PATH ];
  sprintf(szDownloadFileName, "%s%s.%s", syscfgovr.tempdir, pszFileName, arcs[ARC_NUMBER].extension);
  File file(szDownloadFileName);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "No such file.\r\n\n";
    return false;
  }
  long lFileSize = file.GetLength();
  file.Close();
  if (lFileSize == 0L) {
    bout << "File has nothing in it.\r\n\n";
    return false;
  }
  double d = XFER_TIME(lFileSize);
  if (d <= nsl()) {
    bout << "Approx. time: " << ctim(d) << wwiv::endl;
    bool sent = false;
    bool abort = false;
    char szFileToSend[81];
    sprintf(szFileToSend, "%s.%s", pszFileName, arcs[ARC_NUMBER].extension);
    send_file(szDownloadFileName, &sent, &abort, szFileToSend, -1, lFileSize);
    if (sent) {
      if (count_against_xfer_ratio) {
        session()->user()->SetFilesDownloaded(session()->user()->GetFilesDownloaded() + 1);
        session()->user()->SetDownloadK(session()->user()->GetDownloadK() + bytes_to_k(lFileSize));
        bout.nl(2);
        bout.bprintf("Your ratio is now: %-6.3f\r\n", ratio());
      }
      sysoplogf("Downloaded %ldk of \"%s\"", bytes_to_k(lFileSize), szFileToSend);
      if (session()->IsUserOnline()) {
        session()->UpdateTopScreen();
      }
      return true;
    }
  } else {
    bout.nl(2);
    bout << "Not enough time left to D/L.\r\n\n";
  }
  return false;
}

void add_arc(const char *arc, const char *pszFileName, int dos) {
  char szAddArchiveCommand[ MAX_PATH ], szArchiveFileName[ MAX_PATH ];

  sprintf(szArchiveFileName, "%s.%s", arc, arcs[ARC_NUMBER].extension);
  // TODO - This logic is still broken since chain.* and door.* won't match
  if (wwiv::strings::IsEqualsIgnoreCase(pszFileName, "chain.txt") ||
      wwiv::strings::IsEqualsIgnoreCase(pszFileName, "door.sys") ||
      wwiv::strings::IsEqualsIgnoreCase(pszFileName, "chain.*")  ||
      wwiv::strings::IsEqualsIgnoreCase(pszFileName, "door.*")) {
    return;
  }

  get_arc_cmd(szAddArchiveCommand, szArchiveFileName, 2, pszFileName);
  if (szAddArchiveCommand[0]) {
    chdir(syscfgovr.tempdir);
    session()->localIO()->LocalPuts(szAddArchiveCommand);
    session()->localIO()->LocalPuts("\r\n");
    if (dos) {
      ExecuteExternalProgram(szAddArchiveCommand, session()->GetSpawnOptions(SPAWNOPT_ARCH_A));
    } else {
      ExecuteExternalProgram(szAddArchiveCommand, EFLAG_NONE);
      session()->UpdateTopScreen();
    }
    session()->CdHome();
    sysoplogf("Added \"%s\" to %s", pszFileName, szArchiveFileName);

  } else {
    bout << "Sorry, can't add to temp archive.\r\n\n";
  }
}

void add_temp_arc() {
  char szInputFileMask[ MAX_PATH ], szFileMask[ MAX_PATH];

  bout.nl();
  bout << "|#7Enter filename to add to temporary archive file.  May contain wildcards.\r\n|#7:";
  input(szInputFileMask, 12);
  if (!okfn(szInputFileMask)) {
    return;
  }
  if (szInputFileMask[0] == '\0') {
    return;
  }
  if (strchr(szInputFileMask, '.') == nullptr) {
    strcat(szInputFileMask, ".*");
  }
  strcpy(szFileMask, stripfn(szInputFileMask));
  for (int i = 0; i < wwiv::strings::GetStringLength(szFileMask); i++) {
    if (szFileMask[i] == '|' || szFileMask[i] == '>' ||
        szFileMask[i] == '<' || szFileMask[i] == ';' ||
        szFileMask[i] == ' ' || szFileMask[i] == ':' ||
        szFileMask[i] == '/' || szFileMask[i] == '\\') {
      return;
    }
  }
  add_arc("temp", szFileMask, 1);
}

void del_temp() {
  char szFileName[MAX_PATH];

  bout.nl();
  bout << "|#9Enter file name to delete: ";
  input(szFileName, 12, true);
  if (!okfn(szFileName)) {
    return;
  }
  if (szFileName[0]) {
    if (strchr(szFileName, '.') == nullptr) {
      strcat(szFileName, ".*");
    }
    remove_from_temp(szFileName, syscfgovr.tempdir, true);
  }
}

void list_temp_dir() {
  char szFileMask[ MAX_PATH ];

  sprintf(szFileMask, "%s*.*", syscfgovr.tempdir);
  WFindFile fnd;
  bool bFound = fnd.open(szFileMask, 0);
  bout.nl();
  bout << "Files in temporary directory:\r\n\n";
  int i1 = 0;
  bool abort = false;
  while (bFound && !hangup && !abort) {
    char szFileName[ MAX_PATH ];
    strcpy(szFileName, fnd.GetFileName());

    if (!wwiv::strings::IsEqualsIgnoreCase(szFileName, "chain.txt") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "door.sys")) {
      align(szFileName);
      char szBuffer[ 255 ];
      sprintf(szBuffer, "%12s  %-8ld", szFileName, fnd.GetFileSize());
      pla(szBuffer, &abort);
      i1 = 1;
    }
    bFound = fnd.next();
  }
  if (!i1) {
    bout << "None.\r\n";
  }
  bout.nl();
  if (!abort && !hangup) {
    bout << "Free space: " << freek1(syscfgovr.tempdir) << wwiv::endl;
    bout.nl();
  }
}


void temp_extract() {
  int i, i1, i2, ot;
  char s[255], s1[255], s2[81], s3[255];
  uploadsrec u;

  dliscan();
  bout.nl();
  bout << "Extract to temporary directory:\r\n\n";
  bout << "|#2Filename: ";
  input(s, 12);
  if (!okfn(s) || s[0] == '\0') {
    return;
  }
  if (strchr(s, '.') == nullptr) {
    strcat(s, ".*");
  }
  align(s);
  i = recno(s);
  bool ok = true;
  while ((i > 0) && ok && !hangup) {
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    sprintf(s2, "%s%s", directories[udir[session()->GetCurrentFileArea()].subnum].path, u.filename);
    StringRemoveWhitespace(s2);
    if (directories[udir[session()->GetCurrentFileArea()].subnum].mask & mask_cdrom) {
      sprintf(s1, "%s%s", directories[udir[session()->GetCurrentFileArea()].subnum].path, u.filename);
      sprintf(s2, "%s%s", syscfgovr.tempdir, u.filename);
      StringRemoveWhitespace(s1);
      if (!File::Exists(s2)) {
        copyfile(s1, s2, false);
      }
    }
    get_arc_cmd(s1, s2, 1, "");
    if (s1[0] && File::Exists(s2)) {
      bout.nl(2);
      bool abort = false;
      ot = session()->tagging;
      session()->tagging = 2;
      printinfo(&u, &abort);
      session()->tagging = ot;
      bout.nl();
      if (directories[udir[session()->GetCurrentFileArea()].subnum].mask & mask_cdrom) {
        chdir(syscfgovr.tempdir);
      } else {
        chdir(directories[udir[session()->GetCurrentFileArea()].subnum].path);
      }
      File file(File::current_directory(), stripfn(u.filename));
      session()->CdHome();
      if (check_for_files(file.full_pathname().c_str())) {
        bool ok1 = false;
        do {
          bout << "|#2Extract what (?=list,Q=abort) ? ";
          input(s1, 12);
          ok1 = (s1[0] == '\0') ? false : true;
          if (!okfn(s1)) {
            ok1 = false;
          }
          if (wwiv::strings::IsEquals(s1, "?")) {
            list_arc_out(stripfn(u.filename), directories[udir[session()->GetCurrentFileArea()].subnum].path);
            s1[0] = '\0';
          }
          if (wwiv::strings::IsEquals(s1, "Q")) {
            ok = false;
            s1[0] = '\0';
          }
          i2 = 0;
          for (i1 = 0; i1 < wwiv::strings::GetStringLength(s1); i1++) {
            if ((s1[i1] == '|') || (s1[i1] == '>') || (s1[i1] == '<') || (s1[i1] == ';') || (s1[i1] == ' ')) {
              i2 = 1;
            }
          }
          if (i2) {
            s1[0] = '\0';
          }
          if (s1[0]) {
            if (strchr(s1, '.') == nullptr) {
              strcat(s1, ".*");
            }
            get_arc_cmd(s3, file.full_pathname().c_str(), 1, stripfn(s1));
            chdir(syscfgovr.tempdir);
            if (!okfn(s1)) {
              s3[0] = '\0';
            }
            if (s3[0]) {
              ExecuteExternalProgram(s3, session()->GetSpawnOptions(SPAWNOPT_ARCH_E));
              sprintf(s2, "Extracted out \"%s\" from \"%s\"", s1, u.filename);
            } else {
              s2[0] = '\0';
            }
            session()->CdHome();
            if (s2[0]) {
              sysoplog(s2);
            }
          }
        } while (!hangup && ok && ok1);
      }
    } else if (s1[0]) {
      bout.nl();
      bout << "That file currently isn't there.\r\n\n";
    }
    if (ok) {
      i = nrecno(s, i);
    }
  }
}


void list_temp_text() {
  char s[81], s1[MAX_PATH];
  double percent;
  char szFileName[MAX_PATH];

  bout.nl();
  bout << "|#2List what file(s) : ";
  input(s, 12, true);
  if (!okfn(s)) {
    return;
  }
  if (s[0]) {
    if (strchr(s, '.') == nullptr) {
      strcat(s, ".*");
    }
    sprintf(s1, "%s%s", syscfgovr.tempdir, stripfn(s));
    WFindFile fnd;
    bool bFound = fnd.open(s1, 0);
    int ok = 1;
    bout.nl();
    while (bFound && ok) {
      strcpy(szFileName, fnd.GetFileName());
      sprintf(s, "%s%s", syscfgovr.tempdir, szFileName);
      if (!wwiv::strings::IsEqualsIgnoreCase(szFileName, "chain.txt") &&
          !wwiv::strings::IsEqualsIgnoreCase(szFileName, "door.sys")) {
        bout.nl();
        bout << "Listing " << szFileName << wwiv::endl;
        bout.nl();
        bool sent;
        ascii_send(s, &sent, &percent);
        if (sent) {
          sysoplogf("Temp text D/L \"%s\"", szFileName);
        } else {
          sysoplogf("Temp Tried text D/L \"%s\" %3.2f%%", szFileName, percent * 100.0);
          ok = 0;
        }
      }
      bFound = fnd.next();
    }
  }
}


void list_temp_arc() {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "temp.%s", arcs[ARC_NUMBER].extension);
  list_arc_out(szFileName, syscfgovr.tempdir);
  bout.nl();
}


void temporary_stuff() {
  printfile(TARCHIVE_NOEXT);
  do {
    bout.nl();
    bout << "|#9Archive: Q,D,R,A,V,L,T: ";
    char ch = onek("Q?DRAVLT");
    switch (ch) {
    case 'Q':
      return;
      break;
    case 'D':
      download_temp_arc("temp", true);
      break;
    case 'V':
      list_temp_arc();
      break;
    case 'A':
      add_temp_arc();
      break;
    case 'R':
      del_temp();
      break;
    case 'L':
      list_temp_dir();
      break;
    case 'T':
      list_temp_text();
      break;
    case '?':
      printfile(TARCHIVE_NOEXT);
      break;
    }
  } while (!hangup);
}



void move_file_t() {
  char s1[81], s2[81];
  int d1 = -1;
  uploadsrec u, u1;

  tmp_disable_conf(true);

  bout.nl();
  if (session()->numbatch == 0) {
    bout.nl();
    bout << "|#6No files have been tagged for movement.\r\n";
    pausescr();
  }
  for (int nCurBatchPos = session()->numbatch - 1; nCurBatchPos >= 0; nCurBatchPos--) {
    bool ok = false;
    char szCurBatchFileName[ MAX_PATH ];
    strcpy(szCurBatchFileName, batch[nCurBatchPos].filename);
    align(szCurBatchFileName);
    dliscan1(batch[nCurBatchPos].dir);
    int nTempRecordNum = recno(szCurBatchFileName);
    if (nTempRecordNum < 0) {
      bout << "File not found.\r\n";
      pausescr();
    }
    bool done = false;
    int nCurPos = 0;
    while (!hangup && (nTempRecordNum > 0) && !done) {
      nCurPos = nTempRecordNum;
      File fileDownload(g_szDownloadFileName);
      fileDownload.Open(File::modeReadOnly | File::modeBinary);
      FileAreaSetRecord(fileDownload, nTempRecordNum);
      fileDownload.Read(&u, sizeof(uploadsrec));
      fileDownload.Close();
      printfileinfo(&u, batch[nCurBatchPos].dir);
      bout << "|#5Move this (Y/N/Q)? ";
      char ch = ynq();
      if (ch == 'Q') {
        done = true;
        tmp_disable_conf(false);
        dliscan();
        return;
      } else if (ch == 'Y') {
        sprintf(s1, "%s%s", directories[batch[nCurBatchPos].dir].path, u.filename);
        StringRemoveWhitespace(s1);
        char *pszDirectoryNum = nullptr;
        do {
          bout << "|#2To which directory? ";
          pszDirectoryNum = mmkey(1);
          if (pszDirectoryNum[0] == '?') {
            dirlist(1);
            dliscan1(batch[nCurBatchPos].dir);
          }
        } while (!hangup && (pszDirectoryNum[0] == '?'));
        d1 = -1;
        if (pszDirectoryNum[0]) {
          for (int i1 = 0; (i1 < session()->num_dirs) && (udir[i1].subnum != -1); i1++) {
            if (wwiv::strings::IsEquals(udir[i1].keys, pszDirectoryNum)) {
              d1 = i1;
            }
          }
        }
        if (d1 != -1) {
          ok = true;
          d1 = udir[d1].subnum;
          dliscan1(d1);
          if (recno(u.filename) > 0) {
            ok = false;
            bout << "Filename already in use in that directory.\r\n";
          }
          if (session()->numf >= directories[d1].maxfiles) {
            ok = false;
            bout << "Too many files in that directory.\r\n";
          }
          if (freek1(directories[d1].path) < static_cast<long>(u.numbytes / 1024L) + 3) {
            ok = false;
            bout << "Not enough disk space to move it.\r\n";
          }
          dliscan();
        } else {
          ok = false;
        }
      } else {
        ok = false;
      }
      if (ok && !done) {
        bout << "|#5Reset upload time for file? ";
        if (yesno()) {
          u.daten = static_cast<unsigned long>(time(nullptr));
        }
        --nCurPos;
        fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        for (int i1 = nTempRecordNum; i1 < session()->numf; i1++) {
          FileAreaSetRecord(fileDownload, i1 + 1);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i1);
          fileDownload.Write(&u1, sizeof(uploadsrec));
        }
        --session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        u1.numbytes = session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u1, sizeof(uploadsrec));
        fileDownload.Close();
        char *pszExtendedDesc = read_extended_description(u.filename);
        if (pszExtendedDesc) {
          delete_extended_description(u.filename);
        }

        sprintf(s2, "%s%s", directories[d1].path, u.filename);
        StringRemoveWhitespace(s2);
        dliscan1(d1);
        fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        for (int i = session()->numf; i >= 1; i--) {
          FileAreaSetRecord(fileDownload, i);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i + 1);
          fileDownload.Write(&u1, sizeof(uploadsrec));
        }
        FileAreaSetRecord(fileDownload, 1);
        fileDownload.Write(&u, sizeof(uploadsrec));
        ++session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        u1.numbytes = session()->numf;
        if (u.daten > u1.daten) {
          u1.daten = u.daten;
        }
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u1, sizeof(uploadsrec));
        fileDownload.Close();
        if (pszExtendedDesc) {
          add_extended_description(u.filename, pszExtendedDesc);
          free(pszExtendedDesc);
        }
        StringRemoveWhitespace(s1);
        StringRemoveWhitespace(s2);
        if (!wwiv::strings::IsEquals(s1, s2) && File::Exists(s1)) {
          bool bSameDrive = false;
          if (s1[1] != ':' && s2[1] != ':') {
            bSameDrive = true;
          }
          if (s1[1] == ':' && s2[1] == ':' && s1[0] == s2[0]) {
            bSameDrive = true;
          }
          if (bSameDrive) {
            File::Rename(s1, s2);
            if (File::Exists(s2)) {
              File::Remove(s1);
            } else {
              copyfile(s1, s2, false);
              File::Remove(s1);
            }
          } else {
            copyfile(s1, s2, false);
            File::Remove(s1);
          }
          remlist(batch[nCurBatchPos].filename);
          didnt_upload(nCurBatchPos);
          delbatch(nCurBatchPos);
        }
        bout << "File moved.\r\n";
      }
      dliscan();
      nTempRecordNum = nrecno(szCurBatchFileName, nCurPos);
    }
  }
  tmp_disable_conf(false);
}


void removefile() {
  uploadsrec u;
  WUser uu;

  dliscan();
  bout.nl();
  bout << "|#9Enter filename to remove.\r\n:";
  char szFileToRemove[ MAX_PATH ];
  input(szFileToRemove, 12, true);
  if (szFileToRemove[0] == '\0') {
    return;
  }
  if (strchr(szFileToRemove, '.') == nullptr) {
    strcat(szFileToRemove, ".*");
  }
  align(szFileToRemove);
  int i = recno(szFileToRemove);
  bool abort = false;
  while (!hangup && (i > 0) && !abort) {
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    if ((dcs()) || ((u.ownersys == 0) && (u.ownerusr == session()->usernum))) {
      bout.nl();
      if (check_batch_queue(u.filename)) {
        bout << "|#6That file is in the batch queue; remove it from there.\r\n\n";
      } else {
        printfileinfo(&u, udir[session()->GetCurrentFileArea()].subnum);
        bout << "|#9Remove (|#2Y/N/Q|#9) |#0: |#2";
        char ch = ynq();
        if (ch == 'Q') {
          abort = true;
        } else if (ch == 'Y') {
          bool bRemoveDlPoints = true;
          bool bDeleteFileToo = false;
          if (dcs()) {
            bout << "|#5Delete file too? ";
            bDeleteFileToo = yesno();
            if (bDeleteFileToo && (u.ownersys == 0)) {
              bout << "|#5Remove DL points? ";
              bRemoveDlPoints = yesno();
            }
            if (session()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
              bout.nl();
              bout << "|#5Remove from ALLOW.DAT? ";
              if (yesno()) {
                modify_database(u.filename, false);
              }
            }
          } else {
            bDeleteFileToo = true;
            modify_database(u.filename, false);
          }
          if (bDeleteFileToo) {
            char szFileNameToDelete[ MAX_PATH ];
            sprintf(szFileNameToDelete, "%s%s", directories[udir[session()->GetCurrentFileArea()].subnum].path, u.filename);
            StringRemoveWhitespace(szFileNameToDelete);
            File::Remove(szFileNameToDelete);
            if (bRemoveDlPoints && u.ownersys == 0) {
              session()->users()->ReadUser(&uu, u.ownerusr);
              if (!uu.IsUserDeleted()) {
                if (date_to_daten(uu.GetFirstOn()) < static_cast<time_t>(u.daten)) {
                  uu.SetFilesUploaded(uu.GetFilesUploaded() - 1);
                  uu.SetUploadK(uu.GetUploadK() - bytes_to_k(u.numbytes));
                  session()->users()->WriteUser(&uu, u.ownerusr);
                }
              }
            }
          }
          if (u.mask & mask_extended) {
            delete_extended_description(u.filename);
          }
          sysoplogf("- \"%s\" removed off of %s", u.filename, directories[udir[session()->GetCurrentFileArea()].subnum].name);
          fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
          for (int i1 = i; i1 < session()->numf; i1++) {
            FileAreaSetRecord(fileDownload, i1 + 1);
            fileDownload.Read(&u, sizeof(uploadsrec));
            FileAreaSetRecord(fileDownload, i1);
            fileDownload.Write(&u, sizeof(uploadsrec));
          }
          --i;
          --session()->numf;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Read(&u, sizeof(uploadsrec));
          u.numbytes = session()->numf;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Write(&u, sizeof(uploadsrec));
          fileDownload.Close();
        }
      }
    }
    i = nrecno(szFileToRemove, i);
  }
}
