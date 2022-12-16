/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/gfiles.h"
#include "sdk/gfiles.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/gfileedit.h"
#include "bbs/instmsg.h"
#include "bbs/mmkey.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/numbers.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void gfiles2();
void gfiles3(int n);

static void gfl_hdr(int which) {
  std::string s, s1, s2, s3;
  if (okansi()) {
    s2 = std::string(29, '\xC4');
  } else {
    s2 = std::string(29, '-');
  }
  if (which) {
    s1 = std::string(12, ' ');
    s3 = std::string(11, ' ');
  } else {
    s1 = std::string(12, ' ');
    s3 = std::string(17, ' ');
  }
  bool abort = false;
  if (okansi()) {
    if (which) {
      s = StrCat("|#7\xDA\xC4\xC4\xC4\xC2", s2, "\xC2\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC2", s2, "\xC2\xC4\xC4\xC4\xC4\xBF");
    } else {
      s = StrCat("|#7\xDA\xC4\xC4\xC4\xC2", s2, "\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC2", s2, "\xC4\xC4\xC4\xC4\xBF");
    }
  } else {
    if (which) {
      s = StrCat("+---+", s2, "+----+---+", s2, "+----+");
    } else {
      s = StrCat("+---+", s2, "-----+---+", s2, "----+");
    }
  }
  bout.bpla(s, &abort);
  bout.Color(0);
  if (okansi()) {
    if (which) {
      s = StrCat("|#7\xB3|#2 # |#7\xB3", s1, "|#1 Name ", s3, "|#7\xB3|#9Size|#7\xB3|#2 # |#7\xB3", s1, "|#1 Name ", s3, "|#7\xB3|#9Size|#7\xB3");
    } else {
      s = StrCat("|#7\xB3|#2 # |#7\xB3", s1, "|#1 Name", s3, "|#7\xB3|#2 # |#7\xB3", s1, "|#1Name", s3, "|#7\xB3");
    }
  } else {
    if (which) {
      s = StrCat("| #   |", s1, "Name ", s1, "|Size| # |", s1, " Name", s1, "|Size|");
    } else {
      s = StrCat("| # |", s1, " Name     ", s1, "| # |", s1, " Name    ", s1, "|");
    }
  }
  bout.bpla(s, &abort);
  bout.Color(0);
  if (okansi()) {
    if (which) {
      s = StrCat("|#7\xC3\xC4\xC4\xC4\xC5", s2, "\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC5", s2, "\xC5\xC4\xC4\xC4\xC4\xB4");
    } else {
      s = StrCat("|#7\xC3\xC4\xC4\xC4\xC5", s2, "\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC5", s2, "\xC4\xC4\xC4\xC4\xB4");
    }
  } else {
    if (which) {
      s = StrCat("+---+", s2, "+----+---+", s2, "+----+");
    }
    else {
      s = StrCat("+---+", s2, "-----+---+", s2, "----+");
    }
  }
  bout.bpla(s, &abort);
  bout.Color(0);
}

static void list_sec(std::vector<int> map) {
  auto i2 = 0;
  auto abort = false;
  std::string s, s2, s3, s4, s5, s6, s7;
  if (okansi()) {
    s2 = std::string(29, '\xC4');
    s3 = std::string(12, '\xC4');
    s7 = std::string(12, '\xC4');
  } else {
    s2 = std::string(29, '-');
    s3 = std::string(12, '-');
    s7 = std::string(12, '-');
  }

  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " G-Files Section"));
  gfl_hdr(0);
  std::string t = times();
  for (int i = 0; i < size_int(map) && !abort && !a()->sess().hangup(); i++) {
    std::string lnum = std::to_string(i+1);
    std::string rnum;
    s4 = trim_to_size_ignore_colors(a()->gfiles()[map[i]].name, 34);
    if (i + 1 >= size_int(map)) {
      if (okansi()) {
        rnum = std::string(3, '\xFE');
        s5 = std::string(29, '\xFE');
      } else {
        rnum = std::string(3, 'o');
        s5 = std::string(29, 'o');
      }
    } else {
      rnum = std::to_string(i + 2);
      s5 = trim_to_size_ignore_colors(a()->gfiles()[map[i + 1]].name, 29);
    }
    if (okansi()) {
      s = fmt::sprintf("|#7\xB3|#2%3s|#7\xB3|#1%-34s|#7\xB3|#2%3s|#7\xB3|#1%-33s|#7\xB3", lnum, s4,
                       rnum, s5);
    } else {
      s = fmt::sprintf("|%3s|%-34s|%3s|%-33s|", lnum, s4, rnum, s5);
    }
    bout.bpla(s, &abort);
    bout.Color(0);
    i++;
    if (i2 > 10) {
      i2 = 0;
      std::string s1;
      if (okansi()) {
        s1 = fmt::sprintf(
            "|#7\xC3\xC4\xC4\xC4X%s\xC4\xC4\xC4\xC4\xC4X\xC4\xC4\xC4X\xC4\xC4\xC4\xC4\xC4\xC4%s|#"
            "1\xFE|#7\xC4|#2%s|#7\xC4|#2\xFE|#7\xC4\xC4\xC4X",
            s2, s3, t);
      } else {
        s1 = fmt::sprintf("+---+%s-----+--------+%s-o-%s-o---+", s2, s3, t);
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
      bout.nl();
      bout.pausescr();
      gfl_hdr(1);
    }
  }
  if (!abort) {
    if (so()) {
      std::string s1;
      if (okansi()) {
        s1 = fmt::sprintf("|#7\xC3\xC4\xC4\xC4\xC1%s\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC1%"
                          "s\xC4\xC4\xC4\xC4\xB4",
                          s2, s2);
      } else {
        s1 = fmt::sprintf("+---+%s-----+---+%s----+", s2, s2);
      }
      bout.bpla(s1, &abort);
      bout.Color(0);

      std::string padding61(61, ' ');
      if (okansi()) {
        s1 = fmt::sprintf("|#7\xB3  |#2G|#7)|#1G-File Edit%s|#7\xB3", padding61);
      } else {
        s1 = fmt::sprintf("|  G)G-File Edit%s|", padding61);
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
      if (okansi()) {
        s1 = fmt::sprintf("|#7\xC0\xC4\xC4\xC4\xC4%"
                          "s\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4%s|#"
                          "1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xC4\xC4\xD9",
                          s2, s7, t);
      } else {
        s1 = fmt::sprintf("+----%s----------------%so-%s-o---+", s2, s7, t);
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
    } else {
      std::string s1;
      if (okansi()) {
        s1 = fmt::sprintf(
            "|#7\xC0\xC4\xC4\xC4\xC1%"
            "s\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
            "\xC4\xC4\xC4\xC4\xC4\xC4\xC4%s|#1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xC4\xC4\xD9",
            s2, s3, t);
      } else {
        s1 = fmt::sprintf("+---+%s-----+---------------------+%so-%s-o---+", s2, s3, t);
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
    }
  }
  bout.Color(0);
  bout.nl();
}

static void list_gfiles(const gfile_dir_t& g) {
  int i;
  std::string s, s2, s3, s4, s5;
  std::string lnum, rnum, lsize, rsize;
  const auto t = times();

  auto abort = false;
  bout.cls();
  bout.litebar(g.name);
  auto i2 = 0;
  if (okansi()) {
    s2 = std::string(29, '\xC4');
    s3 = std::string(12, '\xC4');
  } else {
    s2 = std::string(29, '-');
    s3 = std::string(12, '-');
  }
  gfl_hdr(1);
  const auto gfilesdir = a()->config()->gfilesdir();
  for (i = 0; i < size_int(g.files) && !abort && !a()->sess().hangup(); i++) {
    i2++;
    lnum = std::to_string(i + 1);
    s4 = trim_to_size_ignore_colors(g.files[i].description, 29);
    if (const auto path_name = FilePath(gfilesdir, FilePath(g.filename, g.files[i].filename));
        File::Exists(path_name)) {
      File handle(path_name);
      lsize = humanize(handle.length());
    } else {
      lsize = "OFL";
    }
    if (i + 1 >= size_int(g.files)) {
      if (okansi()) {
        rnum = std::string(3, '\xFE');
        s5 = std::string(29, '\xFE');
        rsize = std::string(4, '\xFE');
      } else {
        rnum = std::string(3, 'o');
        s5 = std::string(29, 'o');
        rsize = std::string(4, 'o');
      }
    } else {
      rnum = std::to_string(i + 2);
      s5 = trim_to_size_ignore_colors(g.files[i + 1].description, 29);
      const auto path_name2 =
          FilePath(gfilesdir, FilePath(g.filename, g.files[i + 1].filename));
      if (File::Exists(path_name2)) {
        File handle(path_name2);
        rsize = humanize(handle.length());
      } else {
        rsize = "OFL";
      }
    }
    if (okansi()) {
      s = fmt::sprintf("|#7\xB3|#2%3s|#7\xB3|#1%-29s|#7\xB3|#2%4s|#7\xB3|#2%3s|#7\xB3|#1%-29s|#"
                       "7\xB3|#2%4s|#7\xB3",
                       lnum, s4, lsize, rnum, s5, rsize);
    } else {
      s = fmt::sprintf("|%3s|%-29s|%4s|%3s|%-29s|%4s|", lnum, s4, lsize, rnum, s5, rsize);
    }
    bout.bpla(s, &abort);
    bout.Color(0);
    i++;
    if (i2 > 10) {
      i2 = 0;
      std::string s1;
      if (okansi()) {
        s1 = fmt::sprintf("|#7\xC3\xC4\xC4\xC4X%sX\xC4\xC4\xC4\xC4X\xC4\xC4\xC4X\xC4%s|#1\xFE|#"
                          "7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xFE\xC4\xC4\xC4\xC4\xD9",
                          s2, s3, t);
      } else {
        s1 = fmt::sprintf("+---+%s+----+---+%s-o-%s-o-+----+", s2, s3, t);
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
      bout.nl();
      bout.pausescr();
      gfl_hdr(1);
    }
  }
  if (!abort) {
    if (okansi()) {
      s = fmt::sprintf("|#7\xC3\xC4\xC4\xC4\xC1%s\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC1%"
                       "s\xC1\xC4\xC4\xC4\xC4\xB4",
                       s2, s2);
    } else {
      s = fmt::sprintf("+---+%s+----+---+%s+----+", s2, s2);
    }
    bout.bpla(s, &abort);
    bout.Color(0);
    if (so()) {
      std::string s1 = okansi()
        ? "|#7\xB3 |#1A|#7)|#2Add a G-File  |#1D|#7)|#2Download a G-file  |#1E|#7)|#2Edit this section  |#1R|#7)|#2Remove a G-File |#7\xB3"
        : "| A)Add a G-File  D)Download a G-file  E)Edit this section  R)Remove a G-File |";
      bout.bpla(s1, &abort);
      bout.Color(0);
    } else {
      std::string s1;
      if (okansi()) {
        s1 = StrCat("|#7\xB3  |#2D  |#1Download a G-file", std::string(55, ' '), "|#7\xB3");
      } else {
        s1 = StrCat("|  D  Download a G-file", std::string(55, ' '), "|");
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
    }
  }
  std::string s1;
  if (okansi()) {
    s1 = fmt::sprintf(
        "|#7\xC0\xC4\xC4\xC4\xC4%s\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4%"
        "s|#1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xC4\xC4\xC4\xD9",
        s2, s3, t);
  } else {
    s1 = fmt::sprintf("+----%s----------------%so-%s-o----+",
        s2, s3, t);
  }
  bout.bpla(s1, &abort);
  bout.Color(0);
  bout.nl();
}

static void gfile_sec(int sn) {
  int i, i1, i2;
  bool abort;

  const auto& section = a()->gfiles().dir(sn);
  auto& g = a()->gfiles().dir(sn).files;
  const auto nf = size_int(g);
  std::set<char> odc;
  for (i = 1; i <= nf / 10; i++) {
    odc.insert(static_cast<char>(i + '0'));
  }
  list_gfiles(a()->gfiles().dir(sn));
  bool done = false;
  while (!done && !a()->sess().hangup()) {
    a()->tleft(true);
    bout << "|#9Current G|#1-|#9File Section |#1: |#5" << section.name << "|#0\r\n";
    bout << "|#9Which G|#1-|#9File |#1(|#21|#1-|#2" << size_int(g) <<
                       "|#1), |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist|#1) : |#5";
    std::string ss = mmkey(odc);
    i = to_number<int>(ss);
    if (ss == "Q") {
      done = true;
    } else if (ss == "E" && so()) {
      done = true;
      gfiles3(sn);
    }
    if (ss == "A" && so()) {
      fill_sec(sn);
      g = a()->gfiles().dir(sn).files;
      odc.clear();
      for (i = 1; i <= nf / 10; i++) {
        odc.insert(static_cast<char>(i + '0'));
      }
    } else if (ss == "R" && so()) {
      bout.nl();
      bout << "|#2G-file number to delete? ";
      std::string ss1 = mmkey(odc);
      i = to_number<int>(ss1);
      if (i > 0 && i <= nf) {
        bout << "|#9Remove " << g[i - 1].description << "|#1? |#5";
        if (bin.yesno()) {
          bout << "|#5Erase file too? ";
          if (bin.yesno()) {
            const auto file_name = FilePath(section.filename, g[i - 1].filename);
            File::Remove(FilePath(a()->config()->gfilesdir(), file_name));
          }
          erase_at(g, i-1);
          a()->gfiles().Save();
          bout << "\r\nDeleted.\r\n\n";
        }
      }
    } else if (ss == "?") {
      list_gfiles(section);
    } else if (ss == "Q") {
      done = true;
    } else if (i > 0 && i <= nf) {
      auto file_name = FilePath(a()->config()->gfilesdir(),
                                FilePath(section.filename, g[i - 1].filename));
      i1 = bout.printfile_path(file_name);
      a()->user()->gfiles_read(a()->user()->gfiles_read() + 1);
      if (i1 == 0) {
        sysoplog() << fmt::format("Read G-file '{}'", g[i - 1].filename);
      }
    } else if (ss == "D") {
      bool done1 = false;
      while (!done1 && !a()->sess().hangup()) {
        bout << "|#9Download which G|#1-|#9file |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist) : |#5";
        ss = mmkey(odc);
        i2 = to_number<int>(ss);
        abort = false;
        if (ss == "?") {
          list_gfiles(section);
          bout << "|#9Current G|#1-|#9File Section |#1: |#5" << section.name << wwiv::endl;
        } else if (ss == "Q") {
          list_gfiles(section);
          done1 = true;
        } else if (!abort) {
          if (i2 > 0 && i2 <= nf) {
            auto file_name = FilePath(section.filename, g[i2 - 1].filename);
            File file(FilePath(a()->config()->datadir(), file_name));
            if (!file.Open(File::modeReadOnly | File::modeBinary)) {
              bout << "|#6File not found : [" << file << "]";
            } else {
              auto file_size = file.length();
              file.Close();
              bool sent = false;
              abort = false;
              send_file(file_name, &sent, &abort, g[i2 - 1].filename, -1, file_size);
              std::string s1;
              if (sent) {
                s1 = fmt::format("|#2{} |#9successfully transferred|#1.|#0\r\n", g[i2 - 1].filename);
                done1 = true;
              } else {
                s1 = fmt::format("|#6\xFE |#9Error transferring |#2{}|#1.|#0", g[i2 - 1].filename);
                done1 = true;
              }
              bout.nl();
              bout << s1;
              bout.nl();
              sysoplog() << s1;
            }
          } else {
            done1 = true;
          }
        }
      }
    }
  }
}

void gfiles2() {
  write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_ONLINE);
  sysoplog() << "@ Ran Gfile Edit";
  gfileedit();
  gfiles();
}

void gfiles3(int n) {
  write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_ONLINE);
  sysoplog() << "@ Ran Gfile Edit";
  modify_sec(n);
  gfile_sec(n);
}

void gfiles() {
  std::vector<int> map;

  bool done = false;
  int nmap = 0;
  int current_section = 0;
  std::set<char> odc;
  for (const auto& r : a()->gfiles().dirs()) {
    if (wwiv::bbs::check_acs(r.acs)) {
      map.push_back(current_section);
      ++nmap;
      if ((nmap % 10) == 0) {
        odc.insert(static_cast<char>('0' + (nmap / 10)));
      }
    }
    current_section++;
  }
  if (nmap == 0) {
    bout << "\r\nNo G-file sections available.\r\n\n";
    return;
  }
  list_sec(map);
  while (!done && !a()->sess().hangup()) {
    a()->tleft(true);
    bout << "|#9G|#1-|#9Files Main Menu|#0\r\n";
    bout << "|#9Which Section |#1(|#21|#1-|#2" << nmap <<
                       "|#1), |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist|#1) : |#5";
    std::string ss = mmkey(odc);
    if (ss == "Q") {
      done = true;
    } else if (ss == "G" && so()) {
      done = true;
      gfiles2();
    } else if (ss == "A" && cs()) {
      bool bIsSectionFull = false;
      for (int i = 0; i < nmap && !bIsSectionFull; i++) {
        bout.nl();
        bout << "Now loading files for " << a()->gfiles()[map[i]].name << "\r\n\n";
        bIsSectionFull = fill_sec(map[i]);
      }
    } else {
      if (int i = to_number<int>(ss); i > 0 && i <= nmap) {
        gfile_sec(map[i-1]);
      }
    }
    if (!done) {
      list_sec(map);
    }
  }
}
