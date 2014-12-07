/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/input.h"
#include "core/strings.h"
#include "bbs/stuffin.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/xfer_common.h"
#include "bbs/wwiv.h"
#include "bbs/platform/platformfcns.h"

using std::string;
using namespace wwiv::strings;

// How far to indent extended descriptions
static const int INDENTION = 24;

int foundany;
int this_date;

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
  File fileExtDescr(g_szExtDescrFileName);
  if (fileExtDescr.Open(File::modeReadOnly | File::modeBinary)) {
    long lFileSize = fileExtDescr.GetLength();
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

int check_batch_queue(const char *pszFileName) {
  for (int i = 0; i < session()->numbatch; i++) {
    if (IsEquals(pszFileName, batch[i].filename)) {
      return (batch[i].sending) ? 1 : -1;
    }
  }
  return 0;
}

/**
 * returns true if everything is ok, false if the file
 */
bool check_ul_event(int nDirectoryNum, uploadsrec * u) {
  if (syscfg.upload_cmd.empty()) {
    return true;
  }
  const string comport = StringPrintf("%d", incom ? syscfgovr.primaryport : 0);
  const string cmdLine = stuff_in(syscfg.upload_cmd, create_chain_file(), directories[nDirectoryNum].path,
                                  stripfn(u->filename), comport, "");
  ExecuteExternalProgram(cmdLine, application()->GetSpawnOptions(SPAWNOPT_ULCHK));

  File file(directories[nDirectoryNum].path, stripfn(u->filename));
  if (!file.Exists()) {
    sysoplogf("File \"%s\" to %s deleted by UL event.", u->filename, directories[nDirectoryNum].name);
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

void get_arc_cmd(char *pszOutBuffer, const char *pszArcFileName, int cmd, const char *ofn) {
  char szArcCmd[ MAX_PATH ];

  pszOutBuffer[0] = '\0';
  const char* ss = strrchr(pszArcFileName, '.');
  if (ss == nullptr) {
    return;
  }
  ++ss;
  for (int i = 0; i < MAX_ARCS; i++) {
    if (IsEqualsIgnoreCase(ss, arcs[i].extension)) {
      switch (cmd) {
      case 0:
        strcpy(szArcCmd, arcs[i].arcl);
        break;
      case 1:
        strcpy(szArcCmd, arcs[i].arce);
        break;
      case 2:
        strcpy(szArcCmd, arcs[i].arca);
        break;
      case 3:
        strcpy(szArcCmd, arcs[i].arcd);
        break;
      case 4:
        strcpy(szArcCmd, arcs[i].arck);
        break;
      case 5:
        strcpy(szArcCmd, arcs[i].arct);
        break;
      }

      if (szArcCmd[0] == 0) {
        return;
      }
      string command = stuff_in(szArcCmd, pszArcFileName, ofn, "", "", "");
      WWIV_make_abs_cmd(application()->GetHomeDir(), &command);
      strcpy(pszOutBuffer, command.c_str());
      return;
    }
  }
}

int list_arc_out(const char *pszFileName, const char *pszDirectory) {
  char szFileNameToDelete[81];
  int nRetCode = 0;

  szFileNameToDelete[0] = 0;

  char szFullPathName[ MAX_PATH ];
  sprintf(szFullPathName, "%s%s", pszDirectory, pszFileName);
  if (directories[udir[session()->GetCurrentFileArea()].subnum].mask & mask_cdrom) {
    sprintf(szFullPathName, "%s%s", syscfgovr.tempdir, pszFileName);
    if (!File::Exists(szFullPathName)) {
      char szFullPathNameInDir[ MAX_PATH ];
      sprintf(szFullPathNameInDir, "%s%s", pszDirectory, pszFileName);
      copyfile(szFullPathNameInDir, szFullPathName, false);
      strcpy(szFileNameToDelete, szFullPathName);
    }
  }
  char szArchiveCmd[ MAX_PATH ];
  get_arc_cmd(szArchiveCmd, szFullPathName, 0, "");
  if (!okfn(pszFileName)) {
    szArchiveCmd[0] = 0;
  }

  if (File::Exists(szFullPathName) && (szArchiveCmd[0] != 0)) {
    bout.nl(2);
    bout << "Archive listing for " << pszFileName << wwiv::endl;
    bout.nl();
    nRetCode = ExecuteExternalProgram(szArchiveCmd, application()->GetSpawnOptions(SPAWNOPT_ARCH_L));
    bout.nl();
  } else {
    bout.nl();
    session()->localIO()->LocalPuts("Unknown archive: ");
    bout << pszFileName;
    bout.nl(2);
    nRetCode = 0;
  }

  if (szFileNameToDelete[0]) {
    File::Remove(szFileNameToDelete);
  }

  return nRetCode;
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

void dliscan1(int nDirectoryNum) {
  sprintf(g_szDownloadFileName, "%s%s.dir", syscfg.datadir, directories[nDirectoryNum].filename);
  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  int nNumRecords = fileDownload.GetLength() / sizeof(uploadsrec);
  uploadsrec u;
  if (nNumRecords == 0) {
    memset(&u, 0, sizeof(uploadsrec));
    strcpy(u.filename, "|MARKER|");
    time_t tNow;
    time(&tNow);
    u.daten = static_cast<unsigned long>(tNow);
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
      u.daten = static_cast<unsigned long>(l);
      u.numbytes = session()->numf;
      FileAreaSetRecord(fileDownload, 0);
      fileDownload.Write(&u, sizeof(uploadsrec));
    }
  }
  fileDownload.Close();
  session()->numf = u.numbytes;
  this_date = u.daten;
  session()->m_DirectoryDateCache[nDirectoryNum] = this_date;

  sprintf(g_szExtDescrFileName, "%s%s.ext", syscfg.datadir, directories[nDirectoryNum].filename);
  zap_ed_info();
}

void dliscan_hash(int nDirectoryNum) {
  uploadsrec u;

  if (nDirectoryNum >= session()->num_dirs || session()->m_DirectoryDateCache[nDirectoryNum]) {
    return;
  }

  string dir = StringPrintf("%s%s.dir",
      syscfg.datadir, directories[nDirectoryNum].filename);
  File file(dir);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    time((time_t *) & (session()->m_DirectoryDateCache[nDirectoryNum]));
    return;
  }
  int nNumRecords = file.GetLength() / sizeof(uploadsrec);
  if (nNumRecords < 1) {
    time((time_t *) & (session()->m_DirectoryDateCache[nDirectoryNum]));
  } else {
    FileAreaSetRecord(file, 0);
    file.Read(&u, sizeof(uploadsrec));
    if (IsEquals(u.filename, "|MARKER|")) {
      session()->m_DirectoryDateCache[nDirectoryNum] = u.daten;
    } else {
      time((time_t *) & (session()->m_DirectoryDateCache[nDirectoryNum]));
    }
  }
  file.Close();
}

void dliscan() {
  dliscan1(udir[session()->GetCurrentFileArea()].subnum);
}

void add_extended_description(const char *pszFileName, const char *pszDescription) {
  ext_desc_type ed;

  strcpy(ed.name, pszFileName);
  ed.len = static_cast<short>(GetStringLength(pszDescription));

  File file(g_szExtDescrFileName);
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(0L, File::seekEnd);
  file.Write(&ed, sizeof(ext_desc_type));
  file.Write(pszDescription, ed.len);
  file.Close();

  zap_ed_info();
}

void delete_extended_description(const char *pszFileName) {
  ext_desc_type ed;

  char* ss = static_cast<char *>(BbsAllocA(10240L));
  if (ss == nullptr) {
    return;
  }

  File fileExtDescr(g_szExtDescrFileName);
  fileExtDescr.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  long lFileSize = fileExtDescr.GetLength();
  long r = 0, w = 0;
  while (r < lFileSize) {
    fileExtDescr.Seek(r, File::seekBegin);
    fileExtDescr.Read(&ed, sizeof(ext_desc_type));
    if (ed.len < 10000) {
      fileExtDescr.Read(ss, ed.len);
      if (!IsEquals(pszFileName, ed.name)) {
        if (r != w) {
          fileExtDescr.Seek(w, File::seekBegin);
          fileExtDescr.Write(&ed, sizeof(ext_desc_type));
          fileExtDescr.Write(ss, ed.len);
        }
        w += sizeof(ext_desc_type) + ed.len;
      }
    }
    r += sizeof(ext_desc_type) + ed.len;
  }
  fileExtDescr.SetLength(w);
  fileExtDescr.Close();
  free(ss);
  zap_ed_info();
}

char *read_extended_description(const char *pszFileName) {
  get_ed_info();

  if (ed_got && ed_info) {
    for (int i = 0; i < ed_num; i++) {
      if (IsEquals(pszFileName, ed_info[i].name)) {
        File fileExtDescr(g_szExtDescrFileName);
        if (!fileExtDescr.Open(File::modeBinary | File::modeReadOnly)) {
          return nullptr;
        }
        fileExtDescr.Seek(ed_info[i].offset, File::seekBegin);
        ext_desc_type ed;
        int nNumRead = fileExtDescr.Read(&ed, sizeof(ext_desc_type));
        if (nNumRead == sizeof(ext_desc_type) &&
            IsEquals(pszFileName, ed.name)) {
          char* ss = static_cast<char *>(BbsAllocA(ed.len + 10));
          if (ss) {
            fileExtDescr.Read(ss, ed.len);
            ss[ed.len] = 0;
          }
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
    File fileExtDescr(g_szExtDescrFileName);
    if (fileExtDescr.Open(File::modeBinary | File::modeReadOnly)) {
      long lFileSize = fileExtDescr.GetLength();
      long lCurPos = 0;
      while (lCurPos < lFileSize) {
        fileExtDescr.Seek(lCurPos, File::seekBegin);
        ext_desc_type ed;
        lCurPos += static_cast<long>(fileExtDescr.Read(&ed, sizeof(ext_desc_type)));
        if (IsEquals(pszFileName, ed.name)) {
          char* ss = static_cast<char *>(BbsAllocA(ed.len + 10));
          if (ss) {
            fileExtDescr.Read(ss, ed.len);
            ss[ed.len] = 0;
          }
          fileExtDescr.Close();
          return ss;
        } else {
          lCurPos += static_cast<long>(ed.len);
        }
      }
      fileExtDescr.Close();
    }
  }
  return nullptr;
}

void print_extended(const char *pszFileName, bool *abort, int numlist, int indent) {
  bool next = false;
  int numl = 0;
  int cpos = 0;
  char ch, s[81];
  int i;

  char* ss = read_extended_description(pszFileName);
  if (ss) {
    ch = (indent != 2) ? 10 : 0;
    while (ss[cpos] && !(*abort) && numl < numlist) {
      if (ch == SOFTRETURN) {
        if (indent == 1) {
          for (i = 0; i < INDENTION; i++) {
            if (i == 12 || i == 18) {
              s[ i ] = (okansi() ? '\xBA' : '|');
            } else {
              s[ i ] = SPACE;
            }
          }
          s[INDENTION] = '\0';
          bout.Color(session()->user()->IsUseExtraColor() && !(*abort) ? FRAME_COLOR : 0);
          osan(s, abort, &next);
          if (session()->user()->IsUseExtraColor() && !(*abort)) {
            bout.Color(1);
          }
        } else {
          if (indent == 2) {
            for (i = 0; i < 13; i++) {
              s[ i ] = SPACE;
            }
            s[13] = '\0';
            osan(s, abort, &next);
            if (session()->user()->IsUseExtraColor() && !(*abort)) {
              bout.Color(2);
            }
          }
        }
      }
      bputch(ch = ss[ cpos++ ]);
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
    free(ss);
  } else if (session()->localIO()->WhereX()) {
    bout.nl();
  }
}

void align(char *pszFileName) {
  // TODO Modify this to handle long filenames
  char szFileName[40], szExtension[40];

  bool bInvalid = false;
  if (pszFileName[ 0 ] == '.') {
    bInvalid = true;
  }

  for (int i = 0; i < GetStringLength(pszFileName); i++) {
    if (pszFileName[i] == '\\' || pszFileName[i] == '/' ||
        pszFileName[i] == ':'  || pszFileName[i] == '<' ||
        pszFileName[i] == '>'  || pszFileName[i] == '|') {
      bInvalid = true;
    }
  }
  if (bInvalid) {
    strcpy(pszFileName, "        .   ");
    return;
  }
  char* s2 = strrchr(pszFileName, '.');
  if (s2 == nullptr || strrchr(pszFileName, '\\') > s2) {
    szExtension[0] = '\0';
  } else {
    strcpy(szExtension, &(s2[ 1 ]));
    szExtension[3]  = '\0';
    s2[0]           = '\0';
  }
  strcpy(szFileName, pszFileName);

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

  char szBuffer[ MAX_PATH ];
  for (int i4 = 0; i4 < 12; i4++) {
    szBuffer[ i4 ] = SPACE;
  }
  strcpy(szBuffer, szFileName);
  szBuffer[8] = '.';
  strcpy(&(szBuffer[9]), szExtension);
  strcpy(pszFileName, szBuffer);
  for (int i5 = 0; i5 < 12; i5++) {
    pszFileName[ i5 ] = wwiv::UpperCase<char>(pszFileName[ i5 ]);
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

  if (session()->titled != 0) {
    printtitle(abort);
  }
  if (session()->tagging == 0 && !x_only) {
    return;
  }
  if (session()->tagging == 1 && !session()->user()->IsUseNoTagging() && !x_only) {
    if (!filelist) {
      filelist = static_cast<tagrec *>(BbsAllocA(50 * sizeof(tagrec)));
      session()->tagptr = 0;
    } else {
      filelist[session()->tagptr].u = *u;
      filelist[session()->tagptr].directory = udir[session()->GetCurrentFileArea()].subnum;
      filelist[session()->tagptr].dir_mask = directories[udir[session()->GetCurrentFileArea()].subnum].mask;
      session()->tagptr++;
      sprintf(s, "\r|#%d%2d|#%d%c",
              (check_batch_queue(filelist[session()->tagptr - 1].u.filename)) ? 6 : 0,
              session()->tagptr, session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0, okansi() ? '\xBA' : '|');
      osan(s, abort, &next);
    }
  } else if (!x_only) {
    bout << "\r";
  }
  bout.Color(1);
  strncpy(s, u->filename, 8);
  s[8] = '\0';
  osan(s, abort, &next);
  strncpy(s, &((u->filename)[ 8 ]), 4);
  s[4] = '\0';
  bout.Color(1);
  osan(s, abort, &next);
  bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
  osan((okansi() ? "\xBA" : "|"), abort, &next);

  sprintf(s1, "%ld""k", bytes_to_k(u->numbytes));

  if (!(directories[ udir[ session()->GetCurrentFileArea() ].subnum ].mask & mask_cdrom)) {
    strcpy(s2, directories[ udir[ session()->GetCurrentFileArea() ].subnum ].path);
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

  if (session()->tagging == 1 && !session()->user()->IsUseNoTagging() && !x_only) {
    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    osan((okansi() ? "\xBA" : "|"), abort, &next);
    sprintf(s1, "%d", u->numdloads);

    for (i = 0; i < 4 - GetStringLength(s1); i++) {
      s[i] = SPACE;
    }
    s[i] = '\0';
    strcat(s, s1);
    bout.Color(2);
    osan(s, abort, &next);
  }
  bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
  osan((okansi() ? "\xBA" : "|"), abort, &next);
  sprintf(s, "|#%d%s", (u->mask & mask_extended) ? 1 : 2, u->description);
  if (session()->tagging && !session()->user()->IsUseNoTagging() && !x_only) {
    plal(s, session()->user()->GetScreenChars() - 28, abort);
  } else {
    plal(s, strlen(s), abort);
    if (!*abort && session()->user()->GetNumExtended() && (u->mask & mask_extended)) {
      print_extended(u->filename, abort, session()->user()->GetNumExtended(), 1);
    }
  }
  if (!(*abort)) {
    ++g_num_listed;
  } else {
    session()->tagptr = 0;
    session()->tagging = 0;
  }
}

void printtitle(bool *abort) {
  char szBuffer[ 255 ];

  if (x_only) {
    bout.nl();
  }
  const char* ss = (x_only) ? "" : "\r";

  if (lines_listed >= session()->screenlinest - 7 && !x_only &&
      !session()->user()->IsUseNoTagging() && filelist && g_num_listed) {
    tag_files();
    if (session()->tagging == 0) {
      return;
    }
  }
  sprintf(szBuffer, "%s%s - #%s, %d files.", ss, directories[udir[session()->GetCurrentFileArea()].subnum].name,
          udir[session()->GetCurrentFileArea()].keys, session()->numf);
  bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
  if ((g_num_listed == 0 && session()->tagptr == 0) || session()->tagging == 0 ||
      (session()->user()->IsUseNoTagging() && g_num_listed == 0)) {
    if (okansi()) {
      bout << ss << string(78, '\xCD')  << wwiv::endl;
    } else {
      bout << ss << string(78, '-') << wwiv::endl;
    }
  } else if (lines_listed) {
    if (session()->titled != 2 && session()->tagging == 1 && !session()->user()->IsUseNoTagging()) {
      if (okansi()) {
        bout << ss <<
                           "\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                           << wwiv::endl;
      } else {
        bout << ss << "--+------------+-----+----+---------------------------------------------------" <<
                           wwiv::endl;
      }
    } else {
      if ((session()->user()->IsUseNoTagging() || session()->tagging == 2) && g_num_listed != 0) {
        bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
        if (okansi()) {
          bout << ss <<
                             "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                             << wwiv::endl;
        } else {
          bout << ss << "------------+-----+-----------------------------------------------------------" <<
                             wwiv::endl;
        }
      }
    }
  }
  if (session()->user()->IsUseExtraColor()) {
    bout.Color(2);
  }
  pla(szBuffer, abort);
  if (session()->tagging == 1 && !session()->user()->IsUseNoTagging() && !x_only) {
    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    if (okansi()) {
      bout << "\r" <<
                         "\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xcA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                         << wwiv::endl;
    } else {
      bout << "\r" << "--+------------+-----+----+---------------------------------------------------" <<
                         wwiv::endl;
    }
  } else {
    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    if (okansi()) {
      bout << "\r" <<
                         "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                         << wwiv::endl;
    } else if (x_only) {
      bout << "------------+-----+-----------------------------------------------------------" << wwiv::endl;
    } else {
      bout << "\r" << "------------+-----+-----------------------------------------------------------" <<
                         wwiv::endl;
    }
  }
  session()->titled = 0;
}

void file_mask(char *pszFileMask) {
  bout.nl();
  bout << "|#2File mask: ";
  input(pszFileMask, 12);
  if (pszFileMask[0] == '\0') {
    strcpy(pszFileMask, "*.*");
  }
  if (strchr(pszFileMask, '.') == nullptr) {
    strcat(pszFileMask, ".*");
  }
  align(pszFileMask);
  bout.nl();
}


void listfiles() {
  if (ok_listplus()) {
    listfiles_plus(LP_LIST_DIR);
    return;
  }

  dliscan();
  char szFileMask[81];
  file_mask(szFileMask);
  g_num_listed = 0;
  session()->titled = 1;
  lines_listed = 0;

  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeReadOnly);
  bool abort = false;
  for (int i = 1; i <= session()->numf && !abort && !hangup && session()->tagging != 0; i++) {
    FileAreaSetRecord(fileDownload, i);
    uploadsrec u;
    fileDownload.Read(&u, sizeof(uploadsrec));
    if (compare(szFileMask, u.filename)) {
      fileDownload.Close();
      printinfo(&u, &abort);

      // Moved to here from bputch.cpp
      if (lines_listed >= session()->screenlinest - 3) {
        if (session()->tagging && !session()->user()->IsUseNoTagging() && filelist && !chatting) {
          if (g_num_listed != 0) {
            tag_files();
          }
          lines_listed = 0;
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

void nscandir(int nDirNum, bool *abort) {
  if (session()->m_DirectoryDateCache[udir[nDirNum].subnum]
      && session()->m_DirectoryDateCache[udir[nDirNum].subnum] < static_cast<unsigned long>(nscandate)) {
    return;
  }

  int nOldCurDir = session()->GetCurrentFileArea();
  session()->SetCurrentFileArea(nDirNum);
  dliscan();
  if (this_date >= nscandate) {
    if (ok_listplus()) {
      *abort = listfiles_plus(LP_NSCAN_DIR) ? 1 : 0;
      session()->SetCurrentFileArea(nOldCurDir);
      return;
    }
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    for (int i = 1; i <= session()->numf && !(*abort) && !hangup && session()->tagging != 0; i++) {
      CheckForHangup();
      FileAreaSetRecord(fileDownload, i);
      uploadsrec u;
      fileDownload.Read(&u, sizeof(uploadsrec));
      if (u.daten >= static_cast<unsigned long>(nscandate)) {
        fileDownload.Close();
        printinfo(&u, abort);
        fileDownload.Open(File::modeBinary | File::modeReadOnly);
      } else if (bkbhit()) {
        checka(abort);
      }
    }
    fileDownload.Close();
  }
  session()->SetCurrentFileArea(nOldCurDir);
}

void nscanall() {
  bool bScanAllConfs = false;
  g_flags |= g_flag_scanned_files;

  if (uconfdir[1].confnum != -1 && okconf(session()->user())) {
    if (!x_only) {
      bout.nl();
      bout << "|#5All conferences? ";
      bScanAllConfs = yesno();
      bout.nl();
    } else {
      bScanAllConfs = true;
    }
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  if (ok_listplus()) {
    int save_dir = session()->GetCurrentFileArea();
    listfiles_plus(LP_NSCAN_NSCAN);
    if (bScanAllConfs) {
      tmp_disable_conf(false);
    }
    session()->SetCurrentFileArea(save_dir);
    return;
  }
  bool abort      = false;
  g_num_listed    = 0;
  int count       = 0;
  int color       = 3;
  if (!x_only) {
    bout << "\r" << "|#2Searching ";
  }
  for (int i = 0; i < session()->num_dirs && !abort && udir[i].subnum != -1 &&
       session()->tagging != 0; i++) {
    count++;
    if (!x_only) {
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
    }
    int nSubNum = udir[i].subnum;
    if (qsc_n[nSubNum / 32] & (1L << (nSubNum % 32))) {
      session()->titled = 1;
      nscandir(i, &abort);
    }
  }
  endlist(2);
  if (bScanAllConfs) {
    tmp_disable_conf(false);
  }
}

void searchall() {
  char szFileMask[81];

  if (ok_listplus()) {
    listfiles_plus(LP_SEARCH_ALL);
    return;
  }

  bool bScanAllConfs = false;
  if (uconfdir[1].confnum != -1 && okconf(session()->user())) {
    if (!x_only) {
      bout.nl();
      bout << "|#5All conferences? ";
      bScanAllConfs = yesno();
      bout.nl();
    } else {
      bScanAllConfs = true;
    }
    if (bScanAllConfs) {
      tmp_disable_conf(true);
    }
  }
  bool abort = false;
  int nOldCurDir = session()->GetCurrentFileArea();
  if (x_only) {
    strcpy(szFileMask, "*.*");
    align(szFileMask);
  } else {
    bout.nl(2);
    bout << "Search all directories.\r\n";
    file_mask(szFileMask);
    if (!x_only) {
      bout.nl();
      bout << "|#2Searching ";
    }
  }
  g_num_listed = 0;
  lines_listed = 0;
  int count = 0;
  int color = 3;
  for (int i = 0; i < session()->num_dirs && !abort && !hangup && (session()->tagging || x_only)
       && udir[i].subnum != -1; i++) {
    int nDirNum = udir[i].subnum;
    bool bIsDirMarked = false;
    if (qsc_n[nDirNum / 32] & (1L << (nDirNum % 32))) {
      bIsDirMarked = true;
    }
    bIsDirMarked = true;
    // remove bIsDirMarked=true to search only marked directories
    if (bIsDirMarked) {
      if (!x_only) {
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
      }
      session()->SetCurrentFileArea(i);
      dliscan();
      session()->titled = 1;
      File fileDownload(g_szDownloadFileName);
      fileDownload.Open(File::modeBinary | File::modeReadOnly);
      for (int i1 = 1; i1 <= session()->numf && !abort && !hangup && (session()->tagging || x_only); i1++) {
        FileAreaSetRecord(fileDownload, i1);
        uploadsrec u;
        fileDownload.Read(&u, sizeof(uploadsrec));
        if (compare(szFileMask, u.filename)) {
          fileDownload.Close();
          printinfo(&u, &abort);
          fileDownload.Open(File::modeBinary | File::modeReadOnly);
        } else if (bkbhit()) {
          checka(&abort);
        }
      }
      fileDownload.Close();
    }
  }
  session()->SetCurrentFileArea(nOldCurDir);
  endlist(1);
  if (bScanAllConfs) {
    tmp_disable_conf(false);
  }
}

int recno(const char *pszFileMask) {
  return nrecno(pszFileMask, 0);
}

int nrecno(const char *pszFileMask, int nStartingRec) {
  int nRecNum = nStartingRec + 1;
  if (session()->numf < 1 || nStartingRec >= session()->numf) {
    return -1;
  }

  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeReadOnly);
  FileAreaSetRecord(fileDownload, nRecNum);
  uploadsrec u;
  fileDownload.Read(&u, sizeof(uploadsrec));
  while ((nRecNum < session()->numf) && (compare(pszFileMask, u.filename) == 0)) {
    ++nRecNum;
    FileAreaSetRecord(fileDownload, nRecNum);
    fileDownload.Read(&u, sizeof(uploadsrec));
  }
  fileDownload.Close();
  return (compare(pszFileMask, u.filename)) ? nRecNum : -1;
}

int printfileinfo(uploadsrec * u, int nDirectoryNum) {
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
  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%s%s", directories[nDirectoryNum].path, u->filename);
  StringRemoveWhitespace(szFileName);
  if (!File::Exists(szFileName)) {
    bout << "\r\n-=>FILE NOT THERE<=-\r\n\n";
    return -1;
  }
  return (nsl() >= d) ? 1 : 0;
}

void remlist(const char *pszFileName) {
  char szFileName[MAX_PATH], szListFileName[MAX_PATH];

  sprintf(szFileName, "%s", pszFileName);
  align(szFileName);

  if (filelist) {
    for (int i = 0; i < session()->tagptr; i++) {
      strcpy(szListFileName, filelist[i].u.filename);
      align(szListFileName);
      if (IsEquals(szFileName, szListFileName)) {
        for (int i2 = i; i2 < session()->tagptr - 1; i2++) {
          filelist[ i2 ] = filelist[ i2 + 1 ];
        }
        session()->tagptr--;
        g_num_listed--;
      }
    }
  }
}

int FileAreaSetRecord(File &file, int nRecordNumber) {
  return file.Seek(nRecordNumber * sizeof(uploadsrec), File::seekBegin);
}
