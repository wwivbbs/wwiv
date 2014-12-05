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
#include "core/wfndfile.h"
#include "core/textfile.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"

extern char str_quit[];

//
// private functions
//
int  comparedl(uploadsrec * x, uploadsrec * y, int type);
void quicksort(int l, int r, int type);
bool upload_file(const char *pszFileName, int nDirectoryNum, const char *pszDescription);
long db_index(File &fileAllow, const char *pszFileName);
void l_config_nscan();
void config_nscan();


void move_file() {
  char s1[81], s2[81];
  int d1 = 0, d2 = 0;
  uploadsrec u, u1;

  bool ok = false;
  bout.nl(2);
  bout << "|#2Filename to move: ";
  char szFileMask[ MAX_PATH ];
  input(szFileMask, 12);
  if (strchr(szFileMask, '.') == nullptr) {
    strcat(szFileMask, ".*");
  }
  align(szFileMask);
  dliscan();
  int nCurRecNum = recno(szFileMask);
  if (nCurRecNum < 0) {
    bout << "\r\nFile not found.\r\n";
    return;
  }
  bool done = false;

  tmp_disable_conf(true);

  while (!hangup && nCurRecNum > 0 && !done) {
    int nCurrentPos = nCurRecNum;
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, nCurRecNum);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();
    printfileinfo(&u, udir[session()->GetCurrentFileArea()].subnum);
    bout.nl();
    bout << "|#5Move this (Y/N/Q)? ";
    char ch = ynq();
    if (ch == 'Q') {
      done = true;
    } else if (ch == 'Y') {
      sprintf(s1, "%s%s", directories[udir[session()->GetCurrentFileArea()].subnum].path, u.filename);
      char *ss = nullptr;
      do {
        bout.nl(2);
        bout << "|#2To which directory? ";
        ss = mmkey(1);
        if (ss[0] == '?') {
          dirlist(1);
          dliscan();
        }
      } while ((!hangup) && (ss[0] == '?'));
      d1 = -1;
      if (ss[0]) {
        for (int i1 = 0; (i1 < session()->num_dirs) && (udir[i1].subnum != -1); i1++) {
          if (wwiv::strings::IsEquals(udir[i1].keys, ss)) {
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
          bout << "\r\nFilename already in use in that directory.\r\n";
        }
        if (session()->numf >= directories[d1].maxfiles) {
          ok = false;
          bout << "\r\nToo many files in that directory.\r\n";
        }
        if (freek1(directories[d1].path) < ((double)(u.numbytes / 1024L) + 3)) {
          ok = false;
          bout << "\r\nNot enough disk space to move it.\r\n";
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
      --nCurrentPos;
      fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      for (int i1 = nCurRecNum; i1 < session()->numf; i1++) {
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
      char* ss = read_extended_description(u.filename);
      if (ss) {
        delete_extended_description(u.filename);
      }

      sprintf(s2, "%s%s", directories[d1].path, u.filename);
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
        session()->m_DirectoryDateCache[d1] = u.daten;
      }
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Write(&u1, sizeof(uploadsrec));
      fileDownload.Close();
      if (ss) {
        add_extended_description(u.filename, ss);
        free(ss);
      }
      StringRemoveWhitespace(s1);
      StringRemoveWhitespace(s2);
      if (!wwiv::strings::IsEquals(s1, s2) && File::Exists(s1)) {
        d2 = 0;
        if ((s1[1] != ':') && (s2[1] != ':')) {
          d2 = 1;
        }
        if ((s1[1] == ':') && (s2[1] == ':') && (s1[0] == s2[0])) {
          d2 = 1;
        }
        if (d2) {
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
      }
      bout << "\r\nFile moved.\r\n";
    }
    dliscan();
    nCurRecNum = nrecno(szFileMask, nCurrentPos);
  }

  tmp_disable_conf(false);
}


int comparedl(uploadsrec * x, uploadsrec * y, int type) {
  switch (type) {
  case 0:
    return wwiv::strings::StringCompare(x->filename, y->filename);
  case 1:
    if (x->daten < y->daten) {
      return -1;
    }
    return (x->daten > y->daten) ? 1 : 0;
  case 2:
    if (x->daten < y->daten) {
      return 1;
    }
    return ((x->daten > y->daten) ? -1 : 0);
  }
  return 0;
}


void quicksort(int l, int r, int type) {
  uploadsrec a, a2, x;

  int i = l;
  int j = r;
  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);

  FileAreaSetRecord(fileDownload, ((l + r) / 2));
  fileDownload.Read(&x, sizeof(uploadsrec));
  do {
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&a, sizeof(uploadsrec));
    while (comparedl(&a, &x, type) < 0) {
      FileAreaSetRecord(fileDownload, ++i);
      fileDownload.Read(&a, sizeof(uploadsrec));
    }
    FileAreaSetRecord(fileDownload, j);
    fileDownload.Read(&a2, sizeof(uploadsrec));
    while (comparedl(&a2, &x, type) > 0) {
      FileAreaSetRecord(fileDownload, --j);
      fileDownload.Read(&a2, sizeof(uploadsrec));
    }
    if (i <= j) {
      if (i != j) {
        FileAreaSetRecord(fileDownload, i);
        fileDownload.Write(&a2, sizeof(uploadsrec));
        FileAreaSetRecord(fileDownload, j);
        fileDownload.Write(&a, sizeof(uploadsrec));
      }
      i++;
      j--;
    }
  } while (i < j);
  fileDownload.Close();
  if (l < j) {
    quicksort(l, j, type);
  }
  if (i < r) {
    quicksort(i, r, type);
  }
}


void sortdir(int nDirectoryNum, int type) {
  dliscan1(nDirectoryNum);
  if (session()->numf > 1) {
    quicksort(1, session()->numf, type);
  }
}


void sort_all(int type) {
  tmp_disable_conf(true);
  for (int i = 0; (i < session()->num_dirs) && (udir[i].subnum != -1) &&
       (!session()->localIO()->LocalKeyPressed()); i++) {
    bout << "\r\n|#1Sorting " << directories[udir[i].subnum].name << wwiv::endl;
    sortdir(i, type);
  }
  tmp_disable_conf(false);
}


void rename_file() {
  char s[81], s1[81], s2[81], *ss, s3[81];
  uploadsrec u;

  bout.nl(2);
  bout << "|#2File to rename: ";
  input(s, 12);
  if (s[0] == 0) {
    return;
  }
  if (strchr(s, '.') == nullptr) {
    strcat(s, ".*");
  }
  align(s);
  dliscan();
  bout.nl();
  strcpy(s3, s);
  int nRecNum = recno(s);
  while (nRecNum > 0 && !hangup) {
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    int nCurRecNum = nRecNum;
    FileAreaSetRecord(fileDownload, nRecNum);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();
    printfileinfo(&u, udir[session()->GetCurrentFileArea()].subnum);
    bout.nl();
    bout << "|#5Change info for this file (Y/N/Q)? ";
    char ch = ynq();
    if (ch == 'Q') {
      break;
    } else if (ch == 'N') {
      nRecNum = nrecno(s3, nCurRecNum);
      continue;
    }
    bout.nl();
    bout << "|#2New filename? ";
    input(s, 12);
    if (!okfn(s)) {
      s[0] = 0;
    }
    if (s[0]) {
      align(s);
      if (!wwiv::strings::IsEquals(s, "        .   ")) {
        strcpy(s1, directories[udir[session()->GetCurrentFileArea()].subnum].path);
        strcpy(s2, s1);
        strcat(s1, s);
        StringRemoveWhitespace(s1);
        if (File::Exists(s1)) {
          bout << "Filename already in use; not changed.\r\n";
        } else {
          strcat(s2, u.filename);
          StringRemoveWhitespace(s2);
          File::Rename(s2, s1);
          if (File::Exists(s1)) {
            ss = read_extended_description(u.filename);
            if (ss) {
              delete_extended_description(u.filename);
              add_extended_description(s, ss);
              free(ss);
            }
            strcpy(u.filename, s);
          } else {
            bout << "Bad filename.\r\n";
          }
        }
      }
    }
    bout << "\r\nNew description:\r\n|#2: ";
    inputl(s, 58);
    if (s[0]) {
      strcpy(u.description, s);
    }
    ss = read_extended_description(u.filename);
    bout.nl(2);
    bout << "|#5Modify extended description? ";
    if (yesno()) {
      bout.nl();
      if (ss) {
        bout << "|#5Delete it? ";
        if (yesno()) {
          free(ss);
          delete_extended_description(u.filename);
          u.mask &= ~mask_extended;
        } else {
          u.mask |= mask_extended;
          modify_extended_description(&ss,
                                      directories[udir[session()->GetCurrentFileArea()].subnum].name, u.filename);
          if (ss) {
            delete_extended_description(u.filename);
            add_extended_description(u.filename, ss);
            free(ss);
          }
        }
      } else {
        modify_extended_description(&ss,
                                    directories[udir[session()->GetCurrentFileArea()].subnum].name, u.filename);
        if (ss) {
          add_extended_description(u.filename, ss);
          free(ss);
          u.mask |= mask_extended;
        } else {
          u.mask &= ~mask_extended;
        }
      }
    } else if (ss) {
      free(ss);
      u.mask |= mask_extended;
    } else {
      u.mask &= ~mask_extended;
    }
    fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    FileAreaSetRecord(fileDownload, nRecNum);
    fileDownload.Write(&u, sizeof(uploadsrec));
    fileDownload.Close();
    nRecNum = nrecno(s3, nCurRecNum);
  }
}


bool upload_file(const char *pszFileName, int nDirectoryNum, const char *pszDescription) {
  uploadsrec u, u1;

  directoryrec d = directories[nDirectoryNum];
  char szTempFileName[ 255 ];
  strcpy(szTempFileName, pszFileName);
  align(szTempFileName);
  strcpy(u.filename, szTempFileName);
  u.ownerusr = static_cast< unsigned short >(session()->usernum);
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  if (!(d.mask & mask_cdrom) && !check_ul_event(nDirectoryNum, &u)) {
    bout << pszFileName << " was deleted by upload event.\r\n";
  } else {
    char szUnalignedFileName[ MAX_PATH ];
    strcpy(szUnalignedFileName, szTempFileName);
    unalign(szUnalignedFileName);

    char szFullPathName[ MAX_PATH ];
    sprintf(szFullPathName, "%s%s", d.path, szUnalignedFileName);

    File fileUpload(szFullPathName);
    if (!fileUpload.Open(File::modeBinary | File::modeReadOnly)) {
      if (pszDescription && (*pszDescription)) {
        bout << "ERR: " << pszFileName << ":" << pszDescription << wwiv::endl;
      } else {
        bout << "|#1" << pszFileName << " does not exist.";
      }
      return true;
    }
    long lFileSize = fileUpload.GetLength();
    u.numbytes = lFileSize;
    fileUpload.Close();
    strcpy(u.upby, session()->user()->GetUserNameAndNumber(session()->usernum));
    strcpy(u.date, date());
    filedate(szFullPathName, u.actualdate);
    if (d.mask & mask_PD) {
      d.mask = mask_PD;
    }
    bout.nl();

    char szTempDisplayFileName[ MAX_PATH ];
    strcpy(szTempDisplayFileName, u.filename);
    bout << "|#9File name   : |#2" << StringRemoveWhitespace(szTempDisplayFileName) << wwiv::endl;
    bout << "|#9File size   : |#2" << bytes_to_k(u.numbytes) << wwiv::endl;
    if (pszDescription && *pszDescription) {
      strncpy(u.description, pszDescription, 58);
      u.description[58] = '\0';
      bout << "|#1 Description: " << u.description << wwiv::endl;
    } else {
      bout << "|#9Enter a description for this file.\r\n|#7: ";
      inputl(u.description, 58, true);
    }
    bout.nl();
    if (u.description[0] == 0) {
      return false;
    }
    get_file_idz(&u, nDirectoryNum);
    session()->user()->SetFilesUploaded(session()->user()->GetFilesUploaded() + 1);
    if (!(d.mask & mask_cdrom)) {
      modify_database(u.filename, true);
    }
    session()->user()->SetUploadK(session()->user()->GetUploadK() + bytes_to_k(lFileSize));
    time_t tCurrentTime = time(nullptr);
    u.daten = static_cast<unsigned long>(tCurrentTime);
    File fileDownload(g_szDownloadFileName);
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
    u1.daten = static_cast<unsigned long>(tCurrentTime);
    session()->m_DirectoryDateCache[nDirectoryNum] = static_cast<unsigned long>(tCurrentTime);
    FileAreaSetRecord(fileDownload, 0);
    fileDownload.Write(&u1, sizeof(uploadsrec));
    fileDownload.Close();
    WStatus *pStatus = application()->GetStatusManager()->BeginTransaction();
    pStatus->IncrementNumUploadsToday();
    pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
    application()->GetStatusManager()->CommitTransaction(pStatus);
    sysoplogf("+ \"%s\" uploaded on %s", u.filename, d.name);
    application()->UpdateTopScreen();
  }
  return true;
}


bool maybe_upload(const char *pszFileName, int nDirectoryNum, const char *pszDescription) {
  char s[81], ch, s1[81];
  bool abort = false;
  bool ok = true;
  uploadsrec u;

  strcpy(s, pszFileName);
  align(s);
  int i = recno(s);

  if (i == -1) {
    if (application()->HasConfigFlag(OP_FLAGS_FAST_SEARCH) && (!is_uploadable(pszFileName) && dcs())) {
      bout.bprintf("|#2%-12s: ", pszFileName);
      bout << "|#5In filename database - add anyway? ";
      ch = ynq();
      if (ch == *str_quit) {
        return false;
      } else {
        if (ch == *(YesNoString(false))) {
          bout << "|#5Delete it? ";
          if (yesno()) {
            sprintf(s1, "%s%s", directories[nDirectoryNum].path, pszFileName);
            File::Remove(s1);
            bout.nl();
            return true;
          } else {
            return true;
          }
        }
      }
    }
    if (!upload_file(s, udir[nDirectoryNum].subnum, pszDescription)) {
      ok = false;
    }
  } else {
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    int ocd = session()->GetCurrentFileArea();
    session()->SetCurrentFileArea(nDirectoryNum);
    printinfo(&u, &abort);
    session()->SetCurrentFileArea(ocd);
    if (abort) {
      ok = false;
    }
  }
  return ok;
}


/* This assumes the file holds listings of files, one per line, to be
 * uploaded.  The first word (delimited by space/tab) must be the filename.
 * after the filename are optional tab/space separated words (such as file
 * size or date/time).  After the optional words is the description, which
 * is from that position to the end of the line.  the "type" parameter gives
 * the number of optional words between the filename and description.
 * the optional words (size, date/time) are ignored completely.
 */
void upload_files(const char *pszFileName, int nDirectoryNum, int type) {
  char s[255], *fn1 = nullptr, *pszDescription = nullptr, last_fn[81], *ext = nullptr;
  bool abort = false;
  int ok1, i;
  bool ok = true;
  uploadsrec u;

  last_fn[0] = 0;
  dliscan1(udir[nDirectoryNum].subnum);

  TextFile file(pszFileName, "r");
  if (!file.IsOpen()) {
    char szDefaultFileName[ MAX_PATH ];
    sprintf(szDefaultFileName, "%s%s", directories[udir[nDirectoryNum].subnum].path, pszFileName);
    file.Open(szDefaultFileName, "r");
  }
  if (!file.IsOpen()) {
    bout << pszFileName << ": not found.\r\n";
  } else {
    while (ok && file.ReadLine(s, 250)) {
      if (s[0] < SPACE) {
        continue;
      } else if (s[0] == SPACE) {
        if (last_fn[0]) {
          if (!ext) {
            ext = static_cast<char *>(BbsAllocA(4096L));
            *ext = 0;
          }
          for (pszDescription = s; (*pszDescription == ' ') || (*pszDescription == '\t'); pszDescription++);
          if (*pszDescription == '|') {
            do {
              pszDescription++;
            } while ((*pszDescription == ' ') || (*pszDescription == '\t'));
          }
          fn1 = strchr(pszDescription, '\n');
          if (fn1) {
            *fn1 = 0;
          }
          strcat(ext, pszDescription);
          strcat(ext, "\r\n");
        }
      } else {
        ok1 = 0;
        fn1 = strtok(s, " \t\n");
        if (fn1) {
          ok1 = 1;
          for (i = 0; ok1 && (i < type); i++) {
            if (strtok(nullptr, " \t\n") == nullptr) {
              ok1 = 0;
            }
          }
          if (ok1) {
            pszDescription = strtok(nullptr, "\n");
            if (!pszDescription) {
              ok1 = 0;
            }
          }
        }
        if (ok1) {
          if (last_fn[0] && ext && *ext) {
            File fileDownload(g_szDownloadFileName);
            fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
            FileAreaSetRecord(fileDownload, 1);
            fileDownload.Read(&u, sizeof(uploadsrec));
            if (wwiv::strings::IsEquals(last_fn, u.filename)) {
              modify_database(u.filename, true);
              add_extended_description(last_fn, ext);
              u.mask |= mask_extended;
              FileAreaSetRecord(fileDownload, 1);
              fileDownload.Write(&u, sizeof(uploadsrec));
            }
            fileDownload.Close();
            *ext = 0;
          }
          while (*pszDescription == ' ' || *pszDescription == '\t') {
            ++pszDescription;
          }
          ok = maybe_upload(fn1, nDirectoryNum, pszDescription);
          checka(&abort);
          if (abort) {
            ok = false;
          }
          if (ok) {
            strcpy(last_fn, fn1);
            align(last_fn);
            if (ext) {
              *ext = 0;
            }
          }
        }
      }
    }
    file.Close();
    if (ok && last_fn[0] && ext && *ext) {
      File fileDownload(g_szDownloadFileName);
      fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      FileAreaSetRecord(fileDownload, 1);
      fileDownload.Read(&u, sizeof(uploadsrec));
      if (wwiv::strings::IsEquals(last_fn, u.filename)) {
        modify_database(u.filename, true);
        add_extended_description(last_fn, ext);
        u.mask |= mask_extended;
        FileAreaSetRecord(fileDownload, 1);
        fileDownload.Write(&u, sizeof(uploadsrec));
      }
      fileDownload.Close();
    }
  }

  if (ext) {
    free(ext);
  }
}

// returns false on abort
bool uploadall(int nDirectoryNum) {
  dliscan1(udir[nDirectoryNum].subnum);

  char szDefaultFileSpec[ MAX_PATH ];
  strcpy(szDefaultFileSpec, "*.*");

  char szPathName[ MAX_PATH ];
  sprintf(szPathName, "%s%s", directories[udir[nDirectoryNum].subnum].path, szDefaultFileSpec);
  int maxf = directories[udir[nDirectoryNum].subnum].maxfiles;

  WFindFile fnd;
  bool bFound = fnd.open(szPathName, 0);
  bool ok = true;
  bool abort = false;
  while (bFound && !hangup && (session()->numf < maxf) && ok && !abort) {
    const char *pszCurrentFile = fnd.GetFileName();
    if (pszCurrentFile &&
        *pszCurrentFile &&
        !wwiv::strings::IsEquals(pszCurrentFile, ".") &&
        !wwiv::strings::IsEquals(pszCurrentFile, "..")) {
      ok = maybe_upload(pszCurrentFile, nDirectoryNum, nullptr);
    }
    bFound = fnd.next();
    checka(&abort);
  }
  if (!ok || abort) {
    bout << "|#6Aborted.\r\n";
    ok = false;
  }
  if (session()->numf >= maxf) {
    bout << "directory full.\r\n";
  }
  return ok;
}


void relist() {
  char s[85], s1[40], s2[81];
  int i, i1;
  bool next, abort = 0;
  int tcd = -1, otag, tcdi;

  if (!filelist) {
    return;
  }
  bout.cls();
  lines_listed = 0;
  otag = session()->tagging;
  session()->tagging = 0;
  if (application()->HasConfigFlag(OP_FLAGS_FAST_TAG_RELIST)) {
    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    if (okansi()) {
      bout <<
                         "\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\r\n";
    } else {
      bout << "--+------------+-----+----+----------------------------------------------------\r\n";
    }
  }
  for (i = 0; i < session()->tagptr; i++) {
    if (!application()->HasConfigFlag(OP_FLAGS_FAST_TAG_RELIST)) {
      if (tcd != filelist[i].directory) {
        bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
        if (tcd != -1) {
          if (okansi()) {
            bout << "\r" <<
                               "\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                               << wwiv::endl;
          } else {
            bout << "\r" << "--+------------+-----+----+---------------------------------------------------" <<
                               wwiv::endl;
          }
        }
        tcd = filelist[i].directory;
        tcdi = -1;
        for (i1 = 0; i1 < session()->num_dirs; i1++) {
          if (udir[i1].subnum == tcd) {
            tcdi = i1;
            break;
          }
        }
        if (session()->user()->IsUseExtraColor()) {
          bout.Color(2);
        }
        if (tcdi == -1) {
          bout << directories[tcd].name << "." << wwiv::endl;
        } else {
          bout << directories[tcd].name << " - #" << udir[tcdi].keys << ".\r\n";
        }
        bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
        if (okansi()) {
          bout <<
                             "\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                             << wwiv::endl;
        } else {
          bout << "--+------------+-----+----+---------------------------------------------------" << wwiv::endl;
        }
      }
    }
    sprintf(s, "%c%d%2d%c%d%c",
            0x03,
            check_batch_queue(filelist[i].u.filename) ? 6 : 0,
            i + 1,
            0x03,
            session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0,
            okansi() ? '\xBA' : '|');
    osan(s, &abort, &next);
    if (session()->user()->IsUseExtraColor()) {
      bout.Color(1);
    }
    strncpy(s, filelist[i].u.filename, 8);
    s[8] = 0;
    osan(s, &abort, &next);
    strncpy(s, &((filelist[i].u.filename)[8]), 4);
    s[4] = 0;
    if (session()->user()->IsUseExtraColor()) {
      bout.Color(1);
    }
    osan(s, &abort, &next);
    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    osan((okansi() ? "\xBA" : ":"), &abort, &next);

    sprintf(s1, "%ld""k", bytes_to_k(filelist[i].u.numbytes));
    if (!application()->HasConfigFlag(OP_FLAGS_FAST_TAG_RELIST)) {
      if (!(directories[tcd].mask & mask_cdrom)) {
        sprintf(s2, "%s%s", directories[tcd].path, filelist[i].u.filename);
        StringRemoveWhitespace(s2);
        if (!File::Exists(s2)) {
          strcpy(s1, "N/A");
        }
      }
    }
    for (i1 = 0; i1 < 5 - wwiv::strings::GetStringLength(s1); i1++) {
      s[i1] = SPACE;
    }
    s[i1] = 0;
    strcat(s, s1);
    if (session()->user()->IsUseExtraColor()) {
      bout.Color(2);
    }
    osan(s, &abort, &next);

    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    osan((okansi() ? "\xBA" : "|"), &abort, &next);
    sprintf(s1, "%d", filelist[i].u.numdloads);

    for (i1 = 0; i1 < 4 - wwiv::strings::GetStringLength(s1); i1++) {
      s[i1] = SPACE;
    }
    s[i1] = 0;
    strcat(s, s1);
    if (session()->user()->IsUseExtraColor()) {
      bout.Color(2);
    }
    osan(s, &abort, &next);

    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    osan((okansi() ? "\xBA" : "|"), &abort, &next);
    sprintf(s, "%c%d%s",
            0x03,
            (filelist[i].u.mask & mask_extended) ? 1 : 2,
            filelist[i].u.description);
    plal(s, session()->user()->GetScreenChars() - 28, &abort);
  }
  bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
  if (okansi()) {
    bout << "\r" <<
                       "\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                       << wwiv::endl;
  } else {
    bout << "\r" << "--+------------+-----+----+---------------------------------------------------" <<
                       wwiv::endl;
  }
  session()->tagging = otag;
  lines_listed = 0;
}

void edit_database()
/*
 * Allows user to add or remove ALLOW.DAT entries.
 */
{
  char ch, s[20];
  bool done = false;

  if (!application()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return;
  }

  do {
    bout.nl();
    bout << "|#2A|#7)|#9 Add to ALLOW.DAT\r\n";
    bout << "|#2R|#7)|#9 Remove from ALLOW.DAT\r\n";
    bout << "|#2Q|#7)|#9 Quit\r\n";
    bout.nl();
    bout << "|#7Select: ";
    ch = onek("QAR");
    switch (ch) {
    case 'A':
      bout.nl();
      bout << "|#2Filename: ";
      input(s, 12, true);
      if (s[0]) {
        modify_database(s, true);
      }
      break;
    case 'R':
      bout.nl();
      bout << "|#2Filename: ";
      input(s, 12, true);
      if (s[0]) {
        modify_database(s, false);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!hangup && !done);
}


/*
 * Checks the database file to see if filename is there.  If so, the record
 * number plus one is returned.  If not, the negative of record number
 * that it would be minus one is returned.  The plus one is there so that there
 * is no record #0 ambiguity.
 *
 */



long db_index(File &fileAllow, const char *pszFileName) {
  char cfn[18], tfn[81], tfn1[81];
  int i = 0;
  long hirec, lorec, currec, ocurrec = -1;

  strcpy(tfn1, pszFileName);
  align(tfn1);
  strcpy(tfn, stripfn(tfn1));

  hirec = fileAllow.GetLength() / 13;
  lorec = 0;

  if (hirec == 0) {
    return -1;
  }

  for (;;) {
    currec = (hirec + lorec) / 2;
    if (currec == ocurrec) {
      if (i < 0) {
        return (-(currec + 2));
      } else {
        return (-(currec + 1));
      }
    }
    ocurrec = currec;
    fileAllow.Seek(currec * 13, File::seekBegin);
    fileAllow.Read(&cfn, 13);
    i = wwiv::strings::StringCompare(cfn, tfn);

    // found
    if (i == 0) {
      return (currec + 1);
    } else {
      if (i < 0) {
        lorec = currec;
      } else {
        hirec = currec;
      }
    }
  }
}

/*
 * Adds or deletes a single entry to/from filename database.
 */

#define ALLOW_BUFSIZE 61440

void modify_database(const char *pszFileName, bool add) {
  char tfn[MAX_PATH], tfn1[MAX_PATH];
  unsigned int nb;
  long l, l1, cp;

  if (!application()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return;
  }

  File fileAllow(syscfg.datadir, ALLOW_DAT);
  if (!fileAllow.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    return;
  }

  long rec = db_index(fileAllow, pszFileName);

  if ((rec < 0 && !add) || (rec > 0 && add)) {
    fileAllow.Close();
    return;
  }
  char * bfr = static_cast<char *>(BbsAllocA(ALLOW_BUFSIZE));
  if (!bfr) {
    fileAllow.Close();
    return;
  }
  long len = fileAllow.GetLength();

  if (add) {
    cp = (-1 - rec) * 13;
    l1 = len;

    do {
      l = l1 - cp;
      if (l < ALLOW_BUFSIZE) {
        nb = (unsigned int) l;
      } else {
        nb = ALLOW_BUFSIZE;
      }
      if (nb) {
        fileAllow.Seek(l1 - static_cast<long>(nb), File::seekBegin);
        fileAllow.Read(bfr, nb);
        fileAllow.Seek(l1 - static_cast<long>(nb) + 13, File::seekBegin);
        fileAllow.Write(bfr, nb);
        l1 -= nb;
      }
    } while (nb == ALLOW_BUFSIZE);

    // put in the new value
    strcpy(tfn1, pszFileName);
    align(tfn1);
    strncpy(tfn, stripfn(tfn1), 13);
    fileAllow.Seek(cp, File::seekBegin);
    fileAllow.Write(tfn, 13);
  } else {
    cp = rec * 13;

    do {
      l = len - cp;
      if (l < ALLOW_BUFSIZE) {
        nb = (unsigned int) l;
      } else {
        nb = ALLOW_BUFSIZE;
      }
      if (nb) {
        fileAllow.Seek(cp, File::seekBegin);
        fileAllow.Read(bfr, nb);
        fileAllow.Seek(cp - 13, File::seekBegin);
        fileAllow.Write(bfr, nb);
        cp += nb;
      }
    } while (nb == ALLOW_BUFSIZE);

    // truncate the file
    fileAllow.SetLength(len - 13);
  }

  free(bfr);
  fileAllow.Close();
}

/*
 * Returns 1 if file not found in filename database.
 */

bool is_uploadable(const char *pszFileName) {
  if (!application()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return true;
  }

  File fileAllow(syscfg.datadir, ALLOW_DAT);
  if (!fileAllow.Open(File::modeBinary | File::modeReadOnly)) {
    return true;
  }
  long rc = db_index(fileAllow, pszFileName);
  fileAllow.Close();
  return (rc < 0) ? true : false;
}

void l_config_nscan() {
  int i, i1;
  char s[81], s2[81];

  bool abort = false;
  bout.nl();
  bout << "|#9Directories to new-scan marked with '|#2*|#9'\r\n\n";
  for (i = 0; (i < session()->num_dirs) && (udir[i].subnum != -1) && (!abort); i++) {
    i1 = udir[i].subnum;
    if (qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      strcpy(s, "* ");
    } else {
      strcpy(s, "  ");
    }
    sprintf(s2, "%s%s. %s", s, udir[i].keys, directories[i1].name);
    pla(s2, &abort);
  }
  bout.nl(2);
}

void config_nscan() {
  char *s, s1[MAX_CONFERENCES + 2], s2[120], ch;
  int i, i1, oc, os;
  bool abort = false;
  bool done, done1;

  if (okansi()) {
    // ZU - SCONFIG
    config_scan_plus(NSCAN);    // ZU - SCONFIG
    return;             // ZU - SCONFIG
  }                 // ZU - SCONFIG

  done = done1 = false;
  oc = session()->GetCurrentConferenceFileArea();
  os = udir[session()->GetCurrentFileArea()].subnum;

  do {
    if (okconf(session()->user()) && uconfdir[1].confnum != -1) {
      abort = false;
      strcpy(s1, " ");
      bout.nl();
      bout << "Select Conference: \r\n\n";
      i = 0;
      while ((i < dirconfnum) && (uconfdir[i].confnum != -1) && (!abort)) {
        sprintf(s2, "%c) %s", dirconfs[uconfdir[i].confnum].designator,
                stripcolors(reinterpret_cast<char*>(dirconfs[uconfdir[i].confnum].name)));
        pla(s2, &abort);
        s1[i + 1] = dirconfs[uconfdir[i].confnum].designator;
        s1[i + 2] = 0;
        i++;
      }
      bout.nl();
      bout << " Select [" << &s1[1] << ", <space> to quit]: ";
      ch = onek(s1);
    } else {
      ch = '-';
    }
    switch (ch) {
    case ' ':
      done1 = true;
      break;
    default:
      if (okconf(session()->user()) && dirconfnum > 1) {
        i = 0;
        while ((ch != dirconfs[uconfdir[i].confnum].designator) && (i < static_cast<int>(dirconfnum))) {
          i++;
        }
        if (i >= static_cast<int>(dirconfnum)) {
          break;
        }

        setuconf(CONF_DIRS, i, -1);
      }
      l_config_nscan();
      done = false;
      do {
        bout.nl();
        bout << "|#9Enter directory number (|#1C=Clr All, Q=Quit, S=Set All|#9): |#0";
        s = mmkey(1);
        if (s[0]) {
          for (i = 0; i < session()->num_dirs; i++) {
            i1 = udir[i].subnum;
            if (wwiv::strings::IsEquals(udir[i].keys, s)) {
              qsc_n[i1 / 32] ^= 1L << (i1 % 32);
            }
            if (wwiv::strings::IsEquals(s, "S")) {
              qsc_n[i1 / 32] |= 1L << (i1 % 32);
            }
            if (wwiv::strings::IsEquals(s, "C")) {
              qsc_n[i1 / 32] &= ~(1L << (i1 % 32));
            }
          }
          if (wwiv::strings::IsEquals(s, "Q")) {
            done = true;
          }
          if (wwiv::strings::IsEquals(s, "?")) {
            l_config_nscan();
          }
        }
      } while (!done && !hangup);
      break;
    }
    if (!okconf(session()->user()) || uconfdir[1].confnum == -1) {
      done1 = true;
    }
  } while (!done1 && !hangup);

  if (okconf(session()->user())) {
    setuconf(CONF_DIRS, oc, os);
  }
}


void xfer_defaults() {
  char s[81], ch;
  int i;
  bool done = false;

  do {
    bout.cls();
    bout << "|#7[|#21|#7]|#1 Set New-Scan Directories.\r\n";
    bout << "|#7[|#22|#7]|#1 Set Default Protocol.\r\n";
    bout << "|#7[|#23|#7]|#1 New-Scan Transfer after Message Base (" <<
                       YesNoString(session()->user()->IsNewScanFiles()) << ").\r\n";
    bout << "|#7[|#24|#7]|#1 Number of lines of extended description to print [" <<
                       session()->user()->GetNumExtended() << " line(s)].\r\n";
    bout << "|#7[|#25|#7]|#1 File tagging (";
    if (session()->user()->IsUseNoTagging()) {
      bout << "Disabled)\r\n";
    } else {
      bout << "Enabled";
    }
    if (!session()->user()->IsUseListPlus() && !session()->user()->IsUseNoTagging()) {
      bout << " - ListPlus!)\r\n";
    } else if (!session()->user()->IsUseNoTagging()) {
      bout << " - Internal Tagging)\r\n";
    }
    bout << "|#7[|#2Q|#7]|#1 Quit.\r\n\n";
    bout << "|#5Which? ";
    ch = onek("Q12345");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '1':
      config_nscan();
      break;
    case '2':
      bout.nl(2);
      bout << "|#9Enter your default protocol, |#20|#9 for none.\r\n\n";
      i = get_protocol(xf_down);
      if (i >= 0) {
        session()->user()->SetDefaultProtocol(i);
      }
      break;
    case '3':
      session()->user()->ToggleStatusFlag(WUser::nscanFileSystem);
      break;
    case '4':
      bout.nl(2);
      bout << "|#9How many lines of an extended description\r\n";
      bout << "|#9do you want to see when listing files (|#20-" << session()->max_extend_lines << "|#7)\r\n";
      bout << "|#9Current # lines: |#2" << session()->user()->GetNumExtended() << wwiv::endl;
      bout << "|#7: ";
      input(s, 3);
      if (s[0]) {
        i = atoi(s);
        if ((i >= 0) && (i <= session()->max_extend_lines)) {
          session()->user()->SetNumExtended(i);
        }
      }
      break;
    case '5':
      if (session()->user()->IsUseNoTagging()) {
        session()->user()->ClearStatusFlag(WUser::noTag);
        check_listplus();
      } else {
        session()->user()->SetStatusFlag(WUser::noTag);
      }
      break;
    }
  } while (!done && !hangup);
}



void finddescription() {
  uploadsrec u;
  int i, i1, i2, pts, count, color, ac = 0;
  char s[81], s1[81];

  if (ok_listplus()) {
    listfiles_plus(LP_SEARCH_ALL);
    return;
  }

  bout.nl();
  if (uconfdir[1].confnum != -1 && okconf(session()->user())) {
    if (!x_only) {
      bout << "|#5All conferences? ";
      ac = yesno();
    } else {
      ac = 1;
    }
    if (ac) {
      tmp_disable_conf(true);
    }
  }
  bout << "\r\nFind description -\r\n\n";
  bout << "Enter string to search for in file description:\r\n:";
  input(s1, 58);
  if (s1[0] == 0) {
    tmp_disable_conf(false);
    return;
  }
  int ocd = session()->GetCurrentFileArea();
  bool abort = false;
  g_num_listed = 0;
  count = 0;
  color = 3;
  if (!x_only) {
    bout << "\r|#2Searching ";
  }
  lines_listed = 0;
  for (i = 0; (i < session()->num_dirs) && (!abort) && (!hangup) && (session()->tagging != 0)
       && (udir[i].subnum != -1); i++) {
    i1 = udir[i].subnum;
    pts = 0;
    session()->titled = 1;
    if (qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      pts = 1;
    }
    pts = 1;
    // remove pts=1 to search only marked directories
    if ((pts) && (!abort) && (session()->tagging != 0)) {
      if (!x_only) {
        count++;
        bout << static_cast<char>(3) << color << ".";
        if (count == NUM_DOTS) {
          bout << "\r|#2Searching ";
          color++;
          count = 0;
          if (color == 4) {
            color++;
          }
          if (color == 10) {
            color = 0;
          }
        }
      }
      session()->SetCurrentFileArea(i);
      dliscan();
      File fileDownload(g_szDownloadFileName);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      for (i1 = 1; (i1 <= session()->numf) && (!abort) && (!hangup) && (session()->tagging != 0); i1++) {
        FileAreaSetRecord(fileDownload, i1);
        fileDownload.Read(&u, sizeof(uploadsrec));
        strcpy(s, u.description);
        for (i2 = 0; i2 < wwiv::strings::GetStringLength(s); i2++) {
          s[i2] = upcase(s[i2]);
        }
        if (strstr(s, s1) != nullptr) {
          fileDownload.Close();
          printinfo(&u, &abort);
          fileDownload.Open(File::modeBinary | File::modeReadOnly);
        } else if (bkbhit()) {
          checka(&abort);
        }
      }
      fileDownload.Close();
    }
  }
  if (ac) {
    tmp_disable_conf(false);
  }
  session()->SetCurrentFileArea(ocd);
  endlist(1);
}


void arc_l() {
  char szFileSpec[MAX_PATH];
  uploadsrec u;

  bout.nl();
  bout << "|#2File for listing: ";
  input(szFileSpec, 12);
  if (strchr(szFileSpec, '.') == nullptr) {
    strcat(szFileSpec, ".*");
  }
  if (!okfn(szFileSpec)) {
    szFileSpec[0] = 0;
  }
  align(szFileSpec);
  dliscan();
  bool abort = false;
  int nRecordNum = recno(szFileSpec);
  do {
    if (nRecordNum > 0) {
      File fileDownload(g_szDownloadFileName);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      FileAreaSetRecord(fileDownload, nRecordNum);
      fileDownload.Read(&u, sizeof(uploadsrec));
      fileDownload.Close();
      int i1 = list_arc_out(stripfn(u.filename), directories[udir[session()->GetCurrentFileArea()].subnum].path);
      if (i1) {
        abort = true;
      }
      checka(&abort);
      nRecordNum = nrecno(szFileSpec, nRecordNum);
    }
  } while (nRecordNum > 0 && !hangup && !abort);
}
