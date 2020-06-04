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
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/dirlist.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/mmkey.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xfer_common.h"
#include "bbs/xferovl.h"
#include "core/findfiles.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/files.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// the archive type to use
static const int ARC_NUMBER = 0;

using std::function;
using std::string;
using std::vector;
using wwiv::sdk::files::FileName;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

bool bad_filename(const char* file_name) {
  // strings not to allow in a .zip file to extract from
  static const vector<string> bad_words = {
      "COMMAND",
      "..",
      "CMD",
      "4DOS",
      "4OS2",
      "4NT",
      "PKZIP",
      "PKUNZIP",
      "ARJ",
      "RAR",
      "LHA",
      "LHARC",
      "PKPAK",
  };

  for (const auto& bad_word : bad_words) {
    if (strstr(file_name, bad_word.c_str())) {
      bout << "Can't extract from that because it has " << file_name << wwiv::endl;
      return true;
    }
  }
  if (!okfn(file_name)) {
    bout << "Can't extract from that because it has " << file_name << wwiv::endl;
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

static bool check_for_files_arc(const std::filesystem::path& file_path) {
  File file(file_path);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    arch a{};
    const auto file_size = file.length();
    long lFilePos = 1;
    file.Seek(0, File::Whence::begin);
    file.Read(&a, 1);
    if (a.type != 26) {
      file.Close();
      bout << file_path.filename().string() << " is not a valid .ARC file.";
      return false;
    }
    while (lFilePos < file_size) {
      file.Seek(lFilePos, File::Whence::begin);
      const auto num_read = file.Read(&a, sizeof(arch));
      if (num_read == sizeof(arch)) {
        lFilePos += sizeof(arch);
        if (a.type == 1) {
          lFilePos -= 4;
          a.size = a.len;
        }
        if (a.type) {
          lFilePos += a.len;
          ++lFilePos;
          char szArcFileName[MAX_PATH];
          strncpy(szArcFileName, a.name, 13);
          szArcFileName[13] = 0;
          strupr(szArcFileName);
          if (bad_filename(szArcFileName)) {
            file.Close();
            return false;
          }
        } else {
          lFilePos = file_size;
        }
      } else {
        file.Close();
        if (a.type != 0) {
          bout << file_path.filename().string() << " is not a valid .ARC file.";
          return false;
        }
        lFilePos = file_size;
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
  zip_local_header zl;
  zip_central_dir zc;
  zip_end_dir ze;
  char s[MAX_PATH];

#define READ_FN( ln ) { file.Read( s, ln ); s[ ln ] = '\0'; }

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
        READ_FN(zl.filename_len);
        strupr(s);
        if (bad_filename(s)) {
          file.Close();
          return false;
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
          return false;
        }
        l += sizeof(zc);
        l += zc.filename_len + zc.extra_len;
        break;
      case ZIP_CENT_END_SIG:
        file.Read(&ze, sizeof(ze));
        file.Close();
        return true;
      default:
        file.Close();
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
  for (long l = 0; l < file_size;
       l += a.fn_len + a.comp_size + sizeof(lharc_header) + file.Read(&nCrc, sizeof(nCrc)) + 1) {
    file.Seek(l, File::Whence::begin);
    char flag;
    file.Read(&flag, 1);
    if (!flag) {
      l = file_size;
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
  file.Close();
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
      unsigned short sh;
      const int num_read = file.Read(&sh, 2);
      if (num_read != 2 || sh != 0xea60) {
        file.Close();
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
    const char* arc_name;
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

static bool download_temp_arc(const char* file_name, bool count_against_xfer_ratio) {
  bout << "Downloading " << file_name << "." << a()->arcs[ARC_NUMBER].extension << ":\r\n\r\n";
  if (count_against_xfer_ratio && !ratio_ok()) {
    bout << "Ratio too low.\r\n";
    return false;
  }
  const auto file_to_send = StrCat(file_name, ".", a()->arcs[ARC_NUMBER].extension);
  const auto dl_filename = PathFilePath(a()->temp_directory(), file_to_send);
  File file(dl_filename);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "No such file.\r\n\n";
    return false;
  }
  auto file_size = file.length();
  file.Close();
  if (file_size == 0L) {
    bout << "File has nothing in it.\r\n\n";
    return false;
  }
  double d = XFER_TIME(file_size);
  if (d <= nsl()) {
    bout << "Approx. time: " << ctim(std::lround(d)) << wwiv::endl;
    bool sent = false;
    bool abort = false;
    send_file(dl_filename.string(), &sent, &abort, file_to_send, -1, file_size);
    if (sent) {
      if (count_against_xfer_ratio) {
        a()->user()->SetFilesDownloaded(a()->user()->GetFilesDownloaded() + 1);
        a()->user()->set_dk(a()->user()->dk() + bytes_to_k(file_size));
        bout.nl(2);
        bout << fmt::sprintf("Your ratio is now: %-6.3f\r\n", ratio());
      }
      sysoplog() << "Downloaded " << bytes_to_k(file_size) << " of '" << file_to_send << "'";
      if (a()->IsUserOnline()) {
        a()->UpdateTopScreen();
      }
      return true;
    }
  } else {
    bout.nl(2);
    bout << "Not enough time left to D/L.\r\n\n";
  }
  return false;
}

void add_arc(const char* arc, const char* file_name, int dos) {
  const auto arc_fn = fmt::format("{}.{}", arc, a()->arcs[ARC_NUMBER].extension);
  // TODO - This logic is still broken since chain.* and door.* won't match
  if (iequals(file_name, DROPFILE_CHAIN_TXT) ||
      iequals(file_name, "door.sys") ||
      iequals(file_name, "chain.*") ||
      iequals(file_name, "door.*")) {
    return;
  }

  const auto arc_cmd = get_arc_cmd(arc_fn, 2, file_name);
  if (!arc_cmd.empty()) {
    File::set_current_directory(a()->temp_directory());
    a()->localIO()->Puts(arc_cmd);
    a()->localIO()->Puts("\r\n");
    if (dos) {
      ExecuteExternalProgram(arc_cmd, a()->spawn_option(SPAWNOPT_ARCH_A));
    } else {
      ExecuteExternalProgram(arc_cmd, EFLAG_NONE);
      a()->UpdateTopScreen();
    }
    a()->CdHome();
    sysoplog() << fmt::format("Added \"{}\" to {}", file_name, arc_fn);
  } else {
    bout << "Sorry, can't add to temp archive.\r\n\n";
  }
}

void add_temp_arc() {
  bout.nl();
  bout << "|#7Enter filename to add to temporary archive file.  May contain wildcards.\r\n|#7:";
  auto input_mask = input(12);
  if (!okfn(input_mask)) {
    return;
  }
  if (input_mask.empty()) {
    return;
  }
  if (strchr(input_mask.c_str(), '.') == nullptr) {
    input_mask += ".*";
  }
  const auto file_mask = stripfn(input_mask);
  for (auto c : file_mask) {
    if (c == '|' || c == '>' || c == '<' || c == ';' || c == ' ' || c == ':' || c == '/' ||
        c == '\\') {
      return;
    }
  }
  add_arc("temp", file_mask.c_str(), 1);
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
    remove_from_temp(szFileName, a()->temp_directory(), true);
  }
}

void list_temp_dir() {
  FindFiles ff(PathFilePath(a()->temp_directory(), "*"), FindFilesType::any);
  bout.nl();
  bout << "Files in temporary directory:\r\n\n";
  for (const auto& f : ff) {
    CheckForHangup();
    if (checka()) { break; }
    if (iequals(f.name, DROPFILE_CHAIN_TXT) || iequals(f.name, "door.sys")) {
      continue;
    }
    auto filename = aligns(f.name);
    bout.bputs(fmt::sprintf("%12s  %-8ld", filename, f.size));
  }
  if (ff.empty()) {
    bout << "None.\r\n";
  }
  bout.nl();
  bout << "Free space: " << File::freespace_for_path(a()->temp_directory()) << wwiv::endl;
  bout.nl();
}

void temp_extract() {
  dliscan();
  bout.nl();
  bout << "Extract to temporary directory:\r\n\n";
  bout << "|#2Filename: ";
  auto s = input(12);
  if (!okfn(s) || s.empty()) {
    return;
  }
  if (strchr(s.c_str(), '.') == nullptr) {
    s += ".*";
  }
  s = aligns(s);
  int i = recno(s);
  bool ok = true;
  while (i > 0 && ok && !a()->hangup_) {
    auto f = a()->current_file_area()->ReadFile(i);
    auto tmppath = PathFilePath(a()->directories[a()->current_user_dir().subnum].path, f);
    if (a()->directories[a()->current_user_dir().subnum].mask & mask_cdrom) {
      auto curpath = PathFilePath(a()->directories[a()->current_user_dir().subnum].path, f);
      tmppath = PathFilePath(a()->temp_directory(), f);
      if (!File::Exists(tmppath)) {
        File::Copy(curpath, tmppath);
      }
    }
    auto curpath = get_arc_cmd(tmppath.string(), 1, "");
    if (!curpath.empty() && File::Exists(tmppath)) {
      bout.nl(2);
      bool abort = false;
      printinfo(&f.u(), &abort);
      bout.nl();
      if (a()->directories[a()->current_user_dir().subnum].mask & mask_cdrom) {
        File::set_current_directory(a()->temp_directory());
      } else {
        File::set_current_directory(a()->directories[a()->current_user_dir().subnum].path);
      }
      File file(PathFilePath(File::current_directory(), f));
      a()->CdHome();
      if (check_for_files(file.path())) {
        bool ok1;
        do {
          bout << "|#2Extract what (?=list,Q=abort) ? ";
          auto extract_fn = input(12);
          ok1 = !extract_fn.empty();
          if (!okfn(extract_fn)) {
            ok1 = false;
          }
          if (iequals(extract_fn, "?")) {
            list_arc_out(f.unaligned_filename(),
                         a()->directories[a()->current_user_dir().subnum].path);
            extract_fn.clear();
          }
          if (iequals(extract_fn, "Q")) {
            ok = false;
            extract_fn.clear();
          }
          int i2 = 0;
          for (int i1 = 0; i1 < wwiv::stl::ssize(extract_fn); i1++) {
            if (extract_fn[i1] == '|' || extract_fn[i1] == '>' || extract_fn[i1] == '<' ||
                extract_fn[i1] == ';' || extract_fn[i1] == ' ') {
              i2 = 1;
            }
          }
          if (i2) {
            extract_fn.clear();
          }
          if (!extract_fn.empty()) {
            if (strchr(extract_fn.c_str(), '.') == nullptr) {
              extract_fn += ".*";
            }
            auto extract_cmd = get_arc_cmd(file.full_pathname(), 1, stripfn(extract_fn));
            File::set_current_directory(a()->temp_directory());
            if (!okfn(extract_fn)) {
              extract_cmd.clear();
            }
            if (!extract_cmd.empty()) {
              ExecuteExternalProgram(extract_cmd, a()->spawn_option(SPAWNOPT_ARCH_E));
              sysoplog() << fmt::format("Extracted out \"{}\" from \"{}\"", extract_fn,
                                        f.aligned_filename());
            }
            a()->CdHome();
          }
        }
        while (!a()->hangup_ && ok && ok1);
      }
    } else {
      bout.nl();
      bout << "That file currently isn't there.\r\n\n";
    }
    if (ok) {
      i = nrecno(s, i);
    }
  }
}

void list_temp_text() {
  bout.nl();
  bout << "|#2List what file(s) : ";
  auto fn = input(12, true);
  if (!okfn(fn)) {
    return;
  }
  if (!fn.empty()) {
    if (!contains(fn, '.')) {
      fn += ".*";
    }
    const auto fmask = PathFilePath(a()->temp_directory(), stripfn(fn));
    FindFiles ff(fmask, FindFilesType::any);
    bout.nl();
    for (const auto& f : ff) {
      const auto s = PathFilePath(a()->temp_directory(), f.name);
      if (iequals(f.name, "door.sys") || iequals(f.name, DROPFILE_CHAIN_TXT)) {
        continue;
      }
      bout.nl();
      bout << "Listing " << f.name << "\r\n\n";
      bool sent;
      double percent;
      ascii_send(s.string(), &sent, &percent);
      if (sent) {
        sysoplog() << "Temp text D/L \"" << f.name << "\"";
      } else {
        sysoplog() << "Temp Tried text D/L \"" << f.name << "\"" << (percent * 100.0) << "%";
        break;
      }
    }
  }
}


void list_temp_arc() {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "temp.%s", a()->arcs[ARC_NUMBER].extension);
  list_arc_out(szFileName, a()->temp_directory().c_str());
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
      print_help_file(TARCHIVE_NOEXT);
      break;
    }
  }
  while (!a()->hangup_);
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
  for (int nCurBatchPos = a()->batch().size() - 1; nCurBatchPos >= 0; nCurBatchPos--) {
    bool ok;
    auto cur_batch_fn = aligns(a()->batch().entry[nCurBatchPos].filename);
    dliscan1(a()->batch().entry[nCurBatchPos].dir);
    int nTempRecordNum = recno(cur_batch_fn);
    if (nTempRecordNum < 0) {
      bout << "File not found.\r\n";
      pausescr();
    }
    int nCurPos = 0;
    while (!a()->hangup_ && nTempRecordNum > 0) {
      nCurPos = nTempRecordNum;
      auto f = a()->current_file_area()->ReadFile(nTempRecordNum);
      printfileinfo(&f.u(), a()->batch().entry[nCurBatchPos].dir);
      bout << "|#5Move this (Y/N/Q)? ";
      const char ch = ynq();
      if (ch == 'Q') {
        tmp_disable_conf(false);
        dliscan();
        return;
      }
      std::filesystem::path s1;
      if (ch == 'Y') {
        s1 = PathFilePath(a()->directories[a()->batch().entry[nCurBatchPos].dir].path, f);
        string dirnum;
        do {
          bout << "|#2To which directory? ";
          dirnum = mmkey(MMKeyAreaType::dirs);
          if (dirnum.front() == '?') {
            dirlist(1);
            dliscan1(a()->batch().entry[nCurBatchPos].dir);
          }
        }
        while (!a()->hangup_ && (dirnum.front() == '?'));
        d1 = -1;
        if (!dirnum.empty()) {
          for (size_t i1 = 0; i1 < a()->directories.size() && (a()->udir[i1].subnum != -1); i1++) {
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
          if (a()->current_file_area()->number_of_files() >= a()->directories[d1].maxfiles) {
            ok = false;
            bout << "Too many files in that directory.\r\n";
          }
          if (File::freespace_for_path(a()->directories[d1].path) <
              static_cast<long>(f.u().numbytes / 1024L) + 3) {
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
          f.u().daten = daten_t_now();
        }
        --nCurPos;
        auto ext_desc = a()->current_file_area()->ReadExtendedDescriptionAsString(f);
        if (a()->current_file_area()->DeleteFile(f, nTempRecordNum)) {
          a()->current_file_area()->Save();
        }
        auto s2 = PathFilePath(a()->directories[d1].path, f);
        dliscan1(d1);
        // N.B. the current file area changes with calls to dliscan*
        if (a()->current_file_area()->AddFile(f)) {
          a()->current_file_area()->Save();
        }
        if (ext_desc) {
          const auto pos = a()->current_file_area()->FindFile(f).value_or(-1);
          a()->current_file_area()->AddExtendedDescription(f, pos, ext_desc.value());
        }
        if (s1 != s2 && File::Exists(s1)) {
          File::Rename(s1, s2);
          remlist(a()->batch().entry[nCurBatchPos].filename);
          didnt_upload(a()->batch().entry[nCurBatchPos]);
          a()->batch().delbatch(nCurBatchPos);
        }
        bout << "File moved.\r\n";
      }
      dliscan();
      nTempRecordNum = nrecno(cur_batch_fn, nCurPos);
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
  int i = recno(remove_fn);
  bool abort = false;
  while (!a()->hangup_ && (i > 0) && !abort) {
    auto f = a()->current_file_area()->ReadFile(i);
    if (dcs() || f.u().ownersys == 0 && f.u().ownerusr == a()->usernum) {
      bout.nl();
      if (a()->batch().contains_file(f.aligned_filename())) {
        bout << "|#6That file is in the batch queue; remove it from there.\r\n\n";
      } else {
        printfileinfo(&f.u(), a()->current_user_dir().subnum);
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
            if (bDeleteFileToo && (f.u().ownersys == 0)) {
              bout << "|#5Remove DL points? ";
              bRemoveDlPoints = yesno();
            }
            if (a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
              bout.nl();
              bout << "|#5Remove from ALLOW.DAT? ";
              if (yesno()) {
                remove_from_file_database(f.aligned_filename());
              }
            }
          } else {
            bDeleteFileToo = true;
            remove_from_file_database(f.aligned_filename());
          }
          if (bDeleteFileToo) {
            auto del_fn = PathFilePath(a()->directories[a()->current_user_dir().subnum].path, f);
            File::Remove(del_fn);
            if (bRemoveDlPoints && f.u().ownersys == 0) {
              a()->users()->readuser(&uu, f.u().ownerusr);
              if (!uu.IsUserDeleted()) {
                if (date_to_daten(uu.GetFirstOn()) < f.u().daten) {
                  uu.SetFilesUploaded(uu.GetFilesUploaded() - 1);
                  uu.set_uk(uu.uk() - bytes_to_k(f.u().numbytes));
                  a()->users()->writeuser(&uu, f.u().ownerusr);
                }
              }
            }
          }
          sysoplog() << fmt::format("- \"{}\" removed off of {}", f.aligned_filename(),
                                    a()->directories[a()->current_user_dir().subnum].name);
          if (a()->current_file_area()->DeleteFile(f, i)) {
            a()->current_file_area()->Save();
            --i;
          }
        }
      }
    }
    i = nrecno(remove_fn, i);
  }
}
