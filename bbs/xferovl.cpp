/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/xferovl.h"
#include "bbs/application.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/defaults.h"
#include "bbs/dirlist.h"
#include "bbs/input.h"
#include "bbs/listplus.h"
#include "bbs/mmkey.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl1.h"
#include "core/findfiles.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/files/allow.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/files/files.h"

#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

extern char str_quit[];

void move_file() {
  int d1 = 0;

  bool ok = false;
  bout.nl();
  auto fm = file_mask("|#2Filename to move: ");
  dliscan();
  int nCurRecNum = recno(fm);
  if (nCurRecNum < 0) {
    bout << "\r\nFile not found.\r\n";
    return;
  }
  bool done = false;
  tmp_disable_conf(true);

  while (!a()->hangup_ && nCurRecNum > 0 && !done) {
    int nCurrentPos = nCurRecNum;
    auto area = a()->current_file_area();
    auto f = area->ReadFile(nCurRecNum);
    bout.nl();
    printfileinfo(&f.u(), a()->current_user_dir().subnum);
    bout.nl();
    bout << "|#5Move this (Y/N/Q)? ";
    char ch = ynq();
    std::string src_fn;
    if (ch == 'Q') {
      done = true;
    } else if (ch == 'Y') {
      src_fn =
          StrCat(a()->directories[a()->current_user_dir().subnum].path, f.unaligned_filename());
      string ss;
      do {
        bout.nl(2);
        bout << "|#2To which directory? ";
        ss = mmkey(MMKeyAreaType::dirs);
        if (ss[0] == '?') {
          dirlist(1);
          dliscan();
        }
      } while (!a()->hangup_ && ss[0] == '?');
      d1 = -1;
      if (!ss.empty()) {
        for (size_t i1 = 0; i1 < a()->directories.size() && a()->udir[i1].subnum != -1; i1++) {
          if (ss == a()->udir[i1].keys) {
            d1 = i1;
          }
        }
      }
      if (d1 != -1) {
        ok = true;
        d1 = a()->udir[d1].subnum;
        dliscan1(d1);
        if (recno(f.aligned_filename()) > 0) {
          ok = false;
          bout << "\r\nFilename already in use in that directory.\r\n";
        }
        if (a()->current_file_area()->number_of_files() >= a()->directories[d1].maxfiles) {
          ok = false;
          bout << "\r\nToo many files in that directory.\r\n";
        }
        if (File::freespace_for_path(a()->directories[d1].path) <
            static_cast<long>(f.numbytes() / 1024L) + 3) {
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
        f.u().daten = daten_t_now();
      }
      --nCurrentPos;
      auto* area = a()->current_file_area();
      if (area->DeleteFile(nCurRecNum)) {
        area->Save();
      }
      string ss = area->ReadExtendedDescriptionAsString(f).value_or("");
      if (!ss.empty()) {
        area->DeleteExtendedDescription(f.aligned_filename());
      }

      dliscan1(d1);
      auto dest_fn = StrCat(a()->directories[d1].path, f.unaligned_filename());
      if (area->AddFile(f)) {
        area->Save();
      }
      if (!ss.empty()) {
        const auto pos = area->FindFile(f).value_or(-1);
        area->AddExtendedDescription(f, pos, ss);
      }
      StringRemoveWhitespace(&src_fn);
      StringRemoveWhitespace(&dest_fn);
      if (!iequals(src_fn, dest_fn) && File::Exists(src_fn)) {
        File::Rename(src_fn, dest_fn);
      }
      bout << "\r\nFile moved.\r\n";
    }
    dliscan();
    nCurRecNum = nrecno(fm, nCurrentPos);
  }

  tmp_disable_conf(false);
}

static wwiv::sdk::files::FileAreaSortType sort_type_from_wwiv_type(int t) {
  switch (t) {
    case 0: return files::FileAreaSortType::FILENAME_ASC;
    case 1: return files::FileAreaSortType::DATE_ASC;
    case 2: return files::FileAreaSortType::DATE_DESC;
    default: return files::FileAreaSortType::FILENAME_ASC;
  }  
}

void sortdir(int directory_num, int type) {
  dliscan1(directory_num);

  if (a()->current_file_area()->Sort(sort_type_from_wwiv_type(type))) {
    a()->current_file_area()->Save();
  }
}

void sort_all(int type) {
  tmp_disable_conf(true);
  for (size_t i = 0;
       i < a()->directories.size() && a()->udir[i].subnum != -1 && !a()->localIO()->KeyPressed();
       i++) {
    bout << "\r\n|#1Sorting " << a()->directories[a()->udir[i].subnum].name << wwiv::endl;
    sortdir(i, type);
  }
  tmp_disable_conf(false);
}

void rename_file() {
  char s[81], s1[81], s2[81], s3[81];

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
  while (nRecNum > 0 && !a()->hangup_) {
    int nCurRecNum = nRecNum;
    auto f = a()->current_file_area()->ReadFile(nRecNum);
    bout.nl();
    printfileinfo(&f.u(), a()->current_user_dir().subnum);
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
          strcat(s2, f.aligned_filename().c_str());
          StringRemoveWhitespace(s2);
          File::Rename(s2, s1);
          if (File::Exists(s1)) {
            auto* area = a()->current_file_area();
            string ss = area->ReadExtendedDescriptionAsString(f).value_or("");
            if (!ss.empty()) {
              area->DeleteExtendedDescription(f, nCurRecNum);
              area->AddExtendedDescription(s, ss);
              f.set_extended_description(true);
            }
            f.set_filename(s);
          } else {
            bout << "Bad filename.\r\n";
          }
        }
      }
    }
    bout << "\r\nNew description:\r\n|#2: ";
    auto desc = input_text(58);
    if (!desc.empty()) {
      f.set_description(desc);
    }
    auto* area = a()->current_file_area();
    string ss = area->ReadExtendedDescriptionAsString(f).value_or("");
    bout.nl(2);
    bout << "|#5Modify extended description? ";
    if (yesno()) {
      bout.nl();
      if (!ss.empty()) {
        bout << "|#5Delete it? ";
        if (yesno()) {
          area->DeleteExtendedDescription(f, nCurRecNum);
          f.set_extended_description(false);
        } else {
          f.set_extended_description(true);
          modify_extended_description(&ss, a()->directories[a()->current_user_dir().subnum].name);
          if (!ss.empty()) {
            area->DeleteExtendedDescription(f, nCurRecNum);
            area->AddExtendedDescription(f, nCurRecNum, ss);
          }
        }
      } else {
        modify_extended_description(&ss, a()->directories[a()->current_user_dir().subnum].name);
        if (!ss.empty()) {
          area->AddExtendedDescription(f, nCurRecNum, ss);
          f.set_extended_description(true);
        } else {
          f.set_extended_description(false);
        }
      }
    } else if (!ss.empty()) {
      f.set_extended_description(true);
    } else {
      f.set_extended_description(false);
    }
    if (a()->current_file_area()->UpdateFile(f, nRecNum)) {
      a()->current_file_area()->Save();
    }
    nRecNum = nrecno(s3, nCurRecNum);
  }
}

static bool upload_file(const std::string& file_name, uint16_t directory_num, const char* description) {
  auto d = a()->directories[directory_num];
  const auto temp_filename = aligns(file_name);
  wwiv::sdk::files::FileRecord f{};
  f.set_filename(temp_filename);
  f.u().ownerusr = static_cast<uint16_t>(a()->usernum);
  if (!(d.mask & mask_cdrom) && !check_ul_event(directory_num, &f.u())) {
    bout << file_name << " was deleted by upload event.\r\n";
  } else {
    const auto unaligned_filename = files::unalign(file_name);
    const auto full_path = PathFilePath(d.path, unaligned_filename);

    File fileUpload(full_path);
    if (!fileUpload.Open(File::modeBinary | File::modeReadOnly)) {
      if (description && (*description)) {
        bout << "ERR: " << unaligned_filename << ":" << description << wwiv::endl;
      } else {
        bout << "|#1" << unaligned_filename << " does not exist." << wwiv::endl;
      }
      return true;
    }
    auto fs = fileUpload.length();
    f.u().numbytes = static_cast<daten_t>(fs);
    fileUpload.Close();
    to_char_array(f.u().upby, a()->names()->UserName(a()->usernum));
    to_char_array(f.u().date, date());

    auto t = DateTime::from_time_t(File::creation_time(full_path)).to_string("%m/%d/%y");
    to_char_array(f.u().actualdate, t);

    if (d.mask & mask_PD) {
      d.mask = mask_PD;
    }
    bout.nl();

    bout << "|#9File name   : |#2" << f.unaligned_filename() << wwiv::endl;
    bout << "|#9File size   : |#2" << bytes_to_k(f.numbytes()) << wwiv::endl;
    if (description && *description) {
      f.set_description(description);
      bout << "|#1 Description: " << f.u().description << wwiv::endl;
    } else {
      bout << "|#9Enter a description for this file.\r\n|#7: ";
      auto desc = input_text(58);
      f.set_description(desc);
    }
    bout.nl();
    if (f.u().description[0] == 0) {
      return false;
    }
    get_file_idz(&f.u(), directory_num);
    a()->user()->SetFilesUploaded(a()->user()->GetFilesUploaded() + 1);
    if (!(d.mask & mask_cdrom)) {
      add_to_file_database(f.aligned_filename());
    }
    a()->user()->set_uk(a()->user()->uk() + bytes_to_k(fs));
    f.u().daten = daten_t_now();
    if (a()->current_file_area()->AddFile(f)) {
      a()->current_file_area()->Save();
    }
    auto status = a()->status_manager()->BeginTransaction();
    status->IncrementNumUploadsToday();
    status->IncrementFileChangedFlag(WStatus::fileChangeUpload);
    a()->status_manager()->CommitTransaction(std::move(status));
    sysoplog() << "+ '" << f.aligned_filename() << "' uploaded on " << d.name;
    a()->UpdateTopScreen();
  }
  return true;
}

bool maybe_upload(const std::string& file_name, uint16_t directory_num, const char* description) {
  bool abort = false;
  bool ok = true;
  int i = recno(aligns(file_name));

  if (i == -1) {
    if (a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH) && (!is_uploadable(file_name) && dcs())) {
      bout << fmt::format("{:<12}: |#5In filename database - add anyway? ", file_name);
      char ch = ynq();
      if (ch == *str_quit) {
        return false;
      }
      if (ch == *(YesNoString(false))) {
        bout << "|#5Delete it? ";
        if (yesno()) {
          File::Remove(PathFilePath(a()->directories[directory_num].path, file_name));
          bout.nl();
          return true;
        }
        return true;
      }
    }
    if (!upload_file(file_name, a()->udir[directory_num].subnum, description)) {
      ok = false;
    }
  } else {
    auto f = a()->current_file_area()->ReadFile(i);
    const auto ocd = a()->current_user_dir_num();
    a()->set_current_user_dir_num(directory_num);
    printinfo(&f.u(), &abort);
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
void upload_files(const char* file_name, uint16_t directory_num, int type) {
  char s[255], *fn1 = nullptr, *description = nullptr, last_fn[81], *ext = nullptr;
  bool abort = false;
  bool ok = true;

  last_fn[0] = 0;
  dliscan1(a()->udir[directory_num].subnum);

  auto file = std::make_unique<TextFile>(file_name, "r");
  if (!file->IsOpen()) {
    char szDefaultFileName[MAX_PATH];
    sprintf(szDefaultFileName, "%s%s", a()->directories[a()->udir[directory_num].subnum].path,
            file_name);
    file.reset(new TextFile(szDefaultFileName, "r"));
  }
  if (!file->IsOpen()) {
    bout << file_name << ": not found.\r\n";
  } else {
    while (ok && file->ReadLine(s, 250)) {
      if (s[0] < SPACE) {
        continue;
      }
      if (s[0] == SPACE) {
        if (last_fn[0]) {
          if (!ext) {
            ext = static_cast<char*>(BbsAllocA(4096L));
            *ext = 0;
          }
          for (description = s; (*description == ' ') || (*description == '\t'); description++)
            ;
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
        int ok1 = 0;
        fn1 = strtok(s, " \t\n");
        if (fn1) {
          ok1 = 1;
          for (int i = 0; ok1 && (i < type); i++) {
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
            auto f = a()->current_file_area()->ReadFile(1);
            if (iequals(last_fn, f.aligned_filename())) {
              add_to_file_database(f.aligned_filename());
              a()->current_file_area()->AddExtendedDescription(f, 1, ext);
              f.set_extended_description(true);
              if (a()->current_file_area()->UpdateFile(f, 1)) {
                a()->current_file_area()->Save();
              }
            }
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
      auto f = a()->current_file_area()->ReadFile(1);
      if (iequals(last_fn, f.aligned_filename())) {
        add_to_file_database(f.aligned_filename());
        a()->current_file_area()->AddExtendedDescription(f, 1, ext);
        f.set_extended_description(true);
        if (a()->current_file_area()->UpdateFile(f, 1)) {
          a()->current_file_area()->Save();
        }
      }
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
  sprintf(szPathName, "%s%s", a()->directories[a()->udir[directory_num].subnum].path,
          szDefaultFileSpec);
  int maxf = a()->directories[a()->udir[directory_num].subnum].maxfiles;

  bool ok = true;
  bool abort = false;
  FindFiles ff(szPathName, FindFilesType::files);
  for (const auto& f : ff) {
    if (checka() || a()->hangup_ || a()->current_file_area()->number_of_files() >= maxf) {
      break;
    }
    if (!maybe_upload(f.name, directory_num, nullptr)) {
      break;
    }
  }
  if (!ok || abort) {
    bout << "|#6Aborted.\r\n";
    ok = false;
  }
  if (a()->current_file_area()->number_of_files() >= maxf) {
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
    sprintf(s, "%c%d%2d%c%d%c", 0x03, check_batch_queue(f.u.filename) ? 6 : 0, i + 1, 0x03,
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

    sprintf(s1,
            "%ld"
            "k",
            bytes_to_k(f.u.numbytes));
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
    sprintf(s, "%c%d%s", 0x03, (f.u.mask & mask_extended) ? 1 : 2, f.u.description);
    bout.bpla(trim_to_size_ignore_colors(s, a()->user()->GetScreenChars() - 28), &abort);
  }
  bout.Color(FRAME_COLOR);
  bout << "\r" << string(78, '-') << wwiv::endl;
  bout.clear_lines_listed();
}

/*
 * Allows user to add or remove ALLOW.DAT entries.
 */
void edit_database() {
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
        add_to_file_database(s);
      }
      break;
    case 'R':
      bout.nl();
      bout << "|#2Filename: ";
      input(s, 12, true);
      if (s[0]) {
        remove_from_file_database(s);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!a()->hangup_ && !done);
}

#define ALLOW_BUFSIZE 61440

void add_to_file_database(const std::string& file_name) {
  if (!a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return;
  }
  wwiv::sdk::files::Allow allow(*a()->config());
  allow.Add(file_name);
  allow.Save();
}

void remove_from_file_database(const std::string& file_name) {
  if (!a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return;
  }
  wwiv::sdk::files::Allow allow(*a()->config());
  allow.Remove(file_name);
  allow.Save();
}

/*
 * Returns 1 if file not found in filename database.
 */

bool is_uploadable(const std::string& file_name) {
  if (!a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
    return true;
  }
  wwiv::sdk::files::Allow allow(*a()->config());
  return allow.IsAllowed(file_name);
}

static void l_config_nscan() {
  char s[81], s2[81];

  bool abort = false;
  bout.nl();
  bout << "|#9Directories to new-scan marked with '|#2*|#9'\r\n\n";
  for (size_t i = 0; (i < a()->directories.size()) && (a()->udir[i].subnum != -1) && (!abort);
       i++) {
    size_t i1 = a()->udir[i].subnum;
    if (a()->context().qsc_n[i1 / 32] & (1L << (i1 % 32))) {
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
    config_scan_plus(NSCAN); // ZU - SCONFIG
    return;                  // ZU - SCONFIG
  }                          // ZU - SCONFIG

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
      if (okconf(a()->user()) && a()->dirconfs.size() > 1) {
        size_t i = 0;
        while (ch != a()->dirconfs[a()->uconfdir[i].confnum].designator &&
               i < a()->dirconfs.size()) {
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
        auto s = mmkey(MMKeyAreaType::dirs);
        if (s[0]) {
          for (size_t i = 0; i < a()->directories.size(); i++) {
            i1 = a()->udir[i].subnum;
            if (s == a()->udir[i].keys) {
              a()->context().qsc_n[i1 / 32] ^= 1L << (i1 % 32);
            }
            if (s == "S") {
              a()->context().qsc_n[i1 / 32] |= 1L << (i1 % 32);
            }
            if (s == "C") {
              a()->context().qsc_n[i1 / 32] &= ~(1L << (i1 % 32));
            }
          }
          if (s == "Q") {
            done = true;
          }
          if (s == "?") {
            l_config_nscan();
          }
        }
      } while (!done && !a()->hangup_);
      break;
    }
    if (!okconf(a()->user()) || a()->uconfdir[1].confnum == -1) {
      done1 = true;
    }
  } while (!done1 && !a()->hangup_);

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
    bout << "|#7[|#23|#7]|#1 New-Scan Transfer after Message Base ("
         << YesNoString(a()->user()->IsNewScanFiles()) << ").\r\n";
    bout << "|#7[|#24|#7]|#1 Number of lines of extended description to print ["
         << a()->user()->GetNumExtended() << " line(s)].\r\n";
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
      bout << "|#9do you want to see when listing files (|#20-" << a()->max_extend_lines
           << "|#7)\r\n";
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
  } while (!done && !a()->hangup_);
}

void finddescription() {
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
  int count = 0;
  int color = 3;
  bout << "\r|#2Searching ";
  bout.clear_lines_listed();
  for (uint16_t i = 0;
       (i < a()->directories.size()) && !abort && !a()->hangup_ && (a()->udir[i].subnum != -1);
       i++) {
    auto ii1 = a()->udir[i].subnum;
    int pts;
    bool need_title = true;
    if (a()->context().qsc_n[ii1 / 32] & (1L << (ii1 % 32))) {
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
      for (auto i1 = 1;
           i1 <= a()->current_file_area()->number_of_files() && !abort && !a()->hangup_; i1++) {
        auto f = a()->current_file_area()->ReadFile(i1);
        strcpy(s, f.u().description);
        for (int i2 = 0; i2 < ssize(s); i2++) {
          s[i2] = upcase(s[i2]);
        }
        if (strstr(s, s1) != nullptr) {

          if (need_title) {
            if (bout.lines_listed() >= a()->screenlinest - 7 && !a()->filelist.empty()) {
              tag_files(need_title);
            }
            if (need_title) {
              printtitle(&abort);
              need_title = false;
            }
          }

          printinfo(&f.u(), &abort);
        } else if (bkbhit()) {
          checka(&abort);
        }
      }
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
      auto f = a()->current_file_area()->ReadFile(nRecordNum);
      int i1 =
          list_arc_out(stripfn(f.unaligned_filename()), a()->directories[a()->current_user_dir().subnum].path);
      if (i1) {
        abort = true;
      }
      checka(&abort);
      nRecordNum = nrecno(szFileSpec, nRecordNum);
    }
  } while (nRecordNum > 0 && !a()->hangup_ && !abort);
}
