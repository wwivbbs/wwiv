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

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "bbs/batch.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsovl3.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/sysoplog.h"
#include "bbs/sr.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "bbs/xfertmp.h"
#include "bbs/vars.h"
#include "bbs/external_edit.h"
#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/xfer_common.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/file.h"
#include "core/textfile.h"
#include "core/datetime.h"
#include "sdk/filenames.h"

// How far to indent extended descriptions
static const int INDENTION = 24;

// the archive type to use
static const int ARC_NUMBER = 0;

extern int foundany;
static const unsigned char *invalid_chars =
  (unsigned char *)"Ú¿ÀÙÄ³Ã´ÁÂÉ»È¼ÍºÌ¹ÊËÕ¸Ô¾Í³ÆµÏÑÖ·Ó½ÄºÇ¶ÐÒÅÎØ×°±²ÛßÜÝÞ";

using std::string;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void modify_extended_description(std::string* sss, const std::string& dest) {
  char s[255], s1[255];
  int i, i2;

  bool ii = !sss->empty();
  int i4  = 0;
  do {
    if (ii) {
      bout.nl();
      if (okfsed() && a()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
        bout << "|#5Modify the extended description? ";
      } else {
        bout << "|#5Enter a new extended description? ";
      }
      if (!yesno()) {
        return;
      }
    } else {
      bout.nl();
      bout << "|#5Enter an extended description? ";
      if (!yesno()) {
        return;
      }
    }
    if (okfsed() && a()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
      if (!sss->empty()) {
        TextFile file(a()->temp_directory(), "extended.dsc", "w");
        file.Write(*sss);
      } else {
        File::Remove(StrCat(a()->temp_directory(), "extended.dsc"));
      }

      const int saved_screen_chars = a()->user()->GetScreenChars();
      if (a()->user()->GetScreenChars() > (76 - INDENTION)) {
        a()->user()->SetScreenChars(76 - INDENTION);
      }

      bool bEditOK = external_text_edit("extended.dsc", a()->temp_directory(),
          a()->max_extend_lines, MSGED_FLAG_NO_TAGLINE);
      a()->user()->SetScreenChars(saved_screen_chars);
      if (bEditOK) {
        TextFile file(a()->temp_directory(), "extended.dsc", "r");
        *sss = file.ReadFileIntoString();

        for (int i3 = sss->size() - 1; i3 >= 0; i3--) {
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
      int nSavedScreenSize = a()->user()->GetScreenChars();
      if (a()->user()->GetScreenChars() > (76 - INDENTION)) {
        a()->user()->SetScreenChars(76 - INDENTION);
      }
      do {
        bout << "|#2" << i << ": |#0";
        s1[0] = '\0';
        bool bAllowPrevious = (i4 > 0) ? true : false;
        while (inli(s1, s, 90, true, bAllowPrevious, false, true)) {
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
          } while (sss->at(i4) != 10 && i4 != 0);
          if (i4) {
            ++i4;
          }
          sss->resize(i4);
          s[i2] = 0;
          strrev(s);
          if (strlen(s) > static_cast<unsigned int>(a()->user()->GetScreenChars() - 1)) {
            s[ a()->user()->GetScreenChars() - 2 ] = '\0';
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
      } while ((i++ < a()->max_extend_lines) && (s1[0]));
      a()->user()->SetScreenChars(nSavedScreenSize);
    }
    bout << "|#5Is this what you want? ";
    i = !yesno();
    if (i) {
      sss->clear();
    }
  } while (i);
}

bool valid_desc(const string& description) {
  // I don't think this function is really doing what it should
  // be doing, but am not sure what it should be doing instead.
  for (const auto& c : description) {
    if (c > '@' && c < '{') {
      return true;
    }
  }
  return false;
}

// TODO(rushfan): This is probably completely broken
bool get_file_idz(uploadsrec * u, int dn) {
  char *b, cmd[MAX_PATH], s[81];

  if (a()->HasConfigFlag(OP_FLAGS_READ_CD_IDZ) && (a()->directories[dn].mask & mask_cdrom)) {
    return false;
  }
  sprintf(s, "%s%s", a()->directories[dn].path, stripfn(u->filename));
  File f(s);
  auto t = time_t_to_mmddyy(f.creation_time());
  to_char_array(u->actualdate, t);
  {
    char* ss = strchr(stripfn(u->filename), '.');
    if (ss == nullptr) {
      return false;
    }
    ++ss;
    bool ok = false;
    for (auto i = 0; i < MAX_ARCS; i++) {
      if (!ok) {
        ok = IsEqualsIgnoreCase(ss, a()->arcs[i].extension);
      }
    }
    if (!ok) {
      return false;
    }
  }

  File::Remove(a()->temp_directory(), FILE_ID_DIZ);
  File::Remove(a()->temp_directory(), DESC_SDI);

  File::set_current_directory(a()->directories[dn].path);
  {
    File file(FilePath(File::current_directory(), stripfn(u->filename)));
	  a()->CdHome();
	  get_arc_cmd(cmd, file.full_pathname().c_str(), 1, "FILE_ID.DIZ DESC.SDI");
  }
  File::set_current_directory(a()->temp_directory());
  ExecuteExternalProgram(cmd, EFLAG_NOHUP);
  a()->CdHome();
  sprintf(s, "%s%s", a()->temp_directory().c_str(), FILE_ID_DIZ);
  if (!File::Exists(s)) {
    sprintf(s, "%s%s", a()->temp_directory().c_str(), DESC_SDI);
  }
  if (File::Exists(s)) {
    bout.nl();
    bout << "|#9Reading in |#2" << stripfn(s) << "|#9 as extended description...";
    string ss = read_extended_description(u->filename);
    if (!ss.empty()) {
      delete_extended_description(u->filename);
    }
    if ((b = static_cast<char *>(BbsAllocA(a()->max_extend_lines * 256 + 1))) == nullptr) {
      return false;
    }
    File file(s);
    file.Open(File::modeBinary | File::modeReadOnly);
    if (file.length() < (a()->max_extend_lines * 256)) {
      long lFileLen = file.length();
      file.Read(b, lFileLen);
      b[ lFileLen ] = 0;
    } else {
      file.Read(b, a()->max_extend_lines * 256);
      b[a()->max_extend_lines * 256] = 0;
    }
    file.Close();
    if (a()->HasConfigFlag(OP_FLAGS_IDZ_DESC)) {
      ss = strtok(b, "\n");
      if (!ss.empty()) {
        for (size_t i = 0; i < ss.size(); i++) {
          if ((strchr(reinterpret_cast<char*>(const_cast<unsigned char*>(invalid_chars)), ss[i]) != nullptr) && (ss[i] != CZ)) {
            ss[i] = '\x20';
          }
        }
        if (!valid_desc(ss)) {
          do {
            ss = strtok(nullptr, "\n");
          } while (!valid_desc(ss));
        }
      }
      if (ss.back() == '\r') ss.pop_back();
      sprintf(u->description, "%.55s", ss.c_str());
      ss = strtok(nullptr, "");
    } else {
      ss = b;
    }
    if (!ss.empty()) {
      for (auto i = ss.size() - 1; i > 0; i--) {
        if (ss[i] == CZ || ss[i] == 12) {
          ss[i] = '\x20';
        }
      }
      add_extended_description(u->filename, ss);
      u->mask |= mask_extended;
    }
    free(b);
    bout << "Done!\r\n";
  }
  File::Remove(a()->temp_directory(), FILE_ID_DIZ);
  File::Remove(a()->temp_directory(), DESC_SDI);
  return true;
}

int read_idz_all() {
  int count = 0;

  tmp_disable_conf(true);
  TempDisablePause disable_pause;
  a()->ClearTopScreenProtection();
  for (size_t i = 0; (i < a()->directories.size()) && (a()->udir[i].subnum != -1) &&
       (!a()->localIO()->KeyPressed()); i++) {
    count += read_idz(0, i);
  }
  tmp_disable_conf(false);
  a()->UpdateTopScreen();
  return count;
}

int read_idz(int mode, int tempdir) {
  int i, count = 0;
  bool abort = false;
  uploadsrec u;

  std::unique_ptr<TempDisablePause> disable_pause;
  std::string s = "*.*";
  if (mode) {
    disable_pause.reset(new TempDisablePause);
    a()->ClearTopScreenProtection();
    dliscan();
    s = file_mask();
  } else {
    align(&s);
    dliscan1(a()->udir[tempdir].subnum);
  }
  bout.bprintf("|#9Checking for external description files in |#2%-25.25s #%s...\r\n",
                                    a()->directories[a()->udir[tempdir].subnum].name,
                                    a()->udir[tempdir].keys);
  File fileDownload(a()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  for (i = 1; (i <= a()->numf) && (!hangup) && !abort; i++) {
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    if ((compare(s.c_str(), u.filename)) &&
        (strstr(u.filename, ".COM") == nullptr) &&
        (strstr(u.filename, ".EXE") == nullptr)) {
      File::set_current_directory(a()->directories[a()->udir[tempdir].subnum].path);
      File file(FilePath(File::current_directory(), stripfn(u.filename)));
      a()->CdHome();
      if (file.Exists()) {
        if (get_file_idz(&u, a()->udir[tempdir].subnum)) {
          count++;
        }
        FileAreaSetRecord(fileDownload, i);
        fileDownload.Write(&u, sizeof(uploadsrec));
      }
    }
    checka(&abort);
  }
  fileDownload.Close();
  if (mode) {
    a()->UpdateTopScreen();
  }
  return count;
}

void tag_it() {
  int i, i3, i4;
  double t = 0.0;
  char s[255], s1[255], s2[81], s3[400];
  long fs = 0;

  if (a()->batch().entry.size() >= a()->max_batch) {
    bout << "|#6No room left in batch queue.";
    bout.getkey();
    return;
  }
  bout << "|#2Which file(s) (1-" << a()->filelist.size() 
       << ", *=All, 0=Quit)? ";
  input(s3, 30, true);
  if (s3[0] == '*') {
    s3[0] = '\0';
    for (size_t i2 = 0; i2 < a()->filelist.size() && i2 < 78; i2++) {
      sprintf(s2, "%u ", i2 + 1);
      strcat(s3, s2);
      if (strlen(s3) > sizeof(s3) - 10) {
        break;
      }
    }
    bout << "\r\n|#2Tagging: |#4" << s3 << wwiv::endl;
  }
  for (int i2 = 0; i2 < GetStringLength(s3); i2++) {
    sprintf(s1, "%s", s3 + i2);
    i4 = 0;
    bool bad = false;
    for (i3 = 0; i3 < GetStringLength(s1); i3++) {
      if ((s1[i3] == ' ') || (s1[i3] == ',') || (s1[i3] == ';')) {
        s1[i3] = 0;
        i4 = 1;
      } else {
        if (i4 == 0) {
          i2++;
        }
      }
    }
    i = to_number<int>(s1);
    if (i == 0) {
      break;
    }
    i--;
    if (s1[0] && i >= 0 && i < size_int(a()->filelist)) {
    auto& f = a()->filelist[i];
      if (check_batch_queue(f.u.filename)) {
        bout << "|#6" << f.u.filename << " is already in the batch queue.\r\n";
        bad = true;
      }
      if (a()->batch().entry.size() >= a()->max_batch) {
        bout << "|#6Batch file limit of " << a()->max_batch << " has been reached.\r\n";
        bad = true;
      }
      if ((a()->config()->req_ratio() > 0.0001) && (ratio() < a()->config()->req_ratio()) &&
          !a()->user()->IsExemptRatio() && !bad) {
        bout.bprintf("|#2Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n",
                                          ratio(), a()->config()->req_ratio());
        bad = true;
      }
      if (!bad) {
        sprintf(s, "%s%s", a()->directories[f.directory].path,
                stripfn(f.u.filename));
        if (f.dir_mask & mask_cdrom) {
          sprintf(s2, "%s%s", a()->directories[f.directory].path,
                  stripfn(f.u.filename));
          sprintf(s, "%s%s", a()->temp_directory().c_str(), stripfn(f.u.filename));
          if (!File::Exists(s)) {
            copyfile(s2, s, true);
          }
        }
        File fp(s);
        if (!fp.Open(File::modeBinary | File::modeReadOnly)) {
          bout << "|#6The file " << stripfn(f.u.filename) << " is not there.\r\n";
          bad = true;
        } else {
          fs = fp.length();
          fp.Close();
        }
      }
      if (!bad) {
        t = 12.656 / static_cast<double>(a()->modem_speed_) * static_cast<double>(fs);
        if (nsl() <= (a()->batch().dl_time_in_secs() + t)) {
          bout << "|#6Not enough time left in queue for " << f.u.filename << ".\r\n";
          bad = true;
        }
      }
      if (!bad) {
        batchrec b{};
        strcpy(b.filename, f.u.filename);
        b.dir = f.directory;
        b.time = (float) t;
        b.sending = true;
        b.len = fs;
        a()->batch().entry.emplace_back(b);
        bout << "|#1" << f.u.filename << " added to batch queue.\r\n";
      }
    } else {
      bout << "|#6Bad file number " << i + 1 << wwiv::endl;
    }
    bout.clear_lines_listed();
  }
}

static char fancy_prompt(const char *pszPrompt, const char *pszAcceptChars) {
  char s1[81], s2[81], s3[81];
  char ch = 0;

  a()->tleft(true);
  sprintf(s1, "\r|#2%s (|#1%s|#2)? |#0", pszPrompt, pszAcceptChars);
  sprintf(s2, "%s (%s)? ", pszPrompt, pszAcceptChars);
  int i1 = strlen(s2);
  sprintf(s3, "%s%s", pszAcceptChars, " \r");
  a()->tleft(true);
  if (okansi()) {
    bout << s1;
    ch = onek_ncr(s3);
    bout << "\x1b[" << i1 << "D";
    for (int i = 0; i < i1; i++) {
      bout.bputch(' ');
    }
    bout << "\x1b[" << i1 << "D";
  } else {
    bout << s2;
    ch = onek_ncr(s3);
    for (int i = 0; i < i1; i++) {
      bout.bs();
    }
  }
  return ch;
}

void tag_files(bool& need_title) {
  int i;
  char s[255], s1[255], s2[81], ch;
  bool had = false;
  double d;

  if (bout.lines_listed() == 0) {
    return;
  }
  bool abort = false;
  a()->tleft(true);
  if (hangup) {
    return;
  }
  bout.clear_lines_listed();
  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;

  bool done = false;
  while (!done && !hangup) {
    bout.clear_lines_listed();
    ch = fancy_prompt("File Tagging", "CDEMQRTV?");
    bout.clear_lines_listed();
    switch (ch) {
    case '?':
    {
      print_help_file(TTAGGING_NOEXT);
      pausescr();
      relist();
    } break;
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
      if (!had) {
        bout.nl();
        pausescr();
        bout.cls();
      }
      done = true;
      break;
    case 'E':
    {
      bout.clear_lines_listed();
      bout << "|#9Which file (1-" << a()->filelist.size() << ")? ";
      input(s, 2, true);
      i = to_number<int>(s) - 1;
      if (s[0] && i >= 0 && i < size_int(a()->filelist)) {
        auto& f = a()->filelist[i];
        d = XFER_TIME(f.u.numbytes);
        bout.nl();
        size_t i2;
        for (i2 = 0; i2 < a()->directories.size(); i2++) {
          if (a()->udir[i2].subnum == f.directory) {
            break;
          }
        }
        if (i2 < a()->directories.size()) {
          bout << "|#1Directory  : |#2#" << a()->udir[i2].keys << ", " << a()->directories[f.directory].name <<
            wwiv::endl;
        } else {
          bout << "|#1Directory  : |#2#" << "??" << ", " << a()->directories[f.directory].name << wwiv::endl;
        }
        bout << "|#1Filename   : |#2" << f.u.filename << wwiv::endl;
        bout << "|#1Description: |#2" << f.u.description << wwiv::endl;
        if (f.u.mask & mask_extended) {
          to_char_array(s1, a()->extended_description_filename_);
          a()->extended_description_filename_ = 
              FilePath(a()->config()->datadir(), StrCat(a()->directories[f.directory].filename, ".ext"));
          zap_ed_info();
          bout << "|#1Ext. Desc. : |#2";
          print_extended(f.u.filename, &abort, a()->max_extend_lines, 2);
          zap_ed_info();
          a()->extended_description_filename_ = s1;
        }
        bout << "|#1File size  : |#2" << bytes_to_k(f.u.numbytes) << wwiv::endl;
        bout << "|#1Apprx. time: |#2" << ctim(std::lround(d)) << wwiv::endl;
        bout << "|#1Uploaded on: |#2" << f.u.date << wwiv::endl;
        bout << "|#1Uploaded by: |#2" << f.u.upby << wwiv::endl;
        bout << "|#1Times D/L'd: |#2" << f.u.numdloads << wwiv::endl;
        if (a()->directories[f.directory].mask & mask_cdrom) {
          bout.nl();
          bout << "|#3CD ROM DRIVE\r\n";
        } else {
          sprintf(s, "|#7%s%s", a()->directories[f.directory].path, f.u.filename);
          if (!File::Exists(s)) {
            bout.nl();
            bout << "|#6-=>FILE NOT THERE<=-\r\n";
          }
        }
        bout.nl();
        pausescr();
        relist();

      }
    } break;
    case 'M':
      if (dcs()) {
        move_file_t();
        if (a()->filelist.empty()) {
          done = true;
          return;
        }
        relist();
      }
      break;
    case 'Q':
      a()->filelist.clear();
      bout.clear_lines_listed();
      need_title = false;
      done = true;
      return;
    case 'R':
      relist();
      break;
    case 'T':
      tag_it();
      break;
    case 'V':
      bout << "|#2Which file (1-|#2" << a()->filelist.size() << ")? ";
      input(s, 2, true);
      i = to_number<int>(s) - 1;
      if ((s[0]) && (i >= 0) && (i < size_int(a()->filelist))) {
        auto& f = a()->filelist[i];
        sprintf(s1, "%s%s", a()->directories[f.directory].path,
                stripfn(f.u.filename));
        if (a()->directories[f.directory].mask & mask_cdrom) {
          sprintf(s2, "%s%s", a()->directories[f.directory].path,
                  stripfn(f.u.filename));
          sprintf(s1, "%s%s", a()->temp_directory().c_str(),
                  stripfn(f.u.filename));
          if (!File::Exists(s1)) {
            copyfile(s2, s1, true);
          }
        }
        if (!File::Exists(s1)) {
          bout << "|#6File not there.\r\n";
          pausescr();
          break;
        }
        get_arc_cmd(s, s1, 0, "");
        if (!okfn(stripfn(f.u.filename))) {
          s[0] = 0;
        }
        if (s[0] != 0) {
          bout.nl();
          ExecuteExternalProgram(s, a()->spawn_option(SPAWNOPT_ARCH_L));
          bout.nl();
          pausescr();
          a()->UpdateTopScreen();
          bout.cls();
          relist();
        } else {
          bout << "|#6Unknown archive.\r\n";
          pausescr();
          break;
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


int add_batch(char *description, const char *file_name, int dn, long fs) {
  char ch;
  char s1[81], s2[81];
  int i;

  if (find_batch_queue(file_name) > -1) {
    return 0;
  }

  double t = 0.0;
  if (a()->modem_speed_) {
    t = (12.656) / ((double)(a()->modem_speed_)) * ((double)(fs));
  }

  if (nsl() <= (a()->batch().dl_time_in_secs() + t)) {
    bout << "|#6 Insufficient time remaining... press any key.";
    bout.getkey();
  } else {
    if (dn == -1) {
      return 0;
    } else {
      for (i = 0; i < GetStringLength(description); i++) {
        if (description[i] == RETURN) {
          description[i] = SPACE;
        }
      }
      bout.backline();
      bout.bprintf(" |#6? |#1%s %3luK |#5%-43.43s |#7[|#2Y/N/Q|#7] |#0", file_name,
                                        bytes_to_k(fs), stripcolors(description));
      ch = onek_ncr("QYN\r");
      bout.backline();
      if (to_upper_case<char>(ch) == 'Y') {
        if (a()->directories[dn].mask & mask_cdrom) {
          sprintf(s2, "%s%s", a()->directories[dn].path, file_name);
          sprintf(s1, "%s%s", a()->temp_directory().c_str(), file_name);
          if (!File::Exists(s1)) {
            if (!copyfile(s2, s1, true)) {
              bout << "|#6 file unavailable... press any key.";
              bout.getkey();
            }
            bout.backline();
            bout.clreol();
          }
        } else {
          sprintf(s2, "%s%s", a()->directories[dn].path, file_name);
          StringRemoveWhitespace(s2);
          if ((!File::Exists(s2)) && (!so())) {
            bout << "\r";
            bout.clreol();
            bout << "|#6 file unavailable... press any key.";
            bout.getkey();
            bout << "\r";
            bout.clreol();
            return 0;
          }
        }
        batchrec b{};
        strcpy(b.filename, file_name);
        b.dir = static_cast<int16_t>(dn);
        b.time = static_cast<float>(t);
        b.sending = true;
        b.len = fs;
        bout << "\r";
        const string bt = ctim(std::lround(b.time));
        bout.bprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n",
                     a()->batch().entry.size() + 1, b.filename, b.len,
                     bt.c_str(),
                     a()->directories[b.dir].name);
        a()->batch().entry.emplace_back(b);
        bout << "\r";
        bout << "|#5    Continue search? ";
        ch = onek_ncr("YN\r");
        if (to_upper_case<char>(ch) == 'N') {
          return -3;
        } else {
          return 1;
        }
      } else if (ch == 'Q') {
        bout.backline();
        return -3;
      } else {
        bout.backline();
      }
    }
  }
  return 0;
}

int try_to_download(const char *file_mask, int dn) {
  int rtn;
  bool abort = false;
  bool ok = false;
  uploadsrec u;
  char s1[81], s3[81];

  dliscan1(dn);
  int i = recno(file_mask);
  if (i <= 0) {
    checka(&abort);
    return ((abort) ? -1 : 0);
  }
  ok = true;
  foundany = 1;
  do {
    a()->tleft(true);
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();

    if (!(u.mask & mask_no_ratio) && !ratio_ok()) {
      return -2;
    }

    write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
    sprintf(s1, "%s%s", a()->directories[dn].path, u.filename);
    sprintf(s3, "%-40.40s", u.description);
    abort = false;
    rtn = add_batch(s3, u.filename, dn, u.numbytes);
    s3[0] = 0;

    if (abort || rtn == -3) {
      ok = false;
    } else {
      i = nrecno(file_mask, i);
    }
  } while (i > 0 && ok && !hangup);
  if (rtn == -2) {
    return -2;
  }
  if (abort || rtn == -3) {
    return -1;
  } else {
    return 1;
  }
}

void download() {
  char ch, s[81], s1[81];
  int i = 0, color = 0, count;
  bool ok = true;
  int ip, rtn = 0, useconf;
  bool done = false;

  useconf = 0;

  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " Batch Downloads"));
  do {
    if (!i) {
      bout << "|#2Enter files, one per line, wildcards okay.  [Space] aborts a search.\r\n";
      bout.nl();
      bout << "|#1 #  File Name    Size    Time      Directory\r\n";
      bout << "|#7\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\r\n";
    }
    if (i < size_int(a()->batch().entry)) {
      const auto& b = a()->batch().entry[i];
      if (b.sending) {
        const auto t = ctim(std::lround(b.time));
        bout.bprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n", 
          i + 1, b.filename,
          b.len, t.c_str(), 
          a()->directories[b.dir].name);
      }
    } else {
      do {
        count = 0;
        ok = true;
        bout.backline();
        bout.bprintf("|#2%3d ", a()->batch().entry.size() + 1);
        bout.Color(1);
        bool onl = bout.newline;
        bout.newline = false;
        input(s, 12);
        bout.newline = onl;
        if ((s[0]) && (s[0] != ' ')) {
          if (strchr(s, '.') == nullptr) {
            strcat(s, ".*");
          }
          align(s);
          rtn = try_to_download(s, a()->current_user_dir().subnum);
          if (rtn == 0) {
            if (a()->uconfdir[1].confnum != -10 && okconf(a()->user())) {
              bout.backline();
              bout << " |#5Search all conferences? ";
              ch = onek_ncr("YN\r");
              if (ch == '\r' || to_upper_case<char>(ch) == 'Y') {
                tmp_disable_conf(true);
                useconf = 1;
              }
            }
            bout.backline();
            sprintf(s1, "%3d %s", a()->batch().entry.size() + 1, s);
            bout.Color(1);
            bout << s1;
            foundany = 0;
            size_t dn = 0;
            while ((dn < a()->directories.size()) && (a()->udir[dn].subnum != -1)) {
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
              } else {
                dn++;
              }
            }
            if (useconf) {
              tmp_disable_conf(false);
            }
            if (!foundany) {
              bout << "|#6 File not found... press any key.";
              bout.getkey();
              bout.backline();
              ok = false;
            }
          }
        } else {
          bout.backline();
          done = true;
        }
      } while (!ok && !hangup);
    }
    i++;
    if (rtn == -2) {
      rtn = 0;
      i = 0;
    }
  } while (!done && !hangup && (i <= size_int(a()->batch().entry)));

  if (!a()->batch().numbatchdl()) {
    return;
  }

  bout.nl();
  if (!ratio_ok()) {
    bout << "\r\nSorry, your ratio is too low.\r\n\n";
    done = true;
    return;
  }
  bout.nl();
  bout << "|#1Files in Batch Queue   : |#2" << a()->batch().entry.size() << wwiv::endl;
  bout << "|#1Estimated Download Time: |#2" << ctim(a()->batch().dl_time_in_secs()) << wwiv::endl;
  bout.nl();
  rtn = batchdl(3);
  if (rtn) {
    return;
  }
  bout.nl();
  if (a()->batch().entry.empty()) {
    return;
  }
  bout << "|#5Hang up after transfer? ";
  bool had = yesno();
  ip = get_protocol(xf_down_batch);
  if (ip > 0) {
    switch (ip) {
    case WWIV_INTERNAL_PROT_YMODEM: {
      if (a()->over_intern.size() > 0
          && (a()->over_intern[2].othr & othr_override_internal)
          && (a()->over_intern[2].sendbatchfn[0])) {
        dszbatchdl(had, a()->over_intern[2].sendbatchfn, prot_name(WWIV_INTERNAL_PROT_YMODEM));
      } else {
        ymbatchdl(had);
      }
    }
    break;
    case WWIV_INTERNAL_PROT_ZMODEM: {
      zmbatchdl(had);
    } break;
    default: {
      dszbatchdl(had, a()->externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn,
        a()->externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].description);
    }
    }
    if (!had) {
      bout.nl();
      bout << "Your ratio is now: " << ratio() << wwiv::endl;
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

void SetNewFileScanDate() {
  char ag[10];
  bool ok = true;

  bout.nl();
  bout << "|#9Current limiting date: |#2" << daten_to_mmddyy(nscandate) << "\r\n";
  bout.nl();
  bout << "|#9Enter new limiting date in the following format: \r\n";
  bout << "|#1 MM/DD/YY\r\n|#7:";
  bout.mpl(8);
  int i = 0;
  char ch = 0;
  do {
    if (i == 2 || i == 5) {
      ag[i++] = '/';
      bout.bputch('/');
    } else {
      switch (i) {
      case 0:
        ch = onek_ncr("01\r");
        break;
      case 3:
        ch = onek_ncr("0123\b");
        break;
      case 8:
        ch = onek_ncr("\b\r");
        break;
      default:
        ch = onek_ncr("0123456789\b");
        break;
      }
      if (hangup) {
        ok = false;
        ag[0] = '\0';
        break;
      }
      switch (ch) {
      case '\r':
        switch (i) {
        case 0:
          ok = false;
          break;
        case 8:
          ag[8] = '\0';
          break;
        default:
          ch = '\0';
          break;
        }
        break;
      case BACKSPACE:
        bout << " \b";
        --i;
        if (i == 2 || i == 5) {
          bout.bs();
          --i;
        }
        break;
      default:
        ag[i++] = ch;
        break;
      }
    }
  } while (ch != '\r' && !hangup);

  bout.nl();
  if (ok) {
    int m = atoi(ag);
    int dd = atoi(&(ag[3]));
    int y = atoi(&(ag[6])) + 1900;
    if (y < 1920) {
      y += 100;
    }
    struct tm newTime;
    if ((((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (dd >= 31)) ||
        ((m == 2) && (((y % 4 != 0) && (dd == 29)) || (dd == 30))) ||
        (dd > 31) || ((m == 0) || (y == 0) || (dd == 0)) ||
        ((m > 12) || (dd > 31))) {
      bout.nl();
      bout.bprintf("|#6%02d/%02d/%02d is invalid... date not changed!\r\n", m, dd, (y % 100));
      bout.nl();
    } else {
      // Rushfan - Note, this needs a better fix, this whole routine should be replaced.
      newTime.tm_min  = 0;
      newTime.tm_hour = 1;
      newTime.tm_sec  = 0;
      newTime.tm_year = y - 1900;
      newTime.tm_mday = dd;
      newTime.tm_mon  = m - 1;
    }
    bout.nl();
    nscandate = time_t_to_daten(mktime(&newTime));

    // Display the new nscan date
    auto d = daten_to_mmddyyyy(nscandate);
    bout << "|#9New Limiting Date: |#2" << d << "\r\n";

    // Hack to make sure the date covers everythig since we had to increment the hour by one
    // to show the right date on some versions of MSVC
    nscandate -= SECONDS_PER_HOUR;
  } else {
    bout.nl();
  }
}


void removefilesnotthere(int dn, int *autodel) {
  char ch = '\0';
  uploadsrec u;

  dliscan1(dn);
  char szAllFilesFileMask[MAX_PATH];
  strcpy(szAllFilesFileMask, "*.*");
  align(szAllFilesFileMask);
  int i = recno(szAllFilesFileMask);
  bool abort = false;
  while (!hangup && i > 0 && !abort) {
    char szCandidateFileName[MAX_PATH];
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    sprintf(szCandidateFileName, "%s%s", a()->directories[dn].path, u.filename);
    StringRemoveWhitespace(szCandidateFileName);
    if (!File::Exists(szCandidateFileName)) {
      StringTrim(u.description);
      sprintf(szCandidateFileName, "|#2%s :|#1 %-40.40s", u.filename, u.description);
      if (!*autodel) {
        bout.backline();
        bout << szCandidateFileName;
        bout.nl();
        bout << "|#5Remove Entry (Yes/No/Quit/All) : ";
        ch = onek_ncr("QYNA");
      } else {
        bout.nl();
        bout << "|#1Removing entry " << szCandidateFileName;
        ch = 'Y';
      }
      if (ch == 'Y' || ch == 'A') {
        if (ch == 'A') {
          bout << "ll";
          *autodel = 1;
        }
        if (u.mask & mask_extended) {
          delete_extended_description(u.filename);
        }
        sysoplog() << "- '" << u.filename << "' Removed from " << a()->directories[dn].name;
        fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        for (int i1 = i; i1 < a()->numf; i1++) {
          FileAreaSetRecord(fileDownload, i1 + 1);
          fileDownload.Read(&u, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i1);
          fileDownload.Write(&u, sizeof(uploadsrec));
        }
        --i;
        --a()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u, sizeof(uploadsrec));
        u.numbytes = a()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u, sizeof(uploadsrec));
        fileDownload.Close();
      } else if (ch == 'Q') {
        abort = true;
      }
    }
    i = nrecno(szAllFilesFileMask, i);
    bool next = true;
    checka(&abort, &next);
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
  TempDisablePause disable_pause;
  int autodel = 0;
  bout.nl();
  bout << "|#5Remove N/A files in all a()->directories? ";
  if (yesno()) {
    for (size_t i = 0; ((i < a()->directories.size()) && (a()->udir[i].subnum != -1) &&
                     (!a()->localIO()->KeyPressed())); i++) {
      bout.nl();
      bout << "|#1Removing N/A|#0 in " << a()->directories[a()->udir[i].subnum].name;
      bout.nl(2);
      removefilesnotthere(a()->udir[i].subnum, &autodel);
    }
  } else {
    bout.nl();
    bout << "Removing N/A|#0 in " << a()->directories[a()->current_user_dir().subnum].name;
    bout.nl(2);
    removefilesnotthere(a()->current_user_dir().subnum, &autodel);
  }
  tmp_disable_conf(false);
  a()->UpdateTopScreen();
}

int find_batch_queue(const char *file_name) {
  for (size_t i = 0; i < a()->batch().entry.size(); i++) {
    if (IsEquals(file_name, a()->batch().entry[i].filename)) {
      return i;
    }
  }

  return -1;
}

// Removes a file off the batch queue specified by pszFileNam,e
void remove_batch(const char *file_name) {
  int batchNum = find_batch_queue(file_name);
  if (batchNum > -1) {
    delbatch(batchNum);
  }
}

