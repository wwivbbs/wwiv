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

#include "bbs/xfertmp.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/dirlist.h"
#include "bbs/input.h"
#include "bbs/mmkey.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "core/numbers.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/files.h"
#include <functional>
#include <string>
#include <vector>

using std::function;
using std::string;
using std::vector;
using wwiv::sdk::files::FileName;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void move_file_t() {
  int d1 = -1;

  tmp_disable_conf(true);

  bout.nl();
  if (a()->batch().empty()) {
    bout.nl();
    bout << "|#6No files have been tagged for movement.\r\n";
    pausescr();
  }
  // TODO(rushfan): rewrite using iterators.
  for (auto pos = a()->batch().ssize() - 1; pos >= 0; pos--) {
    bool ok;
    auto cur_batch_fn = aligns(a()->batch().entry[pos].aligned_filename());
    dliscan1(a()->batch().entry[pos].dir());
    int temp_record_num = recno(cur_batch_fn);
    if (temp_record_num < 0) {
      bout << "File not found.\r\n";
      pausescr();
    }
    while (!a()->context().hangup() && temp_record_num > 0) {
      auto cur_pos = temp_record_num;
      auto f = a()->current_file_area()->ReadFile(temp_record_num);
      const auto& dir = a()->dirs()[a()->batch().entry[pos].dir()];
      printfileinfo(&f.u(), dir);
      bout << "|#5Move this (Y/N/Q)? ";
      const auto ch = ynq();
      if (ch == 'Q') {
        tmp_disable_conf(false);
        dliscan();
        return;
      }
      std::filesystem::path s1;
      if (ch == 'Y') {
        s1 = FilePath(dir.path, f);
        string dirnum;
        do {
          bout << "|#2To which directory? ";
          dirnum = mmkey(MMKeyAreaType::dirs);
          if (dirnum.front() == '?') {
            dirlist(1);
            dliscan1(a()->batch().entry[pos].dir());
          }
        }
        while (!a()->context().hangup() && (dirnum.front() == '?'));
        d1 = -1;
        if (!dirnum.empty()) {
          for (auto i1 = 0; i1 < a()->dirs().size() && a()->udir[i1].subnum != -1; i1++) {
            if (dirnum == a()->udir[i1].keys) {
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
            bout << "Filename already in use in that directory.\r\n";
          }
          if (a()->current_file_area()->number_of_files() >= a()->dirs()[d1].maxfiles) {
            ok = false;
            bout << "Too many files in that directory.\r\n";
          }
          if (File::freespace_for_path(a()->dirs()[d1].path) <
              static_cast<long>(f.numbytes() / 1024L) + 3) {
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
      if (ok) {
        bout << "|#5Reset upload time for file? ";
        if (yesno()) {
          f.set_date(DateTime::now());
        }
        --cur_pos;
        auto ext_desc = a()->current_file_area()->ReadExtendedDescriptionAsString(f);
        if (a()->current_file_area()->DeleteFile(f, temp_record_num)) {
          a()->current_file_area()->Save();
        }
        auto s2 = FilePath(a()->dirs()[d1].path, f);
        dliscan1(d1);
        // N.B. the current file area changes with calls to dliscan*
        if (a()->current_file_area()->AddFile(f)) {
          a()->current_file_area()->Save();
        }
        if (ext_desc) {
          const auto fpos = a()->current_file_area()->FindFile(f).value_or(-1);
          a()->current_file_area()->AddExtendedDescription(f, fpos, ext_desc.value());
        }
        if (s1 != s2 && File::Exists(s1)) {
          File::Rename(s1, s2);
          remlist(a()->batch().entry[pos].aligned_filename());
          didnt_upload(a()->batch().entry[pos]);
          a()->batch().delbatch(pos);
        }
        bout << "File moved.\r\n";
      }
      dliscan();
      temp_record_num = nrecno(cur_batch_fn, cur_pos);
    }
  }
  tmp_disable_conf(false);
}

void removefile() {
  User uu{};

  dliscan();
  bout.nl();
  bout << "|#9Enter filename to remove.\r\n:";
  auto remove_fn = input(12, true);
  if (remove_fn.empty()) {
    return;
  }
  if (strchr(remove_fn.c_str(), '.') == nullptr) {
    remove_fn += ".*";
  }
  remove_fn = aligns(remove_fn);
  auto record_num = recno(remove_fn);
  auto abort = false;
  while (!a()->context().hangup() && record_num > 0 && !abort) {
    auto f = a()->current_file_area()->ReadFile(record_num);
    if (dcs() || (f.u().ownersys == 0 && f.u().ownerusr == a()->usernum)) {
      const auto& dir = a()->dirs()[a()->current_user_dir().subnum];
      bout.nl();
      if (a()->batch().contains_file(f.filename())) {
        bout << "|#6That file is in the batch queue; remove it from there.\r\n\n";
      } else {
        printfileinfo(&f.u(), dir);
        bout << "|#9Remove (|#2Y/N/Q|#9) |#0: |#2";
        auto ch = ynq();
        if (ch == 'Q') {
          abort = true;
        } else if (ch == 'Y') {
          bool bRemoveDlPoints = true;
          bool bDeleteFileToo = false;
          if (dcs()) {
            bout << "|#5Delete file too? ";
            bDeleteFileToo = yesno();
            if (bDeleteFileToo && (f.u().ownersys == 0)) {
              bout << "|#5Remove DL points? ";
              bRemoveDlPoints = yesno();
            }
            bout.nl();
            bout << "|#5Remove from ALLOW.DAT? ";
            if (yesno()) {
              remove_from_file_database(f.aligned_filename());
            }
          } else {
            bDeleteFileToo = true;
            remove_from_file_database(f.aligned_filename());
          }
          if (bDeleteFileToo) {
            auto del_fn = FilePath(dir.path, f);
            File::Remove(del_fn);
            if (bRemoveDlPoints && f.u().ownersys == 0) {
              a()->users()->readuser(&uu, f.u().ownerusr);
              if (!uu.IsUserDeleted()) {
                if (date_to_daten(uu.GetFirstOn()) < f.u().daten) {
                  uu.SetFilesUploaded(uu.GetFilesUploaded() - 1);
                  uu.set_uk(uu.uk() - bytes_to_k(f.numbytes()));
                  a()->users()->writeuser(&uu, f.u().ownerusr);
                }
              }
            }
          }
          sysoplog() << fmt::format("- \"{}\" removed off of {}", f, dir.name);
          if (a()->current_file_area()->DeleteFile(f, record_num)) {
            a()->current_file_area()->Save();
            --record_num;
          }
        }
      }
    }
    record_num = nrecno(remove_fn, record_num);
  }
}
