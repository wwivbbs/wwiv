/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <string>

#include "bbs/application.h"
#include "bbs/batch.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl2.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/dirlist.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "bbs/listplus.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/defaults.h"
#include "bbs/utility.h"
#include "bbs/sr.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "bbs/xfertmp.h"
#include "bbs/mmkey.h"
#include "bbs/sysoplog.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "sdk/status.h" 
#include "core/strings.h"
#include "core/findfiles.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

extern char str_quit[];

void move_file() {
  char s1[81], s2[81];
  int d1 = 0, d2 = 0;
  uploadsrec u, u1;

  bool ok = false;
  bout.nl(2);
  bout << "|#2Filename to move: ";
  char szFileMask[MAX_PATH];
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
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, nCurRecNum);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();
    printfileinfo(&u, a()->current_user_dir().subnum);
    bout.nl();
    bout << "|#5Move this (Y/N/Q)? ";
    char ch = ynq();
    if (ch == 'Q') {
      done = true;
    } else if (ch == 'Y') {
      sprintf(s1, "%s%s", a()->directories[a()->current_user_dir().subnum].path, u.filename);
      string ss;
      do {
        bout.nl(2);
        bout << "|#2To which directory? ";
        ss = mmkey(MMKeyAreaType::dirs);
        if (ss[0] == '?') {
          dirlist(1);
          dliscan();
        }
      } while ((!hangup) && (ss[0] == '?'));
      d1 = -1;
      if (!ss.empty()) {
        for (size_t i1 = 0; (i1 < a()->directories.size()) && (a()->udir[i1].subnum != -1); i1++) {
          if (ss == a()->udir[i1].keys) {
            d1 = i1;
          }
        }
      }
      if (d1 != -1) {
        ok = true;
        d1 = a()->udir[d1].subnum;
        dliscan1(d1);
        if (recno(u.filename) > 0) {
          ok = false;
          bout << "\r\nFilename already in use in that directory.\r\n";
        }
        if (a()->numf >= a()->directories[d1].maxfiles) {
          ok = false;
          bout << "\r\nToo many files in that directory.\r\n";
        }
        if (File::freespace_for_path(a()->directories[d1].path) < ((double)(u.numbytes / 1024L) + 3)) {
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
        u.daten = daten_t_now();
      }
      --nCurrentPos;
      fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      for (int i1 = nCurRecNum; i1 < a()->numf; i1++) {
        FileAreaSetRecord(fileDownload, i1 + 1);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        FileAreaSetRecord(fileDownload, i1);
        fileDownload.Write(&u1, sizeof(uploadsrec));
      }
      --a()->numf;
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Read(&u1, sizeof(uploadsrec));
      u1.numbytes = a()->numf;
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Write(&u1, sizeof(uploadsrec));
      fileDownload.Close();
      string ss = read_extended_description(u.filename);
      if (!ss.empty()) {
        delete_extended_description(u.filename);
      }

      sprintf(s2, "%s%s", a()->directories[d1].path, u.filename);
      dliscan1(d1);
      fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      for (int i = a()->numf; i >= 1; i--) {
        FileAreaSetRecord(fileDownload, i);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        FileAreaSetRecord(fileDownload, i + 1);
        fileDownload.Write(&u1, sizeof(uploadsrec));
      }
      FileAreaSetRecord(fileDownload, 1);
      fileDownload.Write(&u, sizeof(uploadsrec));
      ++a()->numf;
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Read(&u1, sizeof(uploadsrec));
      u1.numbytes = a()->numf;
      if (u.daten > u1.daten) {
        u1.daten = u.daten;
      }
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Write(&u1, sizeof(uploadsrec));
      fileDownload.Close();
      if (!ss.empty()) {
        add_extended_description(u.filename, ss);
      }
      StringRemoveWhitespace(s1);
      StringRemoveWhitespace(s2);
      if (!IsEquals(s1, s2) && File::Exists(s1)) {
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

static int comparedl(uploadsrec * x, uploadsrec * y, int type) {
  switch (type) {
  case 0:
    return StringCompare(x->filename, y->filename);
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

static void quicksort(int l, int r, int type) {
  uploadsrec ua, a2, x;

  int i = l;
  int j = r;
  File fileDownload(a()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);

  FileAreaSetRecord(fileDownload, ((l + r) / 2));
  fileDownload.Read(&x, sizeof(uploadsrec));
  do {
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&ua, sizeof(uploadsrec));
    while (comparedl(&ua, &x, type) < 0) {
      FileAreaSetRecord(fileDownload, ++i);
      fileDownload.Read(&ua, sizeof(uploadsrec));
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
        fileDownload.Write(&a2, sizeof(uploadsrec));
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


void sortdir(int directory_num, int type) {
  dliscan1(directory_num);
  if (a()->numf > 1) {
    quicksort(1, a()->numf, type);
  }
}


void sort_all(int type) {
  tmp_disable_conf(true);
  for (size_t i = 0; (i < a()->directories.size()) && (a()->udir[i].subnum != -1) &&
       (!a()->localIO()->KeyPressed()); i++) {
    bout << "\r\n|#1Sorting " << a()->directories[a()->udir[i].subnum].name << wwiv::endl;
    sortdir(i, type);
  }
  tmp_disable_conf(false);
}


void rename_file() {
  char s[81], s1[81], s2[81], s3[81];
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
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    int nCurRecNum = nRecNum;
    FileAreaSetRecord(fileDownload, nRecNum);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();
    printfileinfo(&u, a()->current_user_dir().subnum);
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
      if (!IsEquals(s, "        .   ")) {
        strcpy(s1, a()->directories[a()->current_user_dir().subnum].path);
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
            string ss = read_extended_description(u.filename);
            if (!ss.empty()) {
              delete_extended_description(u.filename);
              add_extended_description(s, ss);
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
    string ss = read_extended_description(u.filename);
    bout.nl(2);
    bout << "|#5Modify extended description? ";
    if (yesno()) {
      bout.nl();
      if (!ss.empty()) {
        bout << "|#5Delete it? ";
        if (yesno()) {
          delete_extended_description(u.filename);
          u.mask &= ~mask_extended;
        } else {
          u.mask |= mask_extended;
          modify_extended_description(&ss,
              a()->directories[a()->current_user_dir().subnum].name);
          if (!ss.empty()) {
            delete_extended_description(u.filename);
            add_extended_description(u.filename, ss);
          }
        }
      } else {
        modify_extended_description(&ss,
            a()->directories[a()->current_user_dir().subnum].name);
        if (!ss.empty()) {
          add_extended_description(u.filename, ss);
          u.mask |= mask_extended;
        } else {
          u.mask &= ~mask_extended;
        }
      }
    } else if (!ss.empty()) {
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

static bool upload_file(const char *file_name, uint16_t directory_num, const char *description) {
  uploadsrec u, u1;

  directoryrec d = a()->directories[directory_num];
  char szTempFileName[255];
  strcpy(szTempFileName, file_name);
  align(szTempFileName);
  strcpy(u.filename, szTempFileName);
  u.ownerusr = static_cast<uint16_t>(a()->usernum);
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  if (!(d.mask & mask_cdrom) && !check_ul_event(directory_num, &u)) {
    bout << file_name << " was deleted by upload event.\r\n";
  } else {
    char szUnalignedFileName[MAX_PATH];
    strcpy(szUnalignedFileName, szTempFileName);
    unalign(szUnalignedFileName);

    char szFullPathName[MAX_PATH];
    sprintf(szFullPathName, "%s%s", d.path, szUnalignedFileName);

    File fileUpload(szFullPathName);
    if (!fileUpload.Open(File::modeBinary | File::modeReadOnly)) {
      if (description && (*description)) {
        bout << "ERR: " << file_name << ":" << description << wwiv::endl;
      } else {
        bout << "|#1" << file_name << " does not exist.";
      }
      return true;
    }
    auto fs = fileUpload.length();
    u.numbytes = static_cast<daten_t>(fs);
    fileUpload.Close();
    const string unn = a()->names()->UserName(a()->usernum);
    strcpy(u.upby, unn.c_str());
    to_char_array(u.date, date());

    File f(szFullPathName);
    auto t = daten_to_mmddyy(time_t_to_daten(f.creation_time()));
    to_char_array(u.actualdate, t);

    if (d.mask & mask_PD) {
      d.mask = mask_PD;
    }
    bout.nl();

    char szTempDisplayFileName[MAX_PATH];
    strcpy(szTempDisplayFileName, u.filename);
    bout << "|#9File name   : |#2" << StringRemoveWhitespace(szTempDisplayFileName) << wwiv::endl;
    bout << "|#9File size   : |#2" << bytes_to_k(u.numbytes) << wwiv::endl;
    if (description && *description) {
      to_char_array(u.description, description);
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
    get_file_idz(&u, directory_num);
    a()->user()->SetFilesUploaded(a()->user()->GetFilesUploaded() + 1);
    if (!(d.mask & mask_cdrom)) {
      modify_database(u.filename, true);
    }
    a()->user()->SetUploadK(a()->user()->GetUploadK() + bytes_to_k(fs));
    u.daten = daten_t_now();
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    for (int i = a()->numf; i >= 1; i--) {
      FileAreaSetRecord(fileDownload, i);
      fileDownload.Read(&u1, sizeof(uploadsrec));
      FileAreaSetRecord(fileDownload, i + 1);
      fileDownload.Write(&u1, sizeof(uploadsrec));
    }
    FileAreaSetRecord(fileDownload, 1);
    fileDownload.Write(&u, sizeof(uploadsrec));
    ++a()->numf;
    FileAreaSetRecord(fileDownload, 0);
    fileDownload.Read(&u1, sizeof(uploadsrec));
    u1.numbytes = a()->numf;
    u1.daten = daten_t_now();
    FileAreaSetRecord(fileDownload, 0);
    fileDownload.Write(&u1, sizeof(uploadsrec));
    fileDownload.Close();
    WStatus *pStatus = a()->status_manager()->BeginTransaction();
    pStatus->IncrementNumUploadsToday();
    pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
    a()->status_manager()->CommitTransaction(pStatus);
    sysoplog() << "+ '" << u.filename << "' uploaded on " << d.name;
    a()->UpdateTopScreen();
  }
  return true;
}


bool maybe_upload(const char *file_name, uint16_t directory_num, const char *description) {
  char s[81], ch, s1[81];
  bool abort = false;
  bool ok = true;
  uploadsrec u;

  strcpy(s, file_name);
  align(s);
  int i = recno(s);

  if (i == -1) {
    if (a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH) && (!is_uploadable(file_name) && dcs())) {
      bout.bprintf("|#2%-12s: ", file_name);
      bout << "|#5In filename database - add anyway? ";
      ch = ynq();
      if (ch == *str_quit) {
        return false;
      } else {
        if (ch == *(YesNoString(false))) {
          bout << "|#5Delete it? ";
          if (yesno()) {
            sprintf(s1, "%s%s", a()->directories[directory_num].path, file_name);
            File::Remove(s1);
            bout.nl();
            return true;
          } else {
            return true;
          }
        }
      }
    }
    if (!upload_file(s, a()->udir[directory_num].subnum, description)) {
      ok = false;
    }
  } else {
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    auto ocd = a()->current_user_dir_num();
    a()->set_current_user_dir_num(directory_num);
    printinfo(&u, &abort);
    a()->set_current_user_dir_num(ocd);
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
void upload_files(const char *file_name, uint16_t directory_num, int type) {
  char s[255], *fn1 = nullptr, *description = nullptr, last_fn[81], *ext = nullptr;
  bool abort = false;
  int ok1, i;
  bool ok = true;
  uploadsrec u;

  last_fn[0] = 0;
  dliscan1(a()->udir[directory_num].subnum);

  auto file = std::make_unique<TextFile>(file_name, "r");
  if (!file->IsOpen()) {
    char szDefaultFileName[MAX_PATH];
    sprintf(szDefaultFileName, "%s%s", a()->directories[a()->udir[directory_num].subnum].path, file_name);
    file.reset(new TextFile(szDefaultFileName, "r"));
  }
  if (!file->IsOpen()) {
    bout << file_name << ": not found.\r\n";
  } else {
    while (ok && file->ReadLine(s, 250)) {
      if (s[0] < SPACE) {
        continue;
      } else if (s[0] == SPACE) {
        if (last_fn[0]) {
          if (!ext) {
            ext = static_cast<char *>(BbsAllocA(4096L));
            *ext = 0;
          }
          for (description = s; (*description == ' ') || (*description == '\t'); description++);
          if (*description == '|') {
            do {
              description++;
            } while ((*description == ' ') || (*description == '\t'));
          }
          fn1 = strchr(description, '\n');
          if (fn1) {
            *fn1 = 0;
          }
          strcat(ext, description);
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
            description = strtok(nullptr, "\n");
            if (!description) {
              ok1 = 0;
            }
          }
        }
        if (ok1) {
          if (last_fn[0] && ext && *ext) {
            File fileDownload(a()->download_filename_);
            fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
            FileAreaSetRecord(fileDownload, 1);
            fileDownload.Read(&u, sizeof(uploadsrec));
            if (IsEquals(last_fn, u.filename)) {
              modify_database(u.filename, true);
              add_extended_description(last_fn, ext);
              u.mask |= mask_extended;
              FileAreaSetRecord(fileDownload, 1);
              fileDownload.Write(&u, sizeof(uploadsrec));
            }
            fileDownload.Close();
            *ext = 0;
          }
          while (*description == ' ' || *description == '\t') {
            ++description;
          }
          ok = maybe_upload(fn1, directory_num, description);
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
    file->Close();
    if (ok && last_fn[0] && ext && *ext) {
      File fileDownload(a()->download_filename_);
      fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      FileAreaSetRecord(fileDownload, 1);
      fileDownload.Read(&u, sizeof(uploadsrec));
      if (IsEquals(last_fn, u.filename)) {
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
bool uploadall(uint16_t directory_num) {
  dliscan1(a()->udir[directory_num].subnum);

  char szDefaultFileSpec[MAX_PATH];
  strcpy(szDefaultFileSpec, "*.*");

  char szPathName[MAX_PATH];
  sprintf(szPathName, "%s%s", a()->directories[a()->udir[directory_num].subnum].path, szDefaultFileSpec);
  int maxf = a()->directories[a()->udir[directory_num].subnum].maxfiles;

  bool ok = true;
  bool abort = false;
  FindFiles ff(szPathName, FindFilesType::files);
  for (const auto& f : ff) {
    if (checka() || hangup || a()->numf >= maxf) { break; }
    if (!maybe_upload(f.name.c_str(), directory_num, nullptr)) { break; }
  }
  if (!ok || abort) {
    bout << "|#6Aborted.\r\n";
    ok = false;
  }
  if (a()->numf >= maxf) {
    bout << "directory full.\r\n";
  }
  return ok;
}

void relist() {
  char s[85], s1[40], s2[81];
  bool next, abort = 0;
  int16_t tcd = -1;

  if (a()->filelist.empty()) {
    return;
  }
  bout.cls();
  bout.clear_lines_listed();
  if (a()->HasConfigFlag(OP_FLAGS_FAST_TAG_RELIST)) {
    bout.Color(FRAME_COLOR);
    bout << string(78, '-') << wwiv::endl;
  }
  for (size_t i = 0; i < a()->filelist.size(); i++) {
    auto& f = a()->filelist[i];
    if (!a()->HasConfigFlag(OP_FLAGS_FAST_TAG_RELIST)) {
      if (tcd != f.directory) {
        bout.Color(FRAME_COLOR);
        if (tcd != -1) {
          bout << "\r" << string(78, '-') << wwiv::endl;
        }
        tcd = f.directory;
        int tcdi = -1;
        for (size_t i1 = 0; i1 < a()->directories.size(); i1++) {
          if (a()->udir[i1].subnum == tcd) {
            tcdi = i1;
            break;
          }
        }
        bout.Color(2);
        if (tcdi == -1) {
          bout << a()->directories[tcd].name << "." << wwiv::endl;
        } else {
          bout << a()->directories[tcd].name << " - #" << a()->udir[tcdi].keys << ".\r\n";
        }
        bout.Color(FRAME_COLOR);
        bout << string(78, '-') << wwiv::endl;
      }
    }
    sprintf(s, "%c%d%2d%c%d%c",
            0x03,
            check_batch_queue(f.u.filename) ? 6 : 0,
            i + 1,
            0x03,
            FRAME_COLOR,
            okansi() ? '\xBA' : ' '); // was |
    bout.bputs(s, &abort, &next);
    bout.Color(1);
    strncpy(s, f.u.filename, 8);
    s[8] = 0;
    bout.bputs(s, &abort, &next);
    strncpy(s, &((f.u.filename)[8]), 4);
    s[4] = 0;
    bout.Color(1);
    bout.bputs(s, &abort, &next);
    bout.Color(FRAME_COLOR);
    bout.bputs((okansi() ? "\xBA" : ":"), &abort, &next);

    sprintf(s1, "%ld""k", bytes_to_k(f.u.numbytes));
    if (!a()->HasConfigFlag(OP_FLAGS_FAST_TAG_RELIST)) {
      if (!(a()->directories[tcd].mask & mask_cdrom)) {
        sprintf(s2, "%s%s", a()->directories[tcd].path, f.u.filename);
        StringRemoveWhitespace(s2);
        if (!File::Exists(s2)) {
          strcpy(s1, "N/A");
        }
      }
    }
    if (strlen(s1) < 5) {
      size_t i1 = 0;
      for (; i1 < 5 - strlen(s1); i1++) {
        s[i1] = SPACE;
      }
      s[i1] = 0;
    }
    strcat(s, s1);
    bout.Color(2);
    bout.bputs(s, &abort, &next);

    bout.Color(FRAME_COLOR);
    bout.bputs((okansi() ? "\xBA" : "|"), &abort, &next);
    sprintf(s1, "%d", f.u.numdloads);
    
    size_t i1 = 0;
    for (; i1 < 4 - strlen(s1); i1++) {
      s[i1] = SPACE;
    }
    s[i1] = 0;
    strcat(s, s1);
    bout.Color(2);
    bout.bputs(s, &abort, &next);

    bout.Color(FRAME_COLOR);
    bout.bputs((okansi() ? "\xBA" : "|"), &abort, &next);
    sprintf(s, "%c%d%s",
            0x03,
            (f.u.mask & mask_extended) ? 1 : 2,
      f.u.description);
    bout.bpla(trim_to_size_ignore_colors(s, a()->user()->GetScreenChars() - 28), &abort);
  }
  bout.Color(FRAME_COLOR);
  bout << "\r" << string(78, '-') << wwiv::endl;
  bout.clear_lines_listed();
}

/*
* Allows user to add or remove ALLOW.DAT entries.
*/
void edit_database()
{
  char ch, s[20];
  bool done = false;

  if (!a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
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
static long db_index(File &fileAllow, const char *file_name) {
  char cfn[18], tfn[81], tfn1[81];
  int i = 0;
  long hirec, lorec, currec, ocurrec = -1;

  strcpy(tfn1, file_name);
  align(tfn1);
  strcpy(tfn, stripfn(tfn1));

  hirec = fileAllow.length() / 13;
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
    fileAllow.Seek(currec * 13, File::Whence::begin);
    fileAllow.Read(&cfn, 13);
    i = StringCompare(cfn, tfn);

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

void modify_database(const char *file_name, bool add) {
  char tfn[MAX_PATH], tfn1[MAX_PATH];
  unsigned int nb;
  long l, l1, cp;

  if (!a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return;
  }

  File fileAllow(a()->config()->datadir(), ALLOW_DAT);
  if (!fileAllow.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    return;
  }

  long rec = db_index(fileAllow, file_name);

  if ((rec < 0 && !add) || (rec > 0 && add)) {
    fileAllow.Close();
    return;
  }
  char* bfr = static_cast<char *>(BbsAllocA(ALLOW_BUFSIZE));
  if (!bfr) {
    fileAllow.Close();
    return;
  }
  long len = fileAllow.length();

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
        fileAllow.Seek(l1 - static_cast<long>(nb), File::Whence::begin);
        fileAllow.Read(bfr, nb);
        fileAllow.Seek(l1 - static_cast<long>(nb) + 13, File::Whence::begin);
        fileAllow.Write(bfr, nb);
        l1 -= nb;
      }
    } while (nb == ALLOW_BUFSIZE);

    // put in the new value
    strcpy(tfn1, file_name);
    align(tfn1);
    strncpy(tfn, stripfn(tfn1), 13);
    fileAllow.Seek(cp, File::Whence::begin);
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
        fileAllow.Seek(cp, File::Whence::begin);
        fileAllow.Read(bfr, nb);
        fileAllow.Seek(cp - 13, File::Whence::begin);
        fileAllow.Write(bfr, nb);
        cp += nb;
      }
    } while (nb == ALLOW_BUFSIZE);

    // truncate the file
    fileAllow.set_length(len - 13);
  }

  free(bfr);
  fileAllow.Close();
}

/*
 * Returns 1 if file not found in filename database.
 */

bool is_uploadable(const char *file_name) {
  if (!a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return true;
  }

  File fileAllow(a()->config()->datadir(), ALLOW_DAT);
  if (!fileAllow.Open(File::modeBinary | File::modeReadOnly)) {
    return true;
  }
  long rc = db_index(fileAllow, file_name);
  fileAllow.Close();
  return (rc < 0) ? true : false;
}

static void l_config_nscan() {
  char s[81], s2[81];

  bool abort = false;
  bout.nl();
  bout << "|#9Directories to new-scan marked with '|#2*|#9'\r\n\n";
  for (size_t i = 0; (i < a()->directories.size()) && (a()->udir[i].subnum != -1) && (!abort); i++) {
    size_t i1 = a()->udir[i].subnum;
    if (qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      strcpy(s, "* ");
    } else {
      strcpy(s, "  ");
    }
    sprintf(s2, "%s%s. %s", s, a()->udir[i].keys, a()->directories[i1].name);
    bout.bpla(s2, &abort);
  }
  bout.nl(2);
}

static void config_nscan() {
  char s1[MAX_CONFERENCES + 2], ch;
  int i1, oc, os;
  bool abort = false;
  bool done, done1;

  if (okansi()) {
    // ZU - SCONFIG
    config_scan_plus(NSCAN);    // ZU - SCONFIG
    return;             // ZU - SCONFIG
  }                 // ZU - SCONFIG

  done = done1 = false;
  oc = a()->GetCurrentConferenceFileArea();
  os = a()->current_user_dir().subnum;

  do {
    if (okconf(a()->user()) && a()->uconfdir[1].confnum != -1) {
      abort = false;
      strcpy(s1, " ");
      bout.nl();
      bout << "Select Conference: \r\n\n";
      size_t i = 0;
      while (i < a()->dirconfs.size() && a()->uconfdir[i].confnum != -1 && !abort) {
        const auto cn = stripcolors(a()->dirconfs[a()->uconfdir[i].confnum].conf_name);
        const auto s2 = StrCat(a()->dirconfs[a()->uconfdir[i].confnum].designator, ") ", cn);
        bout.bpla(s2, &abort);
        s1[i + 1] = a()->dirconfs[a()->uconfdir[i].confnum].designator;
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
      if (okconf(a()->user()) && dirconfnum > 1) {
        size_t i = 0;
        while ((ch != a()->dirconfs[a()->uconfdir[i].confnum].designator) && (i < static_cast<size_t>(dirconfnum))) {
          i++;
        }
        if (i >= a()->dirconfs.size()) {
          break;
        }

        setuconf(ConferenceType::CONF_DIRS, i, -1);
      }
      l_config_nscan();
      done = false;
      do {
        bout.nl();
        bout << "|#9Enter directory number (|#1C=Clr All, Q=Quit, S=Set All|#9): |#0";
        string s = mmkey(MMKeyAreaType::dirs);
        if (s[0]) {
          for (size_t i = 0; i < a()->directories.size(); i++) {
            i1 = a()->udir[i].subnum;
            if (s == a()->udir[i].keys) {
              qsc_n[i1 / 32] ^= 1L << (i1 % 32);
            }
            if (s == "S") {
              qsc_n[i1 / 32] |= 1L << (i1 % 32);
            }
            if (s == "C") {
              qsc_n[i1 / 32] &= ~(1L << (i1 % 32));
            }
          }
          if (s == "Q") {
            done = true;
          }
          if (s == "?") {
            l_config_nscan();
          }
        }
      } while (!done && !hangup);
      break;
    }
    if (!okconf(a()->user()) || a()->uconfdir[1].confnum == -1) {
      done1 = true;
    }
  } while (!done1 && !hangup);

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_DIRS, oc, os);
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
                       YesNoString(a()->user()->IsNewScanFiles()) << ").\r\n";
    bout << "|#7[|#24|#7]|#1 Number of lines of extended description to print [" <<
                       a()->user()->GetNumExtended() << " line(s)].\r\n";
    const std::string onek_options = "Q12345";
    bout << "|#7[|#2Q|#7]|#1 Quit.\r\n\n";
    bout << "|#5Which? ";
    ch = onek(onek_options.c_str());
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
        a()->user()->SetDefaultProtocol(i);
      }
      break;
    case '3':
      a()->user()->ToggleStatusFlag(User::nscanFileSystem);
      break;
    case '4':
      bout.nl(2);
      bout << "|#9How many lines of an extended description\r\n";
      bout << "|#9do you want to see when listing files (|#20-" << a()->max_extend_lines << "|#7)\r\n";
      bout << "|#9Current # lines: |#2" << a()->user()->GetNumExtended() << wwiv::endl;
      bout << "|#7: ";
      input(s, 3);
      if (s[0]) {
        i = to_number<int>(s);
        if ((i >= 0) && (i <= a()->max_extend_lines)) {
          a()->user()->SetNumExtended(i);
        }
      }
      break;
    }
  } while (!done && !hangup);
}

void finddescription() {
  uploadsrec u;
  int i2, pts, count, color;
  char s[81], s1[81];

  if (okansi()) {
    listfiles_plus(LP_SEARCH_ALL);
    return;
  }

  bout.nl();
  bool ac = false;
  if (a()->uconfdir[1].confnum != -1 && okconf(a()->user())) {
    bout << "|#5All conferences? ";
    ac = yesno();
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
  auto ocd = a()->current_user_dir_num();
  bool abort = false;
  count = 0;
  color = 3;
  bout << "\r|#2Searching ";
  bout.clear_lines_listed();
  for (uint16_t i = 0; (i < a()->directories.size()) && !abort && !hangup
       && (a()->udir[i].subnum != -1); i++) {
    auto ii1 = a()->udir[i].subnum;
    pts = 0;
    bool need_title = true;
    if (qsc_n[ii1 / 32] & (1L << (ii1 % 32))) {
      pts = 1;
    }
    pts = 1;
    // remove pts=1 to search only marked a()->directories
    if (pts && !abort) {
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
      a()->set_current_user_dir_num(i);
      dliscan();
      File fileDownload(a()->download_filename_);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      for (auto i1 = 1; i1 <= a()->numf && !abort && !hangup; i1++) {
        FileAreaSetRecord(fileDownload, i1);
        fileDownload.Read(&u, sizeof(uploadsrec));
        strcpy(s, u.description);
        for (i2 = 0; i2 < GetStringLength(s); i2++) {
          s[i2] = upcase(s[i2]);
        }
        if (strstr(s, s1) != nullptr) {
          fileDownload.Close();

          if (need_title) {
            if (bout.lines_listed() >= a()->screenlinest - 7 && !a()->filelist.empty()) {
              tag_files(need_title);
            }
            if (need_title) {
              printtitle(&abort);
              need_title = false;
            }
          }

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
  a()->set_current_user_dir_num(ocd);
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
      File fileDownload(a()->download_filename_);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      FileAreaSetRecord(fileDownload, nRecordNum);
      fileDownload.Read(&u, sizeof(uploadsrec));
      fileDownload.Close();
      int i1 = list_arc_out(stripfn(u.filename), a()->directories[a()->current_user_dir().subnum].path);
      if (i1) {
        abort = true;
      }
      checka(&abort);
      nRecordNum = nrecno(szFileSpec, nRecordNum);
    }
  } while (nRecordNum > 0 && !hangup && !abort);
}
