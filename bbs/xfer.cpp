/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include <vector>

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/execexternal.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/input.h"
#include "bbs/listplus.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/xfer_common.h"
#include "bbs/platform/platformfcns.h"
#include "core/stl.h"
#include "core/strings.h"

using std::string;
using namespace wwiv::stl;
using namespace wwiv::strings;

// How far to indent extended descriptions
static const int INDENTION = 24;

int foundany;
uint32_t this_date;

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

  if (!session()->numf) {
    return;
  }

  long lCurFilePos = 0;
  File fileExtDescr(session()->extended_description_filename_);
  if (fileExtDescr.Open(File::modeReadOnly | File::modeBinary)) {
    auto lFileSize = fileExtDescr.GetLength();
    if (lFileSize > 0) {
      ed_info = static_cast<ext_desc_rec *>(BbsAllocA(static_cast<long>(session()->numf) * sizeof(ext_desc_rec)));
      if (ed_info == nullptr) {
        fileExtDescr.Close();
        return;
      }
      ed_num = 0;
      while (lCurFilePos < lFileSize && ed_num < session()->numf) {
        fileExtDescr.Seek(lCurFilePos, File::seekBegin);
        ext_desc_type ed;
        int nNumRead = fileExtDescr.Read(&ed, sizeof(ext_desc_type));
        if (nNumRead == sizeof(ext_desc_type)) {
          strcpy(ed_info[ed_num].name, ed.name);
          ed_info[ed_num].offset = lCurFilePos;
          lCurFilePos += static_cast<long>(ed.len) + sizeof(ext_desc_type);
          ed_num++;
        }
      }
      if (lCurFilePos < lFileSize) {
        ed_got = 2;
      }
    }
    fileExtDescr.Close();
  }
}

unsigned long bytes_to_k(unsigned long lBytes) {
  return (lBytes) ? ((unsigned long)((lBytes + 1023) / 1024)) : 0L;
}

int check_batch_queue(const char *file_name) {
  for (const auto& b : session()->batch().entry) {
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
  if (syscfg.upload_cmd.empty()) {
    return true;
  }
  const string comport = StringPrintf("%d", incom ? session()->primary_port() : 0);
  const string cmdLine = stuff_in(syscfg.upload_cmd, create_chain_file(),
                                  session()->directories[directory_num].path,
                                  stripfn(u->filename), comport, "");
  ExecuteExternalProgram(cmdLine, session()->GetSpawnOptions(SPAWNOPT_ULCHK));

  File file(session()->directories[directory_num].path, stripfn(u->filename));
  if (!file.Exists()) {
    sysoplog() << "File \"" << u->filename << "\" to " << session()->directories[directory_num].name << " deleted by UL event.";
    bout << u->filename << " was deleted by the upload event.\r\n";
    return false;
  }
  return true;
}

static const char *DeviceNames[] = {
  "KBD$", "PRN", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
  "COM8", "LPT1", "LPT2", "LPT3", "CLOCK$", "SCREEN$", "POINTER$", "MOUSE$"
};

const int NUM_DEVICES = 17; // length of DeviceNames

static void finddevs(vector<string>* devices) {
  devices->clear();
  for (int i = 0; i < NUM_DEVICES; i++) {
    devices->push_back(DeviceNames[i]);
  }
}

bool okfn(const string& filename) {
  if (filename.empty()) {
    return false;
  }

  string::size_type len = filename.length();
  if (len == 0) {
    return false;
  }
  if (filename[0] == '-' || filename[0] == ' ' || filename[0] == '.' || filename[0] == '@') {
    return false;
  }
  for (char c : filename) {
    unsigned char ch = c;
    if (ch == ' '  || ch == '/' || ch == '\\' || ch == ':'  ||
        ch == '>'  || ch == '<' || ch == '|'  || ch == '+'  ||
        ch == ','  || ch == ';' || ch == '^'  || ch == '\"' ||
        ch == '\'' || ch == '`' || ch > 126) {
      return false;
    }
  }

  std::vector<string> devices;
  finddevs(&devices);

  for (const auto& device : devices) {
    string::size_type deviceLen = device.length();
    if (filename.length() >= deviceLen && filename.substr(0, deviceLen) == device) {
      if (filename[deviceLen] == '\0' || filename[deviceLen] == '.' || deviceLen == 8) {
        return false;
      }
    }
  }
  return true;
}

void print_devices() {
  std::vector<string> devices;
  finddevs(&devices);

  for (const auto& device : devices) {
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
    if (IsEqualsIgnoreCase(ss, session()->arcs[i].extension)) {
      switch (cmd) {
      case 0:
        strcpy(szArcCmd, session()->arcs[i].arcl);
        break;
      case 1:
        strcpy(szArcCmd, session()->arcs[i].arce);
        break;
      case 2:
        strcpy(szArcCmd, session()->arcs[i].arca);
        break;
      case 3:
        strcpy(szArcCmd, session()->arcs[i].arcd);
        break;
      case 4:
        strcpy(szArcCmd, session()->arcs[i].arck);
        break;
      case 5:
        strcpy(szArcCmd, session()->arcs[i].arct);
        break;
      default:
        // Unknown type.
        return;
      }

      if (szArcCmd[0] == 0) {
        return;
      }
      string command = stuff_in(szArcCmd, pszArcFileName, ofn, "", "", "");
      WWIV_make_abs_cmd(session()->GetHomeDir(), &command);
      strcpy(out_buffer, command.c_str());
      return;
    }
  }
}

int list_arc_out(const char *file_name, const char *pszDirectory) {
  string name_to_delete;

  string full_pathname = StrCat(pszDirectory, file_name);
  if (session()->directories[session()->current_user_dir().subnum].mask & mask_cdrom) {
    full_pathname = StrCat(session()->temp_directory(), file_name);
    if (!File::Exists(full_pathname)) {
      string name_in_dir = StrCat(pszDirectory, file_name);
      copyfile(name_in_dir, full_pathname, false);
      name_to_delete = full_pathname;
    }
  }
  char szArchiveCmd[MAX_PATH];
  get_arc_cmd(szArchiveCmd, full_pathname.c_str(), 0, "");
  if (!okfn(file_name)) {
    szArchiveCmd[0] = 0;
  }

  int return_code = 0;
  if (File::Exists(full_pathname) && (szArchiveCmd[0] != 0)) {
    bout.nl(2);
    bout << "Archive listing for " << file_name;
    bout.nl(2);
    return_code = ExecuteExternalProgram(szArchiveCmd, session()->GetSpawnOptions(SPAWNOPT_ARCH_L));
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

  if (!session()->user()->IsExemptRatio()) {
    if ((syscfg.req_ratio > 0.0001) && (ratio() < syscfg.req_ratio)) {
      bRetValue = false;
      bout.cls();
      bout.nl();
      bout.bprintf("Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n\n",
                                        ratio(), syscfg.req_ratio);
    }
  }
  if (!session()->user()->IsExemptPost()) {
    if ((syscfg.post_call_ratio > 0.0001) && (post_ratio() < syscfg.post_call_ratio)) {
      bRetValue = false;
      bout.cls();
      bout.nl();
      bout.bprintf("%s %-5.3f.  %s %-5.3f %s.\r\n\n",
                                        "Your post/call ratio is", post_ratio(),
                                        "You need a ratio of", syscfg.post_call_ratio,
                                        "to download.");
    }
  }
  return bRetValue;
}

bool dcs() {
  return (cs() || session()->user()->GetDsl() >= 100) ? true : false;
}

void dliscan1(int directory_num) {
  session()->download_filename_ = StrCat(syscfg.datadir, session()->directories[directory_num].filename, ".dir");
  File fileDownload(session()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  int nNumRecords = fileDownload.GetLength() / sizeof(uploadsrec);
  uploadsrec u;
  if (nNumRecords == 0) {
    memset(&u, 0, sizeof(uploadsrec));
    strcpy(u.filename, "|MARKER|");
    time_t tNow;
    time(&tNow);
    u.daten = static_cast<uint32_t>(tNow);
    FileAreaSetRecord(fileDownload, 0);
    fileDownload.Write(&u, sizeof(uploadsrec));
  } else {
    FileAreaSetRecord(fileDownload, 0);
    fileDownload.Read(&u, sizeof(uploadsrec));
    if (!IsEquals(u.filename, "|MARKER|")) {
      session()->numf = u.numbytes;
      memset(&u, 0, sizeof(uploadsrec));
      strcpy(u.filename, "|MARKER|");
      time_t l;
      time(&l);
      u.daten = static_cast<uint32_t>(l);
      u.numbytes = session()->numf;
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Write(&u, sizeof(uploadsrec));
    }
  }
  fileDownload.Close();
  session()->numf = u.numbytes;
  this_date = u.daten;

  session()->extended_description_filename_ = StrCat(syscfg.datadir, session()->directories[directory_num].filename, ".ext");
  zap_ed_info();
}

void dliscan() {
  dliscan1(session()->current_user_dir().subnum);
}

void add_extended_description(const string& file_name, const string& description) {
  ext_desc_type ed;

  strcpy(ed.name, file_name.c_str());
  ed.len = static_cast<int16_t>(description.size());

  File file(session()->extended_description_filename_);
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(0L, File::seekEnd);
  file.Write(&ed, sizeof(ext_desc_type));
  file.Write(description.c_str(), ed.len);
  file.Close();

  zap_ed_info();
}

void delete_extended_description(const string& file_name) {
  ext_desc_type ed;

  File fileExtDescr(session()->extended_description_filename_);
  fileExtDescr.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  auto lFileSize = fileExtDescr.GetLength();
  long r = 0, w = 0;
  while (r < lFileSize) {
    fileExtDescr.Seek(r, File::seekBegin);
    fileExtDescr.Read(&ed, sizeof(ext_desc_type));
    if (ed.len < 10000) {
      auto ss = std::make_unique<char[]>(ed.len);
      fileExtDescr.Read(ss.get(), ed.len);
      if (file_name != ed.name) {
        if (r != w) {
          fileExtDescr.Seek(w, File::seekBegin);
          fileExtDescr.Write(&ed, sizeof(ext_desc_type));
          fileExtDescr.Write(ss.get(), ed.len);
        }
        w += sizeof(ext_desc_type) + ed.len;
      }
    }
    r += sizeof(ext_desc_type) + ed.len;
  }
  fileExtDescr.SetLength(w);
  fileExtDescr.Close();
  zap_ed_info();
}

string read_extended_description(const string& file_name) {
  get_ed_info();

  if (ed_got && ed_info) {
    for (int i = 0; i < ed_num; i++) {
      if (file_name == ed_info[i].name) {
        File fileExtDescr(session()->extended_description_filename_);
        if (!fileExtDescr.Open(File::modeBinary | File::modeReadOnly)) {
          return nullptr;
        }
        fileExtDescr.Seek(ed_info[i].offset, File::seekBegin);
        ext_desc_type ed;
        int nNumRead = fileExtDescr.Read(&ed, sizeof(ext_desc_type));
        if (nNumRead == sizeof(ext_desc_type) && file_name == ed.name) {
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
    File fileExtDescr(session()->extended_description_filename_);
    if (fileExtDescr.Open(File::modeBinary | File::modeReadOnly)) {
      auto lFileSize = fileExtDescr.GetLength();
      long lCurPos = 0;
      while (lCurPos < lFileSize) {
        fileExtDescr.Seek(lCurPos, File::seekBegin);
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
          osan(s, abort, &next);
          bout.Color(1);
        } else {
          if (indent == 2) {
            for (i = 0; i < 13; i++) {
              s[i] = SPACE;
            }
            s[13] = '\0';
            osan(s, abort, &next);
            bout.Color(2);
          }
        }
      }
      bout.bputch(ch = ss[ cpos++ ]);
      checka(abort, &next);
      if (ch == SOFTRETURN) {
        ++numl;
      } else if (ch != RETURN && session()->localIO()->WhereX() >= 78) {
        osan("\r\n", abort, &next);
        ch = SOFTRETURN;
      }
    }
    if (session()->localIO()->WhereX()) {
      bout.nl();
    }
  } else if (session()->localIO()->WhereX()) {
    bout.nl();
  }
}

void align(std::string *file_name) {
  char s[MAX_PATH];
  strcpy(s, file_name->c_str());
  align(s);
  file_name->assign(s);
}

void align(char *file_name) {
  // TODO Modify this to handle long filenames
  char szFileName[40], szExtension[40];

  bool bInvalid = false;
  if (file_name[ 0 ] == '.') {
    bInvalid = true;
  }

  for (int i = 0; i < GetStringLength(file_name); i++) {
    if (file_name[i] == '\\' || file_name[i] == '/' ||
        file_name[i] == ':'  || file_name[i] == '<' ||
        file_name[i] == '>'  || file_name[i] == '|') {
      bInvalid = true;
    }
  }
  if (bInvalid) {
    strcpy(file_name, "        .   ");
    return;
  }
  char* s2 = strrchr(file_name, '.');
  if (s2 == nullptr || strrchr(file_name, '\\') > s2) {
    szExtension[0] = '\0';
  } else {
    strcpy(szExtension, &(s2[1]));
    szExtension[3]  = '\0';
    s2[0]           = '\0';
  }
  strcpy(szFileName, file_name);

  for (int j = strlen(szFileName); j < 8; j++) {
    szFileName[j] = SPACE;
  }
  szFileName[8] = '\0';
  bool bHasWildcard = false;
  bool bHasSpace    = false;
  for (int k = 0; k < 8; k++) {
    if (szFileName[k] == '*') {
      bHasWildcard = true;
    }
    if (szFileName[k] == ' ') {
      bHasSpace = true;
    }
    if (bHasSpace) {
      szFileName[k] = ' ';
    }
    if (bHasWildcard) {
      szFileName[k] = '?';
    }
  }

  for (int i2 = strlen(szExtension); i2 < 3; i2++) {
    szExtension[ i2 ] = SPACE;
  }
  szExtension[ 3 ] = '\0';
  bHasWildcard = false;
  for (int i3 = 0; i3 < 3; i3++) {
    if (szExtension[i3] == '*') {
      bHasWildcard = true;
    }
    if (bHasWildcard) {
      szExtension[i3] = '?';
    }
  }

  char buffer[MAX_PATH];
  for (int i4 = 0; i4 < 12; i4++) {
    buffer[ i4 ] = SPACE;
  }
  strcpy(buffer, szFileName);
  buffer[8] = '.';
  strcpy(&(buffer[9]), szExtension);
  strcpy(file_name, buffer);
  for (int i5 = 0; i5 < 12; i5++) {
    file_name[ i5 ] = wwiv::UpperCase<char>(file_name[ i5 ]);
  }
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
    auto subnum = session()->current_user_dir().subnum;
    t.directory = subnum;
    t.dir_mask = session()->directories[subnum].mask;
    session()->filelist.emplace_back(std::move(t));
    sprintf(s, "\r|#%d%2d|#%d%c",
            (check_batch_queue(u->filename)) ? 6 : 0,
            session()->filelist.size(), FRAME_COLOR, okansi() ? '\xBA' : ' '); // was |
    osan(s, abort, &next);
  }
  bout.Color(1);
  strncpy(s, u->filename, 8);
  s[8] = '\0';
  osan(s, abort, &next);
  strncpy(s, &((u->filename)[8]), 4);
  s[4] = '\0';
  bout.Color(1);
  osan(s, abort, &next);
  bout.Color(FRAME_COLOR);
  osan((okansi() ? "\xBA" : " "), abort, &next); // was |

  sprintf(s1, "%ld""k", bytes_to_k(u->numbytes));

  if (!(session()->directories[ session()->udir[ session()->current_user_dir_num() ].subnum ].mask & mask_cdrom)) {
    strcpy(s2, session()->directories[ session()->udir[ session()->current_user_dir_num() ].subnum ].path);
    strcat(s2, u->filename);
    if (!File::Exists(s2)) {
      strcpy(s1, "N/A");
    }
  }
  for (i = 0; i < 5 - GetStringLength(s1); i++) {
    s[i] = SPACE;
  }
  s[i] = '\0';
  strcat(s, s1);
  bout.Color(2);
  osan(s, abort, &next);

  {
    bout.Color(FRAME_COLOR);
    osan((okansi() ? "\xBA" : " "), abort, &next); // was |
    sprintf(s1, "%d", u->numdloads);

    for (i = 0; i < 4 - GetStringLength(s1); i++) {
      s[i] = SPACE;
    }
    s[i] = '\0';
    strcat(s, s1);
    bout.Color(2);
    osan(s, abort, &next);
  }
  bout.Color(FRAME_COLOR);
  osan((okansi() ? "\xBA" : " "), abort, &next); // was |
  sprintf(s, "|#%d%s", (u->mask & mask_extended) ? 1 : 2, u->description);
  plal(s, session()->user()->GetScreenChars() - 28, abort);

  if (*abort) {
    session()->filelist.clear();
  }
}

void printtitle(bool *abort) {
  bout.Color(FRAME_COLOR);
  bout << "\r" << string(78, '-') << wwiv::endl;

  checka(abort);
  bout.Color(2);
  bout << "\r"
    << session()->directories[session()->current_user_dir().subnum].name
    << " - #" << session()->current_user_dir().keys << ", "
    << session()->numf << " files." << wwiv::endl;

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
  align(&s);
  bout.nl();
  return s;
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

  File fileDownload(session()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeReadOnly);
  bool abort = false;
  for (int i = 1; i <= session()->numf && !abort && !hangup; i++) {
    FileAreaSetRecord(fileDownload, i);
    uploadsrec u;
    fileDownload.Read(&u, sizeof(uploadsrec));
    if (compare(filemask.c_str(), u.filename)) {
      fileDownload.Close();

      if (need_title) {
        printtitle(&abort);
        need_title = false;
      }

      printinfo(&u, &abort);

      // Moved to here from bputch.cpp
      if (bout.lines_listed() >= session()->screenlinest - 3) {
        if (!session()->filelist.empty()) {
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

void nscandir(int nDirNum, bool& need_title, bool *abort) {
  int nOldCurDir = session()->current_user_dir_num();
  session()->set_current_user_dir_num(nDirNum);
  dliscan();
  if (this_date >= nscandate) {
    if (okansi()) {
      *abort = listfiles_plus(LP_NSCAN_DIR) ? 1 : 0;
      session()->set_current_user_dir_num(nOldCurDir);
      return;
    }
    File fileDownload(session()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    for (int i = 1; i <= session()->numf && !(*abort) && !hangup; i++) {
      CheckForHangup();
      FileAreaSetRecord(fileDownload, i);
      uploadsrec u;
      fileDownload.Read(&u, sizeof(uploadsrec));
      if (u.daten >= static_cast<uint32_t>(nscandate)) {
        fileDownload.Close();

        if (need_title) {
          if (bout.lines_listed() >= session()->screenlinest - 7 && !session()->filelist.empty()) {
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
  session()->set_current_user_dir_num(nOldCurDir);
}

void nscanall() {
  bool bScanAllConfs = false;
  g_flags |= g_flag_scanned_files;

  if (uconfdir[1].confnum != -1 && okconf(session()->user())) {
    bout.nl();
    bout << "|#5All conferences? ";
    bScanAllConfs = yesno();
    bout.nl();
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  if (okansi()) {
    int save_dir = session()->current_user_dir_num();
    listfiles_plus(LP_NSCAN_NSCAN);
    if (bScanAllConfs) {
      tmp_disable_conf(false);
    }
    session()->set_current_user_dir_num(save_dir);
    return;
  }
  bool abort      = false;
  int count       = 0;
  int color       = 3;
  bout << "\r" << "|#2Searching ";
  for (size_t i = 0; i < session()->directories.size() && !abort && session()->udir[i].subnum != -1; i++) {
    count++;
    bout << "|#" << color << ".";
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
    int nSubNum = session()->udir[i].subnum;
    if (qsc_n[nSubNum / 32] & (1L << (nSubNum % 32))) {
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
  if (uconfdir[1].confnum != -1 && okconf(session()->user())) {
    bout.nl();
    bout << "|#5All conferences? ";
    bScanAllConfs = yesno();
    bout.nl();
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  bool abort = false;
  int nOldCurDir = session()->current_user_dir_num();
  bout.nl(2);
  bout << "Search all session()->directories.\r\n";
  string filemask = file_mask();
  bout.nl();
  bout << "|#2Searching ";
  bout.clear_lines_listed();
  int count = 0;
  int color = 3;
  for (size_t i = 0; i < session()->directories.size() && !abort && !hangup
       && session()->udir[i].subnum != -1; i++) {
    int nDirNum = session()->udir[i].subnum;
    bool bIsDirMarked = false;
    if (qsc_n[nDirNum / 32] & (1L << (nDirNum % 32))) {
      bIsDirMarked = true;
    }
    bIsDirMarked = true;
    // remove bIsDirMarked=true to search only marked session()->directories
    if (bIsDirMarked) {
      count++;
      bout << "|#" << color << ".";
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
      session()->set_current_user_dir_num(i);
      dliscan();
      bool need_title = true;
      File fileDownload(session()->download_filename_);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      for (int i1 = 1; i1 <= session()->numf && !abort && !hangup; i1++) {
        FileAreaSetRecord(fileDownload, i1);
        uploadsrec u;
        fileDownload.Read(&u, sizeof(uploadsrec));
        if (compare(filemask.c_str(), u.filename)) {
          fileDownload.Close();
          if (need_title) {
            if (bout.lines_listed() >= session()->screenlinest - 7 && !session()->filelist.empty()) {
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
  session()->set_current_user_dir_num(nOldCurDir);
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
  if (session()->numf < 1 || nStartingRec >= session()->numf) {
    return -1;
  }

  File fileDownload(session()->download_filename_);
  fileDownload.Open(File::modeBinary | File::modeReadOnly);
  FileAreaSetRecord(fileDownload, nRecNum);
  uploadsrec u;
  fileDownload.Read(&u, sizeof(uploadsrec));
  while ((nRecNum < session()->numf) && (compare(file_mask.c_str(), u.filename) == 0)) {
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
  bout << "Apprx. time: " << ctim(d) << wwiv::endl;
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
  sprintf(file_name, "%s%s", session()->directories[directory_num].path, u->filename);
  StringRemoveWhitespace(file_name);
  if (!File::Exists(file_name)) {
    bout << "\r\n-=>FILE NOT THERE<=-\r\n\n";
    return -1;
  }
  return (nsl() >= d) ? 1 : 0;
}

void remlist(const char *file_name) {
  char szFileName[MAX_PATH], szListFileName[MAX_PATH];

  sprintf(szFileName, "%s", file_name);
  align(szFileName);

  if (!session()->filelist.empty()) {
    for (auto b = session()->filelist.begin(); b != session()->filelist.end(); b++) {
      strcpy(szListFileName, b->u.filename);
      align(szListFileName);
      if (IsEquals(szFileName, szListFileName)) {
        b = session()->filelist.erase(b);
        break;
      }
    }
  }
}

int FileAreaSetRecord(File &file, int nRecordNumber) {
  return file.Seek(nRecordNumber * sizeof(uploadsrec), File::seekBegin);
}
