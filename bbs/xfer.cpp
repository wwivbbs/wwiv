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

#include "bbs/xfer.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/listplus.h"
#include "bbs/make_abs_cmd.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer_common.h"
#include "bbs/xferovl1.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/files/files.h"
#include <cmath>
#include <string>
#include <vector>

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

// How far to indent extended descriptions
static const int INDENTION = 24;

int foundany;
daten_t this_date;

static int ed_num, ed_got;
static ext_desc_rec *ed_info;

using std::string;
using std::vector;

void zap_ed_info() {
  if (ed_info) {
    free(ed_info);
    ed_info = nullptr;
  }
  ed_num = 0;
  ed_got = 0;
}

void get_ed_info() {
  if (ed_got) {
    return;
  }

  zap_ed_info();
  ed_got = 1;

  if (!a()->numf) {
    return;
  }

  long lCurFilePos = 0;
  File fileExtDescr(a()->extended_description_filename_);
  if (fileExtDescr.Open(File::modeReadOnly | File::modeBinary)) {
    auto file_size = fileExtDescr.length();
    if (file_size > 0) {
      ed_info = static_cast<ext_desc_rec *>(BbsAllocA(static_cast<long>(a()->numf) * sizeof(ext_desc_rec)));
      if (ed_info == nullptr) {
        fileExtDescr.Close();
        return;
      }
      ed_num = 0;
      while (lCurFilePos < file_size && ed_num < a()->numf) {
        fileExtDescr.Seek(lCurFilePos, File::Whence::begin);
        ext_desc_type ed;
        int num_read = fileExtDescr.Read(&ed, sizeof(ext_desc_type));
        if (num_read == sizeof(ext_desc_type)) {
          strcpy(ed_info[ed_num].name, ed.name);
          ed_info[ed_num].offset = lCurFilePos;
          lCurFilePos += static_cast<long>(ed.len) + sizeof(ext_desc_type);
          ed_num++;
        }
      }
      if (lCurFilePos < file_size) {
        ed_got = 2;
      }
    }
    fileExtDescr.Close();
  }
}

unsigned long bytes_to_k(unsigned long lBytes) {
  return lBytes ? static_cast<unsigned long>((lBytes + 1023) / 1024) : 0L;
}

int check_batch_queue(const char *file_name) {
  for (const auto& b : a()->batch().entry) {
    if (IsEquals(file_name, b.filename)) {
      return b.sending ? 1 : -1;
    }
  }
  return 0;
}

/**
 * returns true if everything is ok, false if the file
 */
bool check_ul_event(int directory_num, uploadsrec * u) {
  if (a()->upload_cmd.empty()) {
    return true;
  }
  const auto comport = std::to_string(a()->context().incom() ? a()->primary_port() : 0);
  const auto cmdLine =
      stuff_in(a()->upload_cmd, create_chain_file(), a()->directories[directory_num].path,
               stripfn(u->filename), comport, "");
  ExecuteExternalProgram(cmdLine, a()->spawn_option(SPAWNOPT_ULCHK));

  const auto file = PathFilePath(a()->directories[directory_num].path, stripfn(u->filename));
  if (!File::Exists(file)) {
    sysoplog() << "File \"" << u->filename << "\" to " << a()->directories[directory_num].name << " deleted by UL event.";
    bout << u->filename << " was deleted by the upload event.\r\n";
    return false;
  }
  return true;
}

static const std::vector<std::string> device_names = {
  "KBD$", "PRN", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
  "COM8", "LPT1", "LPT2", "LPT3", "CLOCK$", "SCREEN$", "POINTER$", "MOUSE$"
};

bool okfn(const string& filename) {
  if (filename.empty()) {
    return false;
  }

  const auto len = filename.length();
  if (len == 0) {
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
    const auto deviceLen = device.length();
    if (filename.length() >= deviceLen && filename.substr(0, deviceLen) == device) {
      if (filename[deviceLen] == '\0' || filename[deviceLen] == '.' || deviceLen == 8) {
        return false;
      }
    }
  }
  return true;
}

void print_devices() {
  for (const auto& device : device_names) {
    bout << device;
    bout.nl();
  }
}

void get_arc_cmd(char *out_buffer, const char *pszArcFileName, int cmd, const char *ofn) {
  char szArcCmd[MAX_PATH];

  out_buffer[0] = '\0';
  const char* ss = strrchr(pszArcFileName, '.');
  if (ss == nullptr) {
    return;
  }
  ++ss;
  for (int i = 0; i < MAX_ARCS; i++) {
    if (iequals(ss, a()->arcs[i].extension)) {
      switch (cmd) {
      case 0:
        strcpy(szArcCmd, a()->arcs[i].arcl);
        break;
      case 1:
        strcpy(szArcCmd, a()->arcs[i].arce);
        break;
      case 2:
        strcpy(szArcCmd, a()->arcs[i].arca);
        break;
      case 3:
        strcpy(szArcCmd, a()->arcs[i].arcd);
        break;
      case 4:
        strcpy(szArcCmd, a()->arcs[i].arck);
        break;
      case 5:
        strcpy(szArcCmd, a()->arcs[i].arct);
        break;
      default:
        // Unknown type.
        return;
      }

      if (szArcCmd[0] == 0) {
        return;
      }
      auto command = stuff_in(szArcCmd, pszArcFileName, ofn, "", "", "");
      make_abs_cmd(a()->bbsdir().string(), &command);
      strcpy(out_buffer, command.c_str());
      return;
    }
  }
}

int list_arc_out(const char *file_name, const char *pszDirectory) {
  string name_to_delete;

  auto full_pathname = PathFilePath(pszDirectory, file_name);
  if (a()->directories[a()->current_user_dir().subnum].mask & mask_cdrom) {
    full_pathname = PathFilePath(a()->temp_directory(), file_name);
    if (!File::Exists(full_pathname)) {
      auto name_in_dir = PathFilePath(pszDirectory, file_name);
      File::Copy(name_in_dir, full_pathname);
      name_to_delete = full_pathname.string();
    }
  }
  char szArchiveCmd[MAX_PATH];
  get_arc_cmd(szArchiveCmd, full_pathname.string().c_str(), 0, "");
  if (!okfn(file_name)) {
    szArchiveCmd[0] = 0;
  }

  int return_code = 0;
  if (File::Exists(full_pathname) && (szArchiveCmd[0] != 0)) {
    bout.nl(2);
    bout << "Archive listing for " << file_name;
    bout.nl(2);
    return_code = ExecuteExternalProgram(szArchiveCmd, a()->spawn_option(SPAWNOPT_ARCH_L));
  } else {
    bout.nl();
    bout << "Unknown archive: " << file_name;
    bout.nl();
  }
  bout.nl();

  if (!name_to_delete.empty()) {
    File::Remove(name_to_delete);
  }

  return return_code;
}

bool ratio_ok() {
  bool bRetValue = true;

  if (!a()->user()->IsExemptRatio()) {
    if ((a()->config()->req_ratio() > 0.0001) && (ratio() < a()->config()->req_ratio())) {
      bRetValue = false;
      bout.cls();
      bout.nl();
      bout << fmt::sprintf("Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n\n",
                                        ratio(), a()->config()->req_ratio());
    }
  }
  if (!a()->user()->IsExemptPost()) {
    if ((a()->config()->post_to_call_ratio() > 0.0001) &&
        (post_ratio() < a()->config()->post_to_call_ratio())) {
      bRetValue = false;
      bout.cls();
      bout.nl();
      bout << fmt::sprintf("%s %-5.3f.  %s %-5.3f %s.\r\n\n", 
                           "Your post/call ratio is",
                           post_ratio(), "You need a ratio of", a()->config()->post_to_call_ratio(),
                           "to download.");
    }
  }
  return bRetValue;
}

bool dcs() {
  return (cs() || a()->user()->GetDsl() >= 100) ? true : false;
}

void dliscan1(int directory_num) {
  const std::string basename = a()->directories[directory_num].filename;
  a()->download_filename_ = FilePath(a()->config()->datadir(), StrCat(basename, ".dir"));

  if (!a()->fileapi()->Exist(basename)) {
    if (!a()->fileapi()->Create(basename)) {
      LOG(ERROR) << "Failed to create file area: " << basename;
    }
  }

  auto area = a()->fileapi()->Open(a()->directories[directory_num]);

  a()->numf = area->number_of_files();
  this_date = area->header().daten();

  a()->extended_description_filename_ = 
      FilePath(a()->config()->datadir(), StrCat(a()->directories[directory_num].filename, ".ext"));
  zap_ed_info();

  a()->set_current_file_area(std::move(area));
}

void dliscan() {
  dliscan1(a()->current_user_dir().subnum);
}

void add_extended_description(const string& file_name, const string& description) {
  ext_desc_type ed{};

  to_char_array(ed.name, file_name);
  ed.len = static_cast<int16_t>(description.size());

  File file(a()->extended_description_filename_);
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(0L, File::Whence::end);
  file.Write(&ed, sizeof(ext_desc_type));
  file.Write(description.c_str(), ed.len);
  file.Close();

  zap_ed_info();
}

void delete_extended_description(const string& file_name) {
  ext_desc_type ed{};

  File fileExtDescr(a()->extended_description_filename_);
  fileExtDescr.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  auto file_size = fileExtDescr.length();
  long r = 0, w = 0;
  while (r < file_size) {
    fileExtDescr.Seek(r, File::Whence::begin);
    fileExtDescr.Read(&ed, sizeof(ext_desc_type));
    if (ed.len < 10000) {
      auto ss = std::make_unique<char[]>(ed.len);
      fileExtDescr.Read(ss.get(), ed.len);
      if (file_name != ed.name) {
        if (r != w) {
          fileExtDescr.Seek(w, File::Whence::begin);
          fileExtDescr.Write(&ed, sizeof(ext_desc_type));
          fileExtDescr.Write(ss.get(), ed.len);
        }
        w += sizeof(ext_desc_type) + ed.len;
      }
    }
    r += sizeof(ext_desc_type) + ed.len;
  }
  fileExtDescr.set_length(w);
  fileExtDescr.Close();
  zap_ed_info();
}

string read_extended_description(const string& file_name) {
  get_ed_info();

  if (ed_got && ed_info) {
    for (int i = 0; i < ed_num; i++) {
      if (file_name == ed_info[i].name) {
        File fileExtDescr(a()->extended_description_filename_);
        if (!fileExtDescr.Open(File::modeBinary | File::modeReadOnly)) {
          return nullptr;
        }
        fileExtDescr.Seek(ed_info[i].offset, File::Whence::begin);
        ext_desc_type ed;
        int num_read = fileExtDescr.Read(&ed, sizeof(ext_desc_type));
        if (num_read == sizeof(ext_desc_type) && file_name == ed.name) {
          string ss;
          ss.resize(ed.len);
          fileExtDescr.Read(&ss[0], ed.len);
          fileExtDescr.Close();
          return ss;
        } else {
          zap_ed_info();
          fileExtDescr.Close();
          break;
        }
      }
    }
  }
  if (ed_got != 1) {
    File fileExtDescr(a()->extended_description_filename_);
    if (fileExtDescr.Open(File::modeBinary | File::modeReadOnly)) {
      auto file_size = fileExtDescr.length();
      long lCurPos = 0;
      while (lCurPos < file_size) {
        fileExtDescr.Seek(lCurPos, File::Whence::begin);
        ext_desc_type ed;
        lCurPos += static_cast<long>(fileExtDescr.Read(&ed, sizeof(ext_desc_type)));
        if (file_name == ed.name) {
          string ss;
          ss.resize(ed.len);
          fileExtDescr.Read(&ss[0], ed.len);
          fileExtDescr.Close();
          return ss;
        } else {
          lCurPos += static_cast<long>(ed.len);
        }
      }
      fileExtDescr.Close();
    }
  }
  return "";
}

void print_extended(const char *file_name, bool *abort, int numlist, int indent) {
  bool next = false;
  int numl = 0;
  int cpos = 0;
  char ch, s[81];
  int i;

  string ss = read_extended_description(file_name);
  if (!ss.empty()) {
    ch = (indent != 2) ? 10 : 0;
    while (ss[cpos] && !(*abort) && numl < numlist) {
      if (ch == SOFTRETURN) {
        if (indent == 1) {
          for (i = 0; i < INDENTION; i++) {
            if (i == 12 || i == 18) {
              s[i] = (okansi() ? '\xBA' : ' '); // was |
            } else {
              s[i] = SPACE;
            }
          }
          s[INDENTION] = '\0';
          bout.Color(FRAME_COLOR);
          bout.bputs(s, abort, &next);
          bout.Color(1);
        } else {
          if (indent == 2) {
            for (i = 0; i < 13; i++) {
              s[i] = SPACE;
            }
            s[13] = '\0';
            bout.bputs(s, abort, &next);
            bout.Color(2);
          }
        }
      }
      bout.bputch(ch = ss[ cpos++ ]);
      checka(abort, &next);
      if (ch == SOFTRETURN) {
        ++numl;
      } else if (ch != RETURN && a()->localIO()->WhereX() >= 78) {
        bout.bputs("\r\n", abort, &next);
        ch = SOFTRETURN;
      }
    }
    if (bout.wherex() > 0) {
      bout.nl();
    }
  } else if (bout.wherex() > 0) {
    bout.nl();
  }
}

std::string aligns(const std::string& file_name) {
  return wwiv::sdk::files::align(file_name);
}

void align(char *file_name) {
  const auto s = wwiv::sdk::files::align(file_name);
  strcpy(file_name, s.c_str());
}

bool compare(const char *pszFileName1, const char *pszFileName2) {
  for (int i = 0; i < 12; i++) {
    if (pszFileName1[i] != pszFileName2[i] && pszFileName1[i] != '?' && pszFileName2[i] != '?') {
      return false;
    }
  }
  return true;
}

void printinfo(uploadsrec * u, bool *abort) {
  char s[85], s1[40], s2[81];
  int i;
  bool next;

  {
    tagrec_t t;
    t.u = *u;
    auto subnum = a()->current_user_dir().subnum;
    t.directory = subnum;
    t.dir_mask = a()->directories[subnum].mask;
    a()->filelist.emplace_back(std::move(t));
    sprintf(s, "\r|#%d%2d|#%d%c",
            (check_batch_queue(u->filename)) ? 6 : 0,
            a()->filelist.size(), FRAME_COLOR, okansi() ? '\xBA' : ' '); // was |
    bout.bputs(s, abort, &next);
  }
  bout.Color(1);
  strncpy(s, u->filename, 8);
  s[8] = '\0';
  bout.bputs(s, abort, &next);
  strncpy(s, &((u->filename)[8]), 4);
  s[4] = '\0';
  bout.Color(1);
  bout.bputs(s, abort, &next);
  bout.Color(FRAME_COLOR);
  bout.bputs((okansi() ? "\xBA" : " "), abort, &next); // was |

  sprintf(s1, "%ld""k", bytes_to_k(u->numbytes));

  if (!(a()->directories[ a()->udir[ a()->current_user_dir_num() ].subnum ].mask & mask_cdrom)) {
    strcpy(s2, a()->directories[ a()->udir[ a()->current_user_dir_num() ].subnum ].path);
    strcat(s2, u->filename);
    if (!File::Exists(s2)) {
      to_char_array(s1, "N/A");
    }
  }
  for (i = 0; i < 5 - wwiv::strings::ssize(s1); i++) {
    s[i] = SPACE;
  }
  s[i] = '\0';
  strcat(s, s1);
  bout.Color(2);
  bout.bputs(s, abort, &next);

  {
    bout.Color(FRAME_COLOR);
    bout.bputs((okansi() ? "\xBA" : " "), abort, &next); // was |
    sprintf(s1, "%d", u->numdloads);

    for (i = 0; i < 4 - wwiv::strings::ssize(s1); i++) {
      s[i] = SPACE;
    }
    s[i] = '\0';
    strcat(s, s1);
    bout.Color(2);
    bout.bputs(s, abort, &next);
  }
  bout.Color(FRAME_COLOR);
  bout.bputs((okansi() ? "\xBA" : " "), abort, &next); // was |
  sprintf(s, "|#%d%s", (u->mask & mask_extended) ? 1 : 2, u->description);
  bout.bpla(trim_to_size_ignore_colors(s, a()->user()->GetScreenChars() - 28), abort);

  if (*abort) {
    a()->filelist.clear();
  }
}

void printtitle(bool *abort) {
  bout.Color(FRAME_COLOR);
  bout << "\r" << string(78, '-') << wwiv::endl;

  checka(abort);
  bout.Color(2);
  bout << "\r"
    << a()->directories[a()->current_user_dir().subnum].name
    << " - #" << a()->current_user_dir().keys << ", "
    << a()->numf << " files." << wwiv::endl;

  bout.Color(FRAME_COLOR);
  bout << "\r" << string(78, '-') << wwiv::endl;
}

std::string file_mask() {
  bout.nl();
  bout << "|#2File mask: ";
  string s = input(12);
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
  string filemask = file_mask();
  bool need_title = true;
  bout.clear_lines_listed();

  File fileDownload(a()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeReadOnly);
  bool abort = false;
  for (int i = 1; i <= a()->numf && !abort && !a()->hangup_; i++) {
    FileAreaSetRecord(fileDownload, i);
    uploadsrec u{};
    fileDownload.Read(&u, sizeof(uploadsrec));
    if (compare(filemask.c_str(), u.filename)) {
      fileDownload.Close();

      if (need_title) {
        printtitle(&abort);
        need_title = false;
      }

      printinfo(&u, &abort);

      // Moved to here from bputch.cpp
      if (bout.lines_listed() >= a()->screenlinest - 3) {
        if (!a()->filelist.empty()) {
          tag_files(need_title);
          bout.clear_lines_listed();
        }
      }

      fileDownload.Open(File::modeBinary | File::modeReadOnly);
    } else if (bkbhit()) {
      checka(&abort);
    }
  }
  fileDownload.Close();
  endlist(1);
}

void nscandir(uint16_t nDirNum, bool& need_title, bool *abort) {
  auto nOldCurDir = a()->current_user_dir_num();
  a()->set_current_user_dir_num(nDirNum);
  dliscan();
  if (this_date >= a()->context().nscandate()) {
    if (okansi()) {
      *abort = listfiles_plus(LP_NSCAN_DIR) ? 1 : 0;
      a()->set_current_user_dir_num(nOldCurDir);
      return;
    }
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    for (int i = 1; i <= a()->numf && !(*abort) && !a()->hangup_; i++) {
      CheckForHangup();
      FileAreaSetRecord(fileDownload, i);
      uploadsrec u;
      fileDownload.Read(&u, sizeof(uploadsrec));
      if (u.daten >= a()->context().nscandate()) {
        fileDownload.Close();

        if (need_title) {
          if (bout.lines_listed() >= a()->screenlinest - 7 && !a()->filelist.empty()) {
            tag_files(need_title);
          }
          if (need_title) {
            printtitle(abort);
            need_title = false;
          }
        }

        printinfo(&u, abort);
        fileDownload.Open(File::modeBinary | File::modeReadOnly);
      } else if (bkbhit()) {
        checka(abort);
      }
    }
    fileDownload.Close();
  }
  a()->set_current_user_dir_num(nOldCurDir);
}

void nscanall() {
  bool bScanAllConfs = false;
  a()->context().scanned_files(true);

  if (a()->uconfdir[1].confnum != -1 && okconf(a()->user())) {
    bout.nl();
    bout << "|#5All conferences? ";
    bScanAllConfs = yesno();
    bout.nl();
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  if (okansi()) {
    auto save_dir = a()->current_user_dir_num();
    listfiles_plus(LP_NSCAN_NSCAN);
    if (bScanAllConfs) {
      tmp_disable_conf(false);
    }
    a()->set_current_user_dir_num(save_dir);
    return;
  }
  bool abort = false;
  int count = 0;
  int color = 3;
  bout << "\r|#2Searching ";
  for (uint16_t i = 0; i < a()->directories.size() && !abort && a()->udir[i].subnum != -1; i++) {
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
    if (a()->context().qsc_n[nSubNum / 32] & (1L << (nSubNum % 32))) {
      bool need_title = true;
      nscandir(i, need_title, &abort);
    }
  }
  endlist(2);
  if (bScanAllConfs) {
    tmp_disable_conf(false);
  }
}

void searchall() {
  if (okansi()) {
    listfiles_plus(LP_SEARCH_ALL);
    return;
  }

  bool bScanAllConfs = false;
  if (a()->uconfdir[1].confnum != -1 && okconf(a()->user())) {
    bout.nl();
    bout << "|#5All conferences? ";
    bScanAllConfs = yesno();
    bout.nl();
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  bool abort = false;
  auto nOldCurDir = a()->current_user_dir_num();
  bout.nl(2);
  bout << "Search all a()->directories.\r\n";
  string filemask = file_mask();
  bout.nl();
  bout << "|#2Searching ";
  bout.clear_lines_listed();
  int count = 0;
  int color = 3;
  for (uint16_t i = 0; i < a()->directories.size() && !abort && !a()->hangup_
       && a()->udir[i].subnum != -1; i++) {
    int nDirNum = a()->udir[i].subnum;
    bool bIsDirMarked = false;
    if (a()->context().qsc_n[nDirNum / 32] & (1L << (nDirNum % 32))) {
      bIsDirMarked = true;
    }
    bIsDirMarked = true;
    // remove bIsDirMarked=true to search only marked a()->directories
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
      File fileDownload(a()->download_filename_);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      for (int i1 = 1; i1 <= a()->numf && !abort && !a()->hangup_; i1++) {
        FileAreaSetRecord(fileDownload, i1);
        uploadsrec u;
        fileDownload.Read(&u, sizeof(uploadsrec));
        if (compare(filemask.c_str(), u.filename)) {
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
  a()->set_current_user_dir_num(nOldCurDir);
  endlist(1);
  if (bScanAllConfs) {
    tmp_disable_conf(false);
  }
}

int recno(const std::string& file_mask) {
  return nrecno(file_mask, 0);
}

int nrecno(const std::string& file_mask, int nStartingRec) {
  int nRecNum = nStartingRec + 1;
  if (a()->numf < 1 || nStartingRec >= a()->numf) {
    return -1;
  }

  File fileDownload(a()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeReadOnly);
  FileAreaSetRecord(fileDownload, nRecNum);
  uploadsrec u{};
  fileDownload.Read(&u, sizeof(uploadsrec));
  while ((nRecNum < a()->numf) && (compare(file_mask.c_str(), u.filename) == 0)) {
    ++nRecNum;
    FileAreaSetRecord(fileDownload, nRecNum);
    fileDownload.Read(&u, sizeof(uploadsrec));
  }
  fileDownload.Close();
  return (compare(file_mask.c_str(), u.filename)) ? nRecNum : -1;
}

int printfileinfo(uploadsrec * u, int directory_num) {
  double d = XFER_TIME(u->numbytes);
  bout << "Filename   : " << stripfn(u->filename) << wwiv::endl;
  bout << "Description: " << u->description << wwiv::endl;
  bout << "File size  : " << bytes_to_k(u->numbytes) << wwiv::endl;
  bout << "Apprx. time: " << ctim(std::lround(d)) << wwiv::endl;
  bout << "Uploaded on: " << u->date << wwiv::endl;
  if (u->actualdate[2] == '/' && u->actualdate[5] == '/') {
    bout << "File date  : " << u->actualdate << wwiv::endl;
  }
  bout << "Uploaded by: " << u->upby << wwiv::endl;
  bout << "Times D/L'd: " << u->numdloads << wwiv::endl;
  bout.nl();
  bool abort = false;
  if (u->mask & mask_extended) {
    bout << "Extended Description: \r\n";
    print_extended(u->filename, &abort, 255, 0);
  }
  char file_name[MAX_PATH];
  sprintf(file_name, "%s%s", a()->directories[directory_num].path, u->filename);
  StringRemoveWhitespace(file_name);
  if (!File::Exists(file_name)) {
    bout << "\r\n-=>FILE NOT THERE<=-\r\n\n";
    return -1;
  }
  return (nsl() >= d) ? 1 : 0;
}

void remlist(const char *file_name) {
  const auto fn = aligns(file_name);

  if (a()->filelist.empty()) {
    return;
  }
  for (auto b = a()->filelist.begin(); b != a()->filelist.end(); b++) {
    const auto list_fn = aligns(b->u.filename);
    if (fn == list_fn) {
      a()->filelist.erase(b);
      return;
    }
  }
}

int FileAreaSetRecord(File &file, int nRecordNumber) {
  const auto r = file.Seek(nRecordNumber * sizeof(uploadsrec), File::Whence::begin);
  return static_cast<int>(r);
}
