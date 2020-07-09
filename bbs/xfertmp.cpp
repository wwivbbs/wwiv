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

static bool bad_filename(const std::string& fn) {
  bout << fn;
  return true;
}


struct arch {
  unsigned char type;
  char name[13];
  int32_t len;
  int16_t date, time, crc;
  int32_t size;
};

static bool check_for_files_arc(const std::filesystem::path& file_path) {
  File file(file_path);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    arch a{};
    const auto file_size = file.length();
    long pos = 1;
    file.Seek(0, File::Whence::begin);
    file.Read(&a, 1);
    if (a.type != 26) {
      bout << file_path.filename().string() << " is not a valid .ARC file.";
      return false;
    }
    while (pos < file_size) {
      file.Seek(pos, File::Whence::begin);
      const auto num_read = file.Read(&a, sizeof(arch));
      if (num_read == sizeof(arch)) {
        pos += sizeof(arch);
        if (a.type == 1) {
          pos -= 4;
          a.size = a.len;
        }
        if (a.type) {
          pos += a.len;
          ++pos;
          auto arc_fn = ToStringUpperCase(trim_to_size(a.name, 13));
          if (bad_filename(arc_fn)) {
            return false;
          }
        } else {
          pos = file_size;
        }
      } else {
        if (a.type != 0) {
          bout << file_path.filename().string() << " is not a valid .ARC file.";
          return false;
        }
        pos = file_size;
      }
    }

    file.Close();
    return true;
  }
  bout << "File not found: " << file_path.filename().string() << wwiv::endl;
  return false;
}

// .ZIP structures and defines
#define ZIP_LOCAL_SIG 0x04034b50
#define ZIP_CENT_START_SIG 0x02014b50
#define ZIP_CENT_END_SIG 0x06054b50

struct zip_local_header {
  uint32_t signature; // 0x04034b50
  uint16_t extract_ver;
  uint16_t flags;
  uint16_t comp_meth;
  uint16_t mod_time;
  uint16_t mod_date;
  uint32_t crc_32;
  uint32_t comp_size;
  uint32_t uncomp_size;
  uint16_t filename_len;
  uint16_t extra_length;
};

struct zip_central_dir {
  uint32_t signature; // 0x02014b50
  uint16_t made_ver;
  uint16_t extract_ver;
  uint16_t flags;
  uint16_t comp_meth;
  uint16_t mod_time;
  uint16_t mod_date;
  uint32_t crc_32;
  uint32_t comp_size;
  uint32_t uncomp_size;
  uint16_t filename_len;
  uint16_t extra_len;
  uint16_t comment_len;
  uint16_t disk_start;
  uint16_t int_attr;
  uint32_t ext_attr;
  uint32_t rel_ofs_header;
};

struct zip_end_dir {
  uint32_t signature; // 0x06054b50
  uint16_t disk_num;
  uint16_t cent_dir_disk_num;
  uint16_t total_entries_this_disk;
  uint16_t total_entries_total;
  uint32_t central_dir_size;
  uint32_t ofs_cent_dir;
  uint16_t comment_len;
};

bool check_for_files_zip(const std::filesystem::path& path) {
  zip_local_header zl{};
  zip_central_dir zc{};
  zip_end_dir ze{};
  char s[MAX_PATH];

  const auto fn = path.filename().string();
  File file(path);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    long l = 0;
    const auto len = file.length();
    while (l < len) {
      long sig = 0;
      file.Seek(l, File::Whence::begin);
      file.Read(&sig, 4);
      file.Seek(l, File::Whence::begin);
      switch (sig) {
      case ZIP_LOCAL_SIG:
        file.Read(&zl, sizeof(zl));
        file.Read( s, zl.filename_len );
        s[ zl.filename_len ] = '\0';
        strupr(s);
        if (bad_filename(s)) {
          return false;
        }
        l += sizeof(zl);
        l += zl.comp_size + zl.filename_len + zl.extra_length;
        break;
      case ZIP_CENT_START_SIG:
        file.Read(&zc, sizeof(zc));
        file.Read( s, zl.filename_len ); s[ zl.filename_len ] = '\0';
        strupr(s);
        if (bad_filename(s)) {
          return false;
        }
        l += sizeof(zc);
        l += zc.filename_len + zc.extra_len;
        break;
      case ZIP_CENT_END_SIG:
        file.Read(&ze, sizeof(ze));
        return true;
      default:
        bout << "Error examining that; can't extract from it.\r\n";
        return false;
      }
    }
    file.Close();
    return true;
  }
  bout << "File not found: " << fn << wwiv::endl;
  return false;
}


struct lharc_header {
  unsigned char checksum;
  char ctype[5];
  long comp_size;
  long uncomp_size;
  unsigned short time;
  unsigned short date;
  unsigned short attr;
  unsigned char fn_len;
};

bool check_for_files_lzh(const std::filesystem::path& path) {
  lharc_header a{};

  const auto fn = path.filename().string();
  File file(path);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "File not found: " << fn << wwiv::endl;
    return false;
  }
  const auto file_size = file.length();
  unsigned short nCrc;
  for (long l = 0; l < file_size; l += a.fn_len + a.comp_size +
                                       static_cast<long>(sizeof(lharc_header)) +
                                       file.Read(&nCrc, sizeof(nCrc)) + 1) {
    file.Seek(l, File::Whence::begin);
    char flag;
    file.Read(&flag, 1);
    if (!flag) {
      break;
    }
    auto num_read = file.Read(&a, sizeof(lharc_header));
    if (num_read != sizeof(lharc_header)) {
      bout << fn << " is not a valid .LZH file.";
      return false;
    }
    char buffer[256];
    num_read = file.Read(buffer, a.fn_len);
    if (num_read != a.fn_len) {
      bout << fn << " is not a valid .LZH file.";
      return false;
    }
    buffer[a.fn_len] = '\0';
    strupr(buffer);
    if (bad_filename(buffer)) {
      return false;
    }
  }
  return true;
}

bool check_for_files_arj(const std::filesystem::path& path) {
  const auto fn = path.filename().string();
  File file(path);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    const auto file_size = file.length();
    long lCurPos = 0;
    file.Seek(0L, File::Whence::begin);
    while (lCurPos < file_size) {
      file.Seek(lCurPos, File::Whence::begin);
      uint16_t sh;
      const auto num_read = file.Read(&sh, 2);
      if (num_read != 2 || sh != 0xea60) {
        bout << fn << " is not a valid .ARJ file.";
        return false;
      }
      lCurPos += num_read + 2;
      file.Read(&sh, 2);
      unsigned char s1;
      file.Read(&s1, 1);
      file.Seek(lCurPos + 12, File::Whence::begin);
      long l2;
      file.Read(&l2, 4);
      file.Seek(lCurPos + static_cast<long>(s1), File::Whence::begin);
      char buffer[256];
      file.Read(buffer, 250);
      buffer[250] = '\0';
      if (strlen(buffer) > 240) {
        file.Close();
        bout << fn << " is not a valid .ARJ file.";
        return false;
      }
      lCurPos += 4 + static_cast<long>(sh);
      file.Seek(lCurPos, File::Whence::begin);
      file.Read(&sh, 2);
      lCurPos += 2;
      while ((lCurPos < file_size) && sh) {
        lCurPos += 6 + static_cast<long>(sh);
        file.Seek(lCurPos - 2, File::Whence::begin);
        file.Read(&sh, 2);
      }
      lCurPos += l2;
      strupr(buffer);
      if (bad_filename(buffer)) {
        file.Close();
        return false;
      }
    }
    file.Close();
    return true;
  }

  bout << "File not found: " << fn;
  return false;
}

static bool check_for_files(const std::filesystem::path& path) {
  struct arc_testers {
    const std::string arc_name;
    function<int(const std::filesystem::path&)> func;
  };

  static const vector<arc_testers> arc_t = {
      {"ZIP", check_for_files_zip},
      {"ARC", check_for_files_arc},
      {"LZH", check_for_files_lzh},
      {"ARJ", check_for_files_arj},
  };

  if (!path.has_extension()) {
    // no extension?
    bout << "No extension.\r\n";
    return true;
  }
  auto ext = ToStringUpperCase(path.extension().string());
  if (ext.front() == '.') {
    // trim leading . for extension
    ext = ext.substr(1);
  }
  for (const auto& t : arc_t) {
    if (iequals(ext, t.arc_name)) {
      return t.func(path) == 0;
    }
  }
  return false;
}

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
    while (!a()->hangup_ && temp_record_num > 0) {
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
        while (!a()->hangup_ && (dirnum.front() == '?'));
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
  while (!a()->hangup_ && record_num > 0 && !abort) {
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
