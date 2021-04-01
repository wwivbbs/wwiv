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
#include "bbs/xferovl1.h"

#include "bbs/archivers.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/execexternal.h"
#include "bbs/external_edit.h"
#include "bbs/instmsg.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/tag.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xfertmp.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/pause.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/numbers.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/diz.h"
#include "sdk/files/files.h"
#include <memory>
#include <string>
#include <vector>

// How far to indent extended descriptions
static const int INDENTION = 24;

extern int foundany;

using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::stl;
using namespace wwiv::strings;

void modify_extended_description(std::string* sss, const std::string& dest) {
  char s[255], s1[255];
  int i, i2;

  bool ii = !sss->empty();
  int i4 = 0;
  bout << "File Name: " << dest << wwiv::endl;
  do {
    if (ii) {
      bout.nl();
      if (okfsed() && a()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
        bout << "|#5Modify the extended description? ";
      } else {
        bout << "|#5Enter a new extended description? ";
      }
      if (!bin.yesno()) {
        return;
      }
    } else {
      bout.nl();
      bout << "|#5Enter an extended description? ";
      if (!bin.yesno()) {
        return;
      }
    }
    if (okfsed() && a()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
      if (!sss->empty()) {
        TextFile file(FilePath(a()->sess().dirs().temp_directory(), "extended.dsc"), "w");
        file.Write(*sss);
      } else {
        File::Remove(FilePath(a()->sess().dirs().temp_directory(), "extended.dsc"));
      }

      const auto saved_screen_chars = a()->user()->screen_width();
      if (a()->user()->screen_width() > (76 - INDENTION)) {
        a()->user()->screen_width(76 - INDENTION);
      }

      const auto edit_ok = fsed_text_edit("extended.dsc", a()->sess().dirs().temp_directory(),
                                              a()->max_extend_lines, MSGED_FLAG_NO_TAGLINE);
      a()->user()->screen_width(saved_screen_chars);
      if (edit_ok) {
        TextFile file(FilePath(a()->sess().dirs().temp_directory(), "extended.dsc"), "r");
        *sss = file.ReadFileIntoString();

        for (auto i3 = wwiv::stl::ssize(*sss) - 1; i3 >= 0; i3--) {
          if ((*sss)[i3] == 1) {
            (*sss)[i3] = ' ';
          }
        }
      }
    } else {
      sss->clear();
      i = 1;
      bout.nl();
      bout << "Enter up to  " << a()->max_extend_lines << " lines, "
          << 78 - INDENTION << " chars each.\r\n";
      bout.nl();
      s[0] = '\0';
      const int nSavedScreenSize = a()->user()->screen_width();
      if (a()->user()->screen_width() > (76 - INDENTION)) {
        a()->user()->screen_width(76 - INDENTION);
      }
      do {
        bout << "|#2" << i << ": |#0";
        s1[0] = '\0';
        const auto allow_previous = i4 > 0;
        while (inli(s1, s, 90, true, allow_previous, false, true)) {
          if (i > 1) {
            --i;
          }
          sprintf(s1, "%d:", i);
          bout << "|#2" << s1;
          i2 = 0;
          i4 -= 2;
          do {
            s[i2] = sss->at(i4 - 1);
            ++i2;
            --i4;
          }
          while (sss->at(i4) != 10 && i4 != 0);
          if (i4) {
            ++i4;
          }
          sss->resize(i4);
          s[i2] = 0;
          strrev(s);
          if (strlen(s) > static_cast<unsigned int>(a()->user()->screen_width() - 1)) {
            s[a()->user()->screen_width() - 2] = '\0';
          }
        }
        i2 = strlen(s1);
        if (i2 && (s1[i2 - 1] == 1)) {
          s1[i2 - 1] = '\0';
        }
        if (s1[0]) {
          strcat(s1, "\r\n");
          *sss += s1;
          i4 += strlen(s1);
        }
      }
      while (i++ < a()->max_extend_lines && s1[0]);
      a()->user()->screen_width(nSavedScreenSize);
    }
    bout << "|#5Is this what you want? ";
    i = !bin.yesno();
    if (i) {
      sss->clear();
    }
  }
  while (i);
}

static bool has_arc_cmd_for_ext(const std::string& ext) {
  for (auto i = 0; i < MAX_ARCS; i++) {
    if (iequals(ext, a()->arcs[i].extension)) {
      return true;
    }
  }
  return false;
}

static std::optional<std::filesystem::path> PathToTempdDiz(const std::filesystem::path& p) {
  VLOG(1) << "PathToTempDiz: " << p;
  File::Remove(FilePath(a()->sess().dirs().temp_directory(), FILE_ID_DIZ));
  File::Remove(FilePath(a()->sess().dirs().temp_directory(), DESC_SDI));

  if (!p.has_extension() || !has_arc_cmd_for_ext(p.extension().string().substr(1))) {
    return std::nullopt;
  }

  const auto cmd = get_arc_cmd(p.string(), arc_command_type_t::extract, "FILE_ID.DIZ DESC.SDI");
  if (!cmd.has_value() || cmd->internal) {
    // Can't handle internal extract
    return std::nullopt;
  }
  ExecuteExternalProgram(cmd.value().cmd, EFLAG_NOHUP | EFLAG_TEMP_DIR);
  auto diz_fn = FilePath(a()->sess().dirs().temp_directory(), FILE_ID_DIZ);
  VLOG(1) << "Checking for diz: " << diz_fn;
  if (auto o = FindFile(diz_fn)) {
    VLOG(1) << "Found: " << diz_fn;
    return { o.value() };
  }
  diz_fn = FilePath(a()->sess().dirs().temp_directory(), DESC_SDI);
  VLOG(1) << "Checking for diz: " << diz_fn;
  if (auto o = FindFile(diz_fn)) {
    VLOG(1) << "Found: " << diz_fn;
    return { o.value() };
  }
  VLOG(1) << "No diz.";
  return std::nullopt;
}

bool get_file_idz(FileRecord& fr, const directory_t& dir) {
  ScopeExit at_exit([] {
    File::Remove(FilePath(a()->sess().dirs().temp_directory(), FILE_ID_DIZ));
    File::Remove(FilePath(a()->sess().dirs().temp_directory(), DESC_SDI));
  });

  if (!a()->HasConfigFlag(OP_FLAGS_READ_CD_IDZ) && dir.mask & mask_cdrom) {
    return false;
  }

  const auto dir_path = File::absolute(a()->bbspath(), dir.path);
  fr.set_date(DateTime::from_time_t(File::last_write_time(FilePath(dir_path, fr))));
  auto o = PathToTempdDiz(FilePath(dir_path, fr));
  if (!o) {
    LOG(INFO) << "File had no DIZ: " << fr;
    return true;
  }
  const auto& diz_fn = o.value();
  bout.nl();
  bout << "|#9Reading in |#2" << diz_fn.filename() << "|#9 as extended description...";
  const auto old_ext = a()->current_file_area()->ReadExtendedDescriptionAsString(fr).value_or("");

  const DizParser dp(a()->HasConfigFlag(OP_FLAGS_IDZ_DESC));
  if (auto odiz = dp.parse(diz_fn)) {
    auto& diz = odiz.value();
    fr.set_description(diz.description());
    const auto ext_desc = diz.extended_description();

    VLOG(1) << "Diz Desc: " << diz.description();
    VLOG(1) << "Diz Ext:  " << ext_desc;
    if (!ext_desc.empty()) {
      if (!old_ext.empty()) {
        a()->current_file_area()->DeleteExtendedDescription(fr.filename());
      }
      a()->current_file_area()->AddExtendedDescription(fr.filename(), ext_desc);
      fr.set_extended_description(true);
    }
  } else {
    VLOG(1) << "Failed to parse DIZ: " << diz_fn;
  }

  bout << "Done!\r\n";
  return true;
}

int read_idz_all() {
  int count = 0;

  tmp_disable_conf(true);
  TempDisablePause disable_pause(bout);
  a()->ClearTopScreenProtection();
  for (auto i = 0; i < a()->dirs().size() && !bout.localIO()->KeyPressed(); i++) {
    count += read_idz(false, i);
  }
  tmp_disable_conf(false);
  a()->UpdateTopScreen();
  return count;
}

int read_idz(bool prompt_for_mask, int tempdir) {
  int count = 0;
  bool abort = false;
  const auto dir_num = a()->udir[tempdir].subnum;
  const auto dir = a()->dirs()[dir_num];

  std::unique_ptr<TempDisablePause> disable_pause;
  std::string s = "*.*";
  if (prompt_for_mask) {
    disable_pause.reset(new TempDisablePause(bout));
    a()->ClearTopScreenProtection();
    dliscan();
    s = file_mask();
  } else {
    s = aligns(s);
    dliscan1(dir_num);
  }
  bout.bprintf("|#9Checking for external description files in |#2%-25.25s #%s...\r\n", dir.name,
               a()->udir[tempdir].keys);
  auto* area = a()->current_file_area();
  for (auto i = 1; i <= area->number_of_files() && !a()->sess().hangup() && !abort; i++) {
    auto f = area->ReadFile(i);
    const auto fn = f.aligned_filename();
    if (aligned_wildcard_match(s, fn) && !ends_with(fn, ".COM") && !ends_with(fn, ".EXE")) {
      if (File::Exists(FilePath(dir.path, f))) {
        if (get_file_idz(f, dir)) {
          count++;
        }
        if (area->UpdateFile(f, i)) {
          area->Save();
        }
      }
    }
    bin.checka(&abort);
  }
  if (prompt_for_mask) {
    a()->UpdateTopScreen();
  }
  return count;
}

void tag_it() {
  long fs = 0;

  if (a()->batch().size() >= a()->max_batch) {
    bout << "|#6No room left in batch queue.";
    bin.getkey();
    return;
  }
  bout << "|#2Which file(s) (1-" << a()->filelist.size()
      << ", *=All, 0=Quit)? ";
  auto s3 = bin.input(30, true);
  if (!s3.empty() && s3.front() == '*') {
    s3.clear();
    for (size_t i2 = 0; i2 < a()->filelist.size() && i2 < 78; i2++) {
      auto s2 = fmt::format("{} ", i2 + 1);
      s3 += s2;
      if (s3.size() > 250 /* was sizeof(s3)-10 */) {
        break;
      }
    }
    bout << "\r\n|#2Tagging: |#4" << s3 << wwiv::endl;
  }
  for (int i2 = 0; i2 < wwiv::strings::ssize(s3); i2++) {
    auto s1 = s3.substr(i2);
    int i4 = 0;
    bool bad = false;
    for (int i3 = 0; i3 < wwiv::strings::ssize(s1); i3++) {
      if (s1[i3] == ' ' || s1[i3] == ',' || s1[i3] == ';') {
        s1 = s1.substr(0, i3);
        i4 = 1;
      } else {
        if (i4 == 0) {
          i2++;
        }
      }
    }
    int i = to_number<int>(s1);
    if (i == 0) {
      break;
    }
    i--;
    if (!s1.empty() && i >= 0 && i < ssize(a()->filelist)) {
      auto& f = a()->filelist[i];
      if (a()->batch().contains_file(f.u.filename)) {
        bout << "|#6" << f.u.filename << " is already in the batch queue.\r\n";
        bad = true;
      }
      if (a()->batch().size() >= a()->max_batch) {
        bout << "|#6Batch file limit of " << a()->max_batch << " has been reached.\r\n";
        bad = true;
      }
      if (a()->config()->req_ratio() > 0.0001f &&
          a()->user()->ratio() < a()->config()->req_ratio() && !a()->user()->exempt_ratio() &&
          !bad) {
        bout.bprintf(
            "|#2Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n",
            a()->user()->ratio(), a()->config()->req_ratio());
        bad = true;
      }
      if (!bad) {
        auto s = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
        if (f.dir_mask & mask_cdrom) {
          auto s2 = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
          s = FilePath(a()->sess().dirs().temp_directory(), FileName(f.u.filename));
          if (!File::Exists(s)) {
            File::Copy(s2, s);
          }
        }
        File fp(s);
        if (!fp.Open(File::modeBinary | File::modeReadOnly)) {
          bout << "|#6The file " << FileName(f.u.filename) << " is not there.\r\n";
          bad = true;
        } else {
          fs = fp.length();
          fp.Close();
        }
      }
      if (!bad) {
        const auto t = time_to_transfer(a()->modem_speed_,fs).count();
        if (nsl() <= a()->batch().dl_time_in_secs() + t) {
          bout << "|#6Not enough time left in queue for " << f.u.filename << ".\r\n";
          bad = true;
        }
      }
      if (!bad) {
        BatchEntry b(f.u.filename, f.directory, fs, true);
        a()->batch().AddBatch(std::move(b));
        bout << "|#1" << f.u.filename << " added to batch queue.\r\n";
      }
    } else {
      bout << "|#6Bad file number " << i + 1 << wwiv::endl;
    }
    bout.clear_lines_listed();
  }
}

static char fancy_prompt(const char* pszPrompt, const char* pszAcceptChars) {
  char ch;

  a()->tleft(true);
  const auto s1 = fmt::format("\r|#2{} (|#1{}|#2)? |#0", pszPrompt, pszAcceptChars);
  const auto s2 = fmt::format("{} ({})? ", pszPrompt, pszAcceptChars);
  const int i1 = ssize(s2);
  const auto s3 = StrCat(pszAcceptChars," \r");
  a()->tleft(true);
  if (okansi()) {
    bout << s1;
    ch = onek_ncr(s3);
    bout.Left(i1);
    for (int i = 0; i < i1; i++) {
      bout.bputch(' ');
    }
    bout.Left(i1);
  } else {
    bout << s2;
    ch = onek_ncr(s3);
    for (auto i = 0; i < i1; i++) {
      bout.bs();
    }
  }
  return ch;
}

void tag_files(bool& need_title) {
  if (bout.lines_listed() == 0) {
    return;
  }
  a()->tleft(true);
  if (a()->sess().hangup()) {
    return;
  }
  bout.clear_lines_listed();
  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;

  auto done = false;
  while (!done && !a()->sess().hangup()) {
    bout.clear_lines_listed();
    const auto ch = fancy_prompt("File Tagging", "CDEMQRTV?");
    bout.clear_lines_listed();
    switch (ch) {
    case '?': {
      bout.print_help_file(TTAGGING_NOEXT);
      bout.pausescr();
      relist();
    }
    break;
    case 'C':
    case SPACE:
    case RETURN:
      bout.clear_lines_listed();
      a()->filelist.clear();
      need_title = true;
      bout.cls();
      done = true;
      break;
    case 'D':
      batchdl(1);
      bout.nl();
      bout.pausescr();
      bout.cls();
      done = true;
      break;
    case 'E': {
      bout.clear_lines_listed();
      bout << "|#9Which file (1-" << a()->filelist.size() << ")? ";
      auto s = bin.input(2, true);
      const auto i = to_number<int>(s) - 1;
      if (!s.empty() && i >= 0 && i < ssize(a()->filelist)) {
        auto& f = a()->filelist[i];
        bout.nl();
        int i2;
        for (i2 = 0; i2 < size_int(a()->udir); i2++) {
          if (a()->udir[i2].subnum == f.directory) {
            break;
          }
        }
        std::string keys = i2 < size_int(a()->udir) ? a()->udir[i2].keys : "??";
        const auto& dir = a()->dirs()[f.directory];
        bout.format("|#1Directory  : |#2#{}, {}\r\n", keys, dir.name);
        printfileinfo(&f.u, dir);
        bout.nl();
        bout.pausescr();
        relist();
      }
    }
    break;
    case 'M':
      if (dcs()) {
        move_file_t();
        if (a()->filelist.empty()) {
          return;
        }
        relist();
      }
      break;
    case 'Q':
      a()->filelist.clear();
      bout.clear_lines_listed();
      need_title = false;
      return;
    case 'R':
      relist();
      break;
    case 'T':
      tag_it();
      break;
    case 'V': {
      bout << "|#2Which file (1-|#2" << a()->filelist.size() << ")? ";
      auto s = bin.input(2, true);
      int i = to_number<int>(s) - 1;
      if (!s.empty() && i >= 0 && i < ssize(a()->filelist)) {
        auto& f = a()->filelist[i];
        auto s1 = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
        if (a()->dirs()[f.directory].mask & mask_cdrom) {
          auto s2 = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
          s1 = FilePath(a()->sess().dirs().temp_directory(), FileName(f.u.filename));
          if (!File::Exists(s1)) {
            File::Copy(s2, s1);
          }
        }
        if (!File::Exists(s1)) {
          bout << "|#6File not there.\r\n";
          bout.pausescr();
          break;
        }
        list_arc_out(s1.string(), "");
        bout.pausescr();
        a()->UpdateTopScreen();
        bout.cls();
        relist();
      }
    }
    break;
    default:
      bout.cls();
      done = true;
      break;
    }
  }
  a()->filelist.clear();
  bout.clear_lines_listed();
}


int add_batch(std::string& description, const std::string& aligned_file_name, int dn, long fs) {
  if (a()->batch().FindBatch(aligned_file_name) > -1) {
    return 0;
  }

  const auto t = time_to_transfer(a()->modem_speed_,fs).count();

  if (nsl() <= a()->batch().dl_time_in_secs() + t) {
    bout << "|#6 Insufficient time remaining... press any key.";
    bin.getkey();
  } else {
    if (dn == -1) {
      return 0;
    }
    for (auto& c : description) {
      if (c == '\r')
        c = ' ';
    }
    bout.backline();
    bout.bprintf(" |#6? |#1%s %3.3s |#5%-43.43s |#7[|#2Y/N/Q|#7] |#0", aligned_file_name,
                 humanize(fs), stripcolors(description));
    auto ch = onek_ncr("QYN\r");
    bout.backline();
    const auto ufn = wwiv::sdk::files::unalign(aligned_file_name);
    const auto dir = a()->dirs()[dn];
    if (to_upper_case<char>(ch) == 'Y') {
      if (dir.mask & mask_cdrom) {
        const auto src = FilePath(dir.path, ufn);
        const auto dest = FilePath(a()->sess().dirs().temp_directory(), ufn);
        if (!File::Exists(dest)) {
          if (!File::Copy(src, dest)) {
            bout << "|#6 file unavailable... press any key.";
            bin.getkey();
          }
          bout.backline();
          bout.clreol();
        }
      } else {
        auto f = FilePath(a()->dirs()[dn].path, ufn);
        if (!File::Exists(f) && !so()) {
          bout << "\r";
          bout.clreol();
          bout << "|#6 file unavailable... press any key.";
          bin.getkey();
          bout << "\r";
          bout.clreol();
          return 0;
        }
      }
      const BatchEntry b(aligned_file_name, dn, fs, true);
      bout << "\r";
      const auto bt = ctim(b.time(a()->modem_speed_));
      bout.bprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n", 
                           a()->batch().size() + 1,
                           b.aligned_filename(), b.len(), bt, a()->dirs()[b.dir()].name);
      a()->batch().AddBatch(b);
      bout << "\r";
      bout << "|#5    Continue search? ";
      ch = onek_ncr("YN\r");
      if (to_upper_case<char>(ch) == 'N') {
        return -3;
      }
      return 1;
    }
    if (ch == 'Q') {
      bout.backline();
      return -3;
    }
    bout.backline();
  }
  return 0;
}

int try_to_download(const std::string& file_mask, int dn) {
  int rtn;
  bool abort = false;

  dliscan1(dn);
  int i = recno(file_mask);
  if (i <= 0) {
    bin.checka(&abort);
    return abort ? -1 : 0;
  }
  bool ok = true;
  foundany = 1;
  do {
    a()->tleft(true);
    auto* area = a()->current_file_area();
    auto f = area->ReadFile(i);

    if (!(f.u().mask & mask_no_ratio) && !ratio_ok()) {
      return -2;
    }

    write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
    auto d = fmt::sprintf("%-40.40s", f.description());
    abort = false;
    rtn = add_batch(d, f.aligned_filename(), dn, f.numbytes());

    if (abort || rtn == -3) {
      ok = false;
    } else {
      i = nrecno(file_mask, i);
    }
  } while (i > 0 && ok && !a()->sess().hangup());

  if (rtn == -2) {
    return -2;
  }
  if (abort || rtn == -3) {
    return -1;
  }
  return 1;
}

void download() {
  int i = 0, color = 0;
  bool ok;
  int rtn = 0;
  bool done = false;

  int useconf = 0;

  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " Batch Downloads"));
  do {
    if (!i) {
      bout << "|#2Enter files, one per line, wildcards okay.  [Space] aborts a search.\r\n";
      bout.nl();
      bout << "|#1 #  File Name    Size    Time      Directory\r\n";
      bout <<
          "|#7\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\r\n";
    }
    if (i < ssize(a()->batch().entry)) {
      const auto& b = a()->batch().entry[i];
      if (b.sending()) {
        const auto t = ctim(b.time(a()->modem_speed_));
        bout.bprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n", 
                             i + 1, b.aligned_filename(), b.len(), t, a()->dirs()[b.dir()].name);
      }
    } else {
      do {
        int count = 0;
        ok = true;
        bout.backline();
        bout.bprintf("|#2%3d ", a()->batch().size() + 1);
        bout.Color(1);
        const bool onl = bout.newline;
        bout.newline = false;
        auto s = bin.input(12);
        bout.newline = onl;
        if (!s.empty() && s.front() != ' ') {
          if (strchr(s.c_str(), '.') == nullptr) {
            s += ".*";
          }
          s = aligns(s);
          rtn = try_to_download(s, a()->current_user_dir().subnum);
          if (rtn == 0) {
            if (ok_multiple_conf(a()->user(), a()->uconfdir)) {
              bout.backline();
              bout << " |#5Search all conferences? ";
              const auto ch = onek_ncr("YN\r");
              if (ch == '\r' || to_upper_case<char>(ch) == 'Y') {
                tmp_disable_conf(true);
                useconf = 1;
              }
            }
            bout.backline();
            auto s1 = fmt::sprintf("%3d %s", a()->batch().size() + 1, s);
            bout.Color(1);
            bout << s1;
            foundany = 0;
            int dn = 0;
            while (dn < size_int(a()->udir)) {
              count++;
              bout.Color(color);
              if (count == NUM_DOTS) {
                bout << "\r";
                bout.Color(color);
                bout << s1;
                color++;
                count = 0;
                if (color == 4) {
                  color++;
                }
                if (color == 10) {
                  color = 0;
                }
              }
              rtn = try_to_download(s, a()->udir[dn].subnum);
              if (rtn < 0) {
                break;
              }
              dn++;
            }
            if (useconf) {
              tmp_disable_conf(false);
            }
            if (!foundany) {
              bout << "|#6 File not found... press any key.";
              bin.getkey();
              bout.backline();
              ok = false;
            }
          }
        } else {
          bout.backline();
          done = true;
        }
      }
      while (!ok && !a()->sess().hangup());
    }
    i++;
    if (rtn == -2) {
      rtn = 0;
      i = 0;
    }
  } while (!done && !a()->sess().hangup() && i <= ssize(a()->batch().entry));

  if (!a()->batch().numbatchdl()) {
    return;
  }

  bout.nl();
  if (!ratio_ok()) {
    bout << "\r\nSorry, your ratio is too low.\r\n\n";
    return;
  }
  bout.nl();
  bout << "|#1Files in Batch Queue   : |#2" << a()->batch().size() << wwiv::endl;
  bout << "|#1Estimated Download Time: |#2" << ctim(a()->batch().dl_time_in_secs()) << wwiv::endl;
  bout.nl();
  rtn = batchdl(3);
  if (rtn) {
    return;
  }
  bout.nl();
  if (a()->batch().empty()) {
    return;
  }
  bout << "|#5Hang up after transfer? ";
  const bool had = bin.yesno();
  const int ip = get_protocol(xfertype::xf_down_batch);
  if (ip > 0) {
    switch (ip) {
    case WWIV_INTERNAL_PROT_YMODEM: {
      if (!a()->over_intern.empty()
          && a()->over_intern[2].othr & othr_override_internal
          && a()->over_intern[2].sendbatchfn[0]) {
        dszbatchdl(had, a()->over_intern[2].sendbatchfn, prot_name(WWIV_INTERNAL_PROT_YMODEM));
      } else {
        ymbatchdl(had);
      }
    }
    break;
    case WWIV_INTERNAL_PROT_ZMODEM: {
      zmbatchdl(had);
    }
    break;
    default: {
      dszbatchdl(had, a()->externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn,
                 a()->externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].description);
    }
    }
    if (!had) {
      bout.nl();
      bout << "Your ratio is now: " << a()->user()->ratio() << wwiv::endl;
    }
  }
}

void endlist(int mode) {
  // if mode == 1, list files
  // if mode == 2, new files
  if (a()->filelist.empty()) {
    bout << "\r";
    if (mode == 1) {
      bout << "|#3No matching files found.\r\n\n";
    } else {
      bout << "\r|#1No new files found.\r\n\n";
    }
    return;
  }
  bool need_title = false;
  if (!a()->filelist.empty()) {
    tag_files(need_title);
    return;
  }
  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;
  bout << "\r|#9Files listed: |#2 " << a()->filelist.size();
}

// TODO(rushfan): Move to datetime.h? either in bbs or core?
// Converts a mm/dd/yyyy string into int for month, day, year and OK 
static std::optional<DateTime> mmddyy_to_mdyo(std::string s) {
  if (s.size() != 10) {
    return std::nullopt;
  }
  const auto m = to_number<int>(StrCat(s[0], s[1]));
  const auto d = to_number<int>(StrCat(s[3], s[4]));
  const auto y = to_number<int>(StrCat(s[6], s[7], s[8], s[9]));
  // These parens are not redundant
  if (((m == 2 || m == 9 || m == 4 || m == 6 || m == 11) && d >= 31) ||
      (m == 2 && ((y % 4 != 0 && d == 29) || d == 30)) ||
      d > 31 || (m == 0 || y == 0 || d == 0) ||
      (m > 12 || d > 31)) {
      return std::nullopt;
  }

  tm t{};
  t.tm_min = 0;
  t.tm_hour = 1;
  t.tm_sec = 0;
  t.tm_year = y - 1900;
  t.tm_mday = d;
  t.tm_mon = m - 1;

  return {DateTime::from_tm(&t)};
}

void SetNewFileScanDate() {
  bout.nl();
  const auto current_dt = DateTime::from_daten(a()->sess().nscandate());
  bout << "|#9Current limiting date: |#2" << current_dt.to_string("%m/%d/%Y") << wwiv::endl;
  bout.nl();
  bout << "|#9Enter new limiting date in the following format:\r\n";
  bout << "|#1 MM/DD/YYYY\r\n|#7:";
  bout.mpl(8);
  const auto ag = bin.input_date_mmddyyyy("");
  bout.nl();
  if (!ag.empty()) {
    auto o = mmddyy_to_mdyo(ag);
    if (o.has_value()) {
      const auto& dt = o.value();
      a()->sess().nscandate(dt.to_daten_t());

      // Display the new nscan date
      bout << "|#9New Limiting Date: |#2" << dt.to_string("%m/%d/%Y") << "\r\n";

      // Hack to make sure the date covers everything since we had to increment the hour by one
      // to show the right date on some versions of MSVC
      a()->sess().nscandate(a()->sess().nscandate() - SECONDS_PER_HOUR);
    }
  }
}


void removefilesnotthere(int dn, int* autodel) {
  char ch;
  dliscan1(dn);
  const auto all_files = aligns("*.*");
  int i = recno(all_files);
  bool abort = false;
  while (!a()->sess().hangup() && i > 0 && !abort) {
    auto* area = a()->current_file_area();
    auto f = area->ReadFile(i);
    auto candidate_fn = FilePath(a()->dirs()[dn].path, f);
    if (!File::Exists(candidate_fn)) {
      StringTrim(f.u().description);
      candidate_fn = fmt::sprintf("|#2%s :|#1 %-40.40s", f.aligned_filename(), f.description());
      if (!*autodel) {
        bout.backline();
        bout << candidate_fn;
        bout.nl();
        bout << "|#5Remove Entry (Yes/No/Quit/All) : ";
        ch = onek_ncr("QYNA");
      } else {
        bout.nl();
        bout << "|#1Removing entry " << candidate_fn;
        ch = 'Y';
      }
      if (ch == 'Y' || ch == 'A') {
        if (ch == 'A') {
          bout << "ll";
          *autodel = 1;
        }
        sysoplog() << "- '" << f << "' Removed from " << a()->dirs()[dn].name;
        if (area->DeleteFile(f, i)) {
          area->Save();
        }
        --i;
      } else if (ch == 'Q') {
        abort = true;
      }
    }
    i = nrecno(all_files, i);
    bool next = true;
    bin.checka(&abort, &next);
    if (!next) {
      i = 0;
    }
  }
}

void removenotthere() {
  if (!so()) {
    return;
  }

  tmp_disable_conf(true);
  TempDisablePause disable_pause(bout);
  int autodel = 0;
  bout.nl();
  bout << "|#5Remove N/A files in all directories? ";
  if (bin.yesno()) {
    for (auto i = 0; i < size_int(a()->udir) && !bout.localIO()->KeyPressed(); i++) {
      bout.nl();
      bout << "|#1Removing N/A|#0 in " << a()->dirs()[a()->udir[i].subnum].name;
      bout.nl(2);
      removefilesnotthere(a()->udir[i].subnum, &autodel);
    }
  } else {
    bout.nl();
    bout << "Removing N/A|#0 in " << a()->dirs()[a()->current_user_dir().subnum].name;
    bout.nl(2);
    removefilesnotthere(a()->current_user_dir().subnum, &autodel);
  }
  tmp_disable_conf(false);
  a()->UpdateTopScreen();
}
