/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "bbs/xfer.h"

#include "bbs/archivers.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/dropfile.h"
#include "bbs/execexternal.h"
#include "bbs/listplus.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/tag.h"
#include "bbs/utility.h"
#include "bbs/xferovl1.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "core/numbers.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/files/arc.h"
#include "sdk/files/files.h"

#include <string>
#include <vector>

using wwiv::sdk::files::FileName;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

int foundany;
daten_t this_date;

/**
 * returns true if everything is ok, false if the file
 */
bool check_ul_event(int directory_num, uploadsrec * u) {
  if (a()->upload_cmd.empty()) {
    return true;
  }
  const auto comport = std::to_string(a()->sess().incom() ? a()->primary_port() : 0);
  const auto& d = a()->dirs()[directory_num];
  const auto dir_path = File::absolute(a()->bbspath(), d.path);
  const auto cmd_line = stuff_in(a()->upload_cmd, create_chain_file(), dir_path.string(),
                                FileName(u->filename).unaligned_filename(), comport, "");
  ExecuteExternalProgram(cmd_line, a()->spawn_option(SPAWNOPT_ULCHK));

  if (const auto file = FilePath(dir_path, FileName(u->filename)); !File::Exists(file)) {
    sysoplog() << "File \"" << u->filename << "\" to " << a()->dirs()[directory_num].name << " deleted by UL event.";
    bout << u->filename << " was deleted by the upload event.\r\n";
    return false;
  }
  return true;
}

static const std::vector<std::string> device_names = {
  "KBD$", "PRN", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
  "COM8", "LPT1", "LPT2", "LPT3", "CLOCK$", "SCREEN$", "POINTER$", "MOUSE$"
};

bool okfn(const std::string& filename) {
  if (filename.empty()) {
    return false;
  }

  if (const auto len = filename.length(); len == 0) {
    return false;
  }
  if (filename[0] == '-' || filename[0] == ' ' || filename[0] == '.' || filename[0] == '@') {
    return false;
  }
  for (auto c : filename) {
    const unsigned char ch = c;
    if (ch == ' '  || ch == '/' || ch == '\\' || ch == ':'  ||
        ch == '>'  || ch == '<' || ch == '|'  || ch == '+'  ||
        ch == ','  || ch == ';' || ch == '^'  || ch == '\"' ||
        ch == '\'' || ch == '`' || ch > 126) {
      return false;
    }
  }


  for (const auto& device : device_names) {
    if (const auto deviceLen = device.length();
        filename.length() >= deviceLen && filename.substr(0, deviceLen) == device) {
      if (filename[deviceLen] == '\0' || filename[deviceLen] == '.' || deviceLen == 8) {
        return false;
      }
    }
  }
  return true;
}

void print_devices() {
  for (const auto& device : device_names) {
    bout << device << "\r\n";
  }
}


int list_arc_out(const std::string& file_name, const std::string& dir) {
  std::string name_to_delete;

  if (a()->dirs()[a()->current_user_dir().subnum].mask & mask_cdrom) {
    if (const auto full_pathname = FilePath(a()->sess().dirs().temp_directory(), file_name); !File::Exists(full_pathname)) {
      const auto name_in_dir = FilePath(dir, file_name);
      File::Copy(name_in_dir, full_pathname);
      name_to_delete = full_pathname.string();
    }
  }
  const auto full_pathname = FilePath(dir, file_name);
  auto opt_cmd = get_arc_cmd(full_pathname.string(), arc_command_type_t::list, "");
  if (!okfn(file_name)) {
    opt_cmd.reset();
  }

  auto return_code = 0;
  if (File::Exists(full_pathname) && opt_cmd.has_value()) {
    bout << "\r\n\nArchive listing for " << file_name << "\r\n\n";
    if (const auto cmd = opt_cmd.value(); cmd.internal) {
      auto o = wwiv::sdk::files::list_archive(full_pathname);
      if (!o) {
        bout << "Unable to view archive: '" << full_pathname << "'.";
        return 1;
      }
      const auto& files = o.value();
      bout << "|#2CompSize     Size   Date   Time  CRC-32   File Name" << wwiv::endl;
      bout << "|#7======== -------- ======== ----- ======== ----------------------------------" << wwiv::endl;
      int line_num = 0;
      for (const auto& f : files) {
        const auto dt = DateTime::from_time_t(f.dt);
        auto d = dt.to_string("%m/%d/%y");
        auto t = dt.to_string("%I:%M");
        auto line = fmt::format("{:>8} {:>8} {:<8} {:<5} {:>08x} {}", f.compress_size,
                                f.uncompress_size, d, t, f.crc32, f.filename);
        bout << ((++line_num % 2) ? "|#9" : "|#1") << line << wwiv::endl;
      }
    } else {
      return_code = ExecuteExternalProgram(cmd.cmd, a()->spawn_option(SPAWNOPT_ARCH_L));
    }
  } else {
    bout << "\r\nUnknown archive: " << file_name << "\r\n";
  }
  bout.nl();

  if (!name_to_delete.empty()) {
    File::Remove(name_to_delete);
  }
  return return_code;
}

bool ratio_ok() {
  if (!a()->user()->exempt_ratio()) {
    if (a()->config()->req_ratio() > 0.0001f && a()->user()->ratio() < a()->config()->req_ratio()) {
      bout.cls();
      bout.nl();
      bout.bprintf(
          "Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n\n",
          a()->user()->ratio(), a()->config()->req_ratio());
      return false;
    }
  }
  if (!a()->user()->exempt_post()) {
    if (a()->config()->post_to_call_ratio() > 0.0001f &&
        post_ratio() < a()->config()->post_to_call_ratio()) {
      bout.nl();
      bout.bprintf("%s %-5.3f.  %s %-5.3f %s.\r\n\n", "Your post/call ratio is",
                           post_ratio(), "You need a ratio of", a()->config()->post_to_call_ratio(),
                           "to download.");
      return false;
    }
  }
  return true;
}

bool dcs() {
  return cs() || a()->user()->dsl() >= 100;
}

void dliscan1(int directory_num) {
  dliscan1(a()->dirs()[directory_num]);
}

void dliscan1(const wwiv::sdk::files::directory_t& dir) {
  if (!a()->fileapi()->Exist(dir.filename)) {
    if (!a()->fileapi()->Create(dir.filename)) {
      LOG(ERROR) << "Failed to create file area: " << dir.filename;
    }
  }

  a()->set_current_file_area(a()->fileapi()->Open(dir));
  this_date = a()->current_file_area()->header().daten();
}

void dliscan() {
  dliscan1(a()->dirs()[a()->current_user_dir().subnum]);
}

std::string aligns(const std::string& file_name) {
  return wwiv::sdk::files::align(file_name);
}

void printinfo(uploadsrec* u, bool *abort) {
  bool next;

  {
    tagrec_t t{};
    t.u = *u;
    const auto subnum = a()->current_user_dir().subnum;
    t.directory = subnum;
    t.dir_mask = a()->dirs()[subnum].mask;
    a()->filelist.emplace_back(t);
    const FileName fn(u->filename);
    const auto s = fmt::format("\r|#{}{:<2} |#1{} ", a()->batch().contains_file(u->filename) ? 6 : 0,
                               size_int(a()->filelist), fn.aligned_filename());
    bout.bputs(s, abort, &next);
  }

  auto size = humanize(u->numbytes);

  if (const auto& dir = a()->dirs()[a()->udir[a()->current_user_dir_num()].subnum];
      !(dir.mask & mask_cdrom)) {
    if (const auto dir_path = File::absolute(a()->bbspath(), dir.path);
        !File::Exists(FilePath(dir_path, FileName(u->filename)))) {
      size = "N/A";
    }
  }
  const auto desc = trim_to_size_ignore_colors(u->description, a()->user()->GetScreenChars() - 28);
  const auto s = fmt::format("|#2{:>5} {:>4} |#1{}", size, u->numdloads, desc);
  bout.bpla(s, abort);

  if (*abort) {
    a()->filelist.clear();
  }
}

void printtitle(bool *abort) {
  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;

  bin.checka(abort);
  bout.Color(2);
  bout << "\r"
    << a()->dirs()[a()->current_user_dir().subnum].name
    << " - #" << a()->current_user_dir().keys << ", "
    << a()->current_file_area()->number_of_files() << " files."
    << wwiv::endl;

  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;
}
std::string file_mask() {
  return file_mask("|#2File mask: ");
}

std::string file_mask(const std::string& prompt) {
  if (!prompt.empty()) {
    bout.nl();
    bout << prompt;
  }
  auto s = bin.input(12);
  if (s.empty()) {
    s = "*.*";
  }
  if (!contains(s, '.')) {
    s += ".*";
  }
  bout.nl();
  return aligns(s);
}

void listfiles() {
  if (okansi()) {
    listfiles_plus(LP_LIST_DIR);
    return;
  }

  dliscan();
  const auto filemask = file_mask();
  auto need_title = true;
  bout.clear_lines_listed();

  auto* area = a()->current_file_area();
  auto abort = false;
  for (auto i = 1; i <= area->number_of_files() && !abort && !a()->sess().hangup(); i++) {
    if (auto f = area->ReadFile(i);
        wwiv::sdk::files::aligned_wildcard_match(filemask, f.aligned_filename())) {
      if (need_title) {
        printtitle(&abort);
        need_title = false;
      }

      printinfo(&f.u(), &abort);

      // Moved to here from bputch.cpp
      if (bout.lines_listed() >= a()->sess().num_screen_lines() - 3) {
        if (!a()->filelist.empty()) {
          tag_files(need_title);
          bout.clear_lines_listed();
        }
      }
    } else if (bin.bkbhit()) {
      bin.checka(&abort);
    }
  }
  endlist(1);
}

void nscandir(uint16_t nDirNum, bool& need_title, bool *abort) {
  const auto old_cur_dir = a()->current_user_dir_num();
  a()->set_current_user_dir_num(nDirNum);
  dliscan();
  if (this_date >= a()->sess().nscandate()) {
    if (okansi()) {
      *abort = listfiles_plus(LP_NSCAN_DIR) ? 1 : 0;
      a()->set_current_user_dir_num(old_cur_dir);
      return;
    }
    auto* area = a()->current_file_area();
    for (auto i = 1; i <= a()->current_file_area()->number_of_files() && !*abort && !a()->sess().hangup();
         i++) {
      a()->CheckForHangup();
      if (auto f = area->ReadFile(i); f.u().daten >= a()->sess().nscandate()) {
        if (need_title) {
          if (bout.lines_listed() >= a()->sess().num_screen_lines() - 7 && !a()->filelist.empty()) {
            tag_files(need_title);
          }
          if (need_title) {
            printtitle(abort);
            need_title = false;
          }
        }

        printinfo(&f.u(), abort);
      } else if (bin.bkbhit()) {
        bin.checka(abort);
      }
    }
  }
  a()->set_current_user_dir_num(old_cur_dir);
}

void nscanall() {
  bool scan_all_confs = false;
  a()->sess().scanned_files(true);

  if (ok_multiple_conf(a()->user(), a()->uconfdir)) {
    bout.nl();
    bout << "|#5All conferences? ";
    scan_all_confs = bin.yesno();
    bout.nl();
    if (scan_all_confs) {
      tmp_disable_conf(true);
    }
  }
  if (okansi()) {
    const auto save_dir = a()->current_user_dir_num();
    listfiles_plus(LP_NSCAN_NSCAN);
    if (scan_all_confs) {
      tmp_disable_conf(false);
    }
    a()->set_current_user_dir_num(save_dir);
    return;
  }
  bool abort = false;
  int count = 0;
  int color = 3;
  bout << "\r|#2Searching ";
  for (uint16_t i = 0; i < size_int(a()->udir) && !abort; i++) {
    count++;
    bout.Color(color);
    bout << ".";
    if (count >= NUM_DOTS) {
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
    int nSubNum = a()->udir[i].subnum;
    if (a()->sess().qsc_n[nSubNum / 32] & (1L << (nSubNum % 32))) {
      bool need_title = true;
      nscandir(i, need_title, &abort);
    }
  }
  endlist(2);
  if (scan_all_confs) {
    tmp_disable_conf(false);
  }
}

void searchall() {
  if (okansi()) {
    listfiles_plus(LP_SEARCH_ALL);
    return;
  }

  bool bScanAllConfs = false;
  if (ok_multiple_conf(a()->user(), a()->uconfdir)) {
    bout.nl();
    bout << "|#5All conferences? ";
    bScanAllConfs = bin.yesno();
    bout.nl();
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  auto abort = false;
  const auto old_cur_dir = a()->current_user_dir_num();
  bout.nl(2);
  bout << "Search all directories.\r\n";
  const auto filemask = file_mask();
  bout.nl();
  bout << "|#2Searching ";
  bout.clear_lines_listed();
  int count = 0;
  int color = 3;
  for (auto i = 0; i < size_int(a()->udir) && !abort && !a()->sess().hangup(); i++) {
    const int nDirNum = a()->udir[i].subnum;
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    bool bIsDirMarked =  a()->sess().qsc_n[nDirNum / 32] & (1L << (nDirNum % 32));
    bIsDirMarked = true;
    // remove bIsDirMarked=true to search only marked directories
    if (bIsDirMarked) {
      count++;
      bout.Color(color);
      bout << ".";
      if (count >= NUM_DOTS) {
        bout << "\r" << "|#2Searching ";
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
      bool need_title = true;
      auto* area = a()->current_file_area();
      for (int i1 = 1; i1 <= a()->current_file_area()->number_of_files() && !abort && !a()->sess().hangup();
           i1++) {
        if (auto f = area->ReadFile(i1);
            wwiv::sdk::files::aligned_wildcard_match(filemask, f.aligned_filename())) {
          if (need_title) {
            if (bout.lines_listed() >= a()->sess().num_screen_lines() - 7 && !a()->filelist.empty()) {
              tag_files(need_title);
            }
            if (need_title) {
              printtitle(&abort);
              need_title = false;
            }
          }
          printinfo(&f.u(), &abort);
        } else if (bin.bkbhit()) {
          bin.checka(&abort);
        }
      }
    }
  }
  a()->set_current_user_dir_num(old_cur_dir);
  endlist(1);
  if (bScanAllConfs) {
    tmp_disable_conf(false);
  }
}

int recno(const std::string& file_mask) {
  return nrecno(file_mask, 0);
}

int nrecno(const std::string& file_mask, int start_recno) {
  auto* area = a()->current_file_area();
  return !area ? -1 : area->SearchFile(file_mask, start_recno + 1).value_or(-1);
}

static long xfer_time_in_seconds(long b) {
  if (a()->modem_speed_ == 0) {
    return 0;
  }
  return (b + 127) * 10 / a()->modem_speed_;
}

int printfileinfo(const uploadsrec* u, const wwiv::sdk::files::directory_t& dir) {
  const auto d = xfer_time_in_seconds(u->numbytes);
  bout << "|#9Filename:    |#2" << FileName(u->filename) << wwiv::endl;
  bout << "|#9Description: |#2" << u->description << wwiv::endl;
  bout << "|#9File size:   |#2" << humanize(u->numbytes) << wwiv::endl;
  bout << "|#9Apprx. time: |#2" << ctim(d) << wwiv::endl;
  bout << "|#9Uploaded on: |#2" << u->date << wwiv::endl;
  if (u->actualdate[2] == '/' && u->actualdate[5] == '/') {
    bout << "|#9File date:  |#2 " << u->actualdate << wwiv::endl;
  }
  bout << "|#9Uploaded by: |#2" << u->upby << wwiv::endl;
  bout << "|#9Times D/L'd: |#2" << u->numdloads << wwiv::endl;
  bout.nl();
  if (u->mask & mask_extended) {
    bout << "|#9Extended Description: \r\n";
    print_extended(u->filename, 255, -1, wwiv::sdk::Color::YELLOW, nullptr);

  }

  if (dir.mask & mask_cdrom) {
    bout.nl();
    bout << "|#3CD ROM DRIVE\r\n";
  } else {
    if (const auto dir_path = File::absolute(a()->bbspath(), dir.path);
        !File::Exists(FilePath(dir_path, FileName(u->filename)))) {
      bout << "\r\n-=>FILE NOT THERE<=-\r\n\n";
      return -1;
    }
  }

  return nsl() >= d ? 1 : 0;
}

void remlist(const std::string& file_name) {
  const auto fn = aligns(file_name);

  if (a()->filelist.empty()) {
    return;
  }
  for (auto b = a()->filelist.begin(); b != a()->filelist.end(); ++b) {
    if (const auto list_fn = aligns(b->u.filename); fn == list_fn) {
      a()->filelist.erase(b);
      return;
    }
  }
}
