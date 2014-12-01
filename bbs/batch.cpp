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
#include <algorithm>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include "bbs/wwiv.h"
#include "bbs/instmsg.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/wwivassert.h"

using std::string;


// module private functions
void  listbatch();
void  downloaded(char *pszFileName, long lCharsPerSecond);
void    uploaded(char *pszFileName, long lCharsPerSecond);
void    handle_dszline(char *l);
double  ratio1(long a);
void  make_ul_batch_list(char *pszListFileName);
void  run_cmd(char *pszCommandLine, const char *downlist, const char *uplist, const char *dl, bool bHangupAfterDl);
void  ProcessDSZLogFile();
void  dszbatchul(bool bHangupAfterDl, char *pszCommandLine, char *pszDescription);
void  bihangup(int up);
int   try_to_ul(char *pszFileName);
int   try_to_ul_wh(char *pszFileName);
void  normalupload(int dn);


using namespace wwiv::strings;


//////////////////////////////////////////////////////////////////////////////
// Implementation

// Shows listing of files currently in batch queue(s), both upload and
// download. Shows estimated time, by item, for files in the d/l queue.
void listbatch() {
  bout.nl();
  if (GetSession()->numbatch == 0) {
    return;
  }
  bool abort = false;
  bout << "|#9Files - |#2" << GetSession()->numbatch << "  ";
  if (GetSession()->numbatchdl) {
    bout << "|#9Time - |#2" << ctim(batchtime);
  }
  bout.nl(2);
  for (int i = 0; i < GetSession()->numbatch && !abort && !hangup; i++) {
    char szBuffer[255];
    if (batch[i].sending) {
      sprintf(szBuffer, "%d. %s %s   %s  %s", i + 1, "(D)", batch[i].filename, ctim(batch[i].time),
              directories[batch[i].dir].name);
    } else {
      sprintf(szBuffer, "%d. %s %s             %s", i + 1, "(U)", batch[i].filename, directories[batch[i].dir].name);
    }
    pla(szBuffer, &abort);
  }
  bout.nl();
}


// Deletes one item (index i) from the d/l batch queue.
void delbatch(int nBatchEntryNum) {
  if (nBatchEntryNum < GetSession()->numbatch) {
    if (batch[nBatchEntryNum].sending) {
      batchtime -= batch[nBatchEntryNum].time;
      --GetSession()->numbatchdl;
    }
    --GetSession()->numbatch;
    for (int i1 = nBatchEntryNum; i1 <= GetSession()->numbatch; i1++) {
      batch[i1] = batch[i1 + 1];
    }
  }
}

void downloaded(char *pszFileName, long lCharsPerSecond) {
  uploadsrec u;

  for (int i1 = 0; i1 < GetSession()->numbatch; i1++) {
    if (wwiv::strings::IsEquals(pszFileName, batch[i1].filename) &&
        batch[i1].sending) {
      dliscan1(batch[i1].dir);
      int nRecNum = recno(batch[i1].filename);
      if (nRecNum > 0) {
        File file(g_szDownloadFileName);
        file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
        FileAreaSetRecord(file, nRecNum);
        file.Read(&u, sizeof(uploadsrec));
        GetSession()->GetCurrentUser()->SetFilesDownloaded(GetSession()->GetCurrentUser()->GetFilesDownloaded() + 1);
        GetSession()->GetCurrentUser()->SetDownloadK(GetSession()->GetCurrentUser()->GetDownloadK() +
            static_cast<int>(bytes_to_k(u.numbytes)));
        ++u.numdloads;
        FileAreaSetRecord(file, nRecNum);
        file.Write(&u, sizeof(uploadsrec));
        file.Close();
        if (lCharsPerSecond) {
          sysoplogf("Downloaded \"%s\" (%ld cps)", u.filename, lCharsPerSecond);
        } else {
          sysoplogf("Downloaded \"%s\"", u.filename);
        }
        if (syscfg.sysconfig & sysconfig_log_dl) {
          WUser user;
          GetApplication()->GetUserManager()->ReadUser(&user, u.ownerusr);
          if (!user.IsUserDeleted()) {
            if (date_to_daten(user.GetFirstOn()) < static_cast<time_t>(u.daten)) {
              ssm(u.ownerusr, 0, "%s downloaded|#1 \"%s\" |#7on %s",
                  GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum), u.filename, fulldate());
            }
          }
        }
      }
      delbatch(i1);
      return;
    }
  }
  sysoplogf("!!! Couldn't find \"%s\" in DL batch queue.", pszFileName);
}


void didnt_upload(int nBatchIndex) {
  uploadsrec u;

  if (batch[nBatchIndex].sending) {
    return;
  }

  dliscan1(batch[nBatchIndex].dir);
  int nRecNum = recno(batch[nBatchIndex].filename);
  if (nRecNum > 0) {
    File file(g_szDownloadFileName);
    file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite, File::shareDenyNone);
    do {
      FileAreaSetRecord(file, nRecNum);
      file.Read(&u, sizeof(uploadsrec));
      if (u.numbytes != 0) {
        file.Close();
        nRecNum = nrecno(batch[ nBatchIndex ].filename, nRecNum);
        file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite, File::shareDenyNone);
      }
    } while (nRecNum != -1 && u.numbytes != 0);

    if (nRecNum != -1 && u.numbytes == 0) {
      if (u.mask & mask_extended) {
        delete_extended_description(u.filename);
      }
      for (int i1 = nRecNum; i1 < GetSession()->numf; i1++) {
        FileAreaSetRecord(file, i1 + 1);
        file.Read(&u, sizeof(uploadsrec));
        FileAreaSetRecord(file, i1);
        file.Write(&u, sizeof(uploadsrec));
      }
      --nRecNum;
      --GetSession()->numf;
      FileAreaSetRecord(file, 0);
      file.Read(&u, sizeof(uploadsrec));
      u.numbytes = GetSession()->numf;
      FileAreaSetRecord(file, 0);
      file.Write(&u, sizeof(uploadsrec));
      return;
    }
  }
  sysoplogf("!!! Couldn't find \"%s\" in transfer area.", batch[nBatchIndex].filename);
}


void uploaded(char *pszFileName, long lCharsPerSecond) {
  uploadsrec u;

  for (int i1 = 0; i1 < GetSession()->numbatch; i1++) {
    if (wwiv::strings::IsEquals(pszFileName, batch[i1].filename) &&
        !batch[i1].sending) {
      dliscan1(batch[i1].dir);
      int nRecNum = recno(batch[i1].filename);
      if (nRecNum > 0) {
        File downFile(g_szDownloadFileName);
        downFile.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        do {
          FileAreaSetRecord(downFile, nRecNum);
          downFile.Read(&u, sizeof(uploadsrec));
          if (u.numbytes != 0) {
            nRecNum = nrecno(batch[i1].filename, nRecNum);
          }
        } while (nRecNum != -1 && u.numbytes != 0);
        downFile.Close();
        if (nRecNum != -1 && u.numbytes == 0) {
          char szSourceFileName[MAX_PATH], szDestFileName[MAX_PATH];
          sprintf(szSourceFileName, "%s%s", syscfgovr.batchdir, pszFileName);
          sprintf(szDestFileName, "%s%s", directories[batch[i1].dir].path, pszFileName);
          if (!wwiv::strings::IsEquals(szSourceFileName, szDestFileName) &&
              File::Exists(szSourceFileName)) {
            bool found = false;
            if (szSourceFileName[1] != ':' && szDestFileName[1] != ':') {
              found = true;
            }
            if (szSourceFileName[1] == ':' && szDestFileName[1] == ':' && szSourceFileName[0] == szDestFileName[0]) {
              found = true;
            }
            if (found) {
              File::Rename(szSourceFileName, szDestFileName);
              File::Remove(szSourceFileName);
            } else {
              copyfile(szSourceFileName, szDestFileName, false);
              File::Remove(szSourceFileName);
            }
          }
          File file(szDestFileName);
          if (file.Open(File::modeBinary | File::modeReadOnly)) {
            if (!syscfg.upload_cmd.empty()) {
              file.Close();
              if (!check_ul_event(batch[i1].dir, &u)) {
                didnt_upload(i1);
              } else {
                file.Open(File::modeBinary | File::modeReadOnly);
              }
            }
            if (file.IsOpen()) {
              u.numbytes = file.GetLength();
              file.Close();
              get_file_idz(&u, batch[i1].dir);
              GetSession()->GetCurrentUser()->SetFilesUploaded(GetSession()->GetCurrentUser()->GetFilesUploaded() + 1);
              modify_database(u.filename, true);
              GetSession()->GetCurrentUser()->SetUploadK(GetSession()->GetCurrentUser()->GetUploadK() +
                  static_cast<int>(bytes_to_k(u.numbytes)));
              WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
              pStatus->IncrementNumUploadsToday();
              pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
              GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
              File fileDn(g_szDownloadFileName);
              fileDn.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
              FileAreaSetRecord(fileDn, nRecNum);
              fileDn.Write(&u, sizeof(uploadsrec));
              fileDn.Close();
              sysoplogf("+ \"%s\" uploaded on %s (%ld cps)", u.filename, directories[batch[i1].dir].name, lCharsPerSecond);
              bout << "Uploaded '" << u.filename <<
                                 "' to " << directories[batch[i1].dir].name << " (" <<
                                 lCharsPerSecond << " cps)" << wwiv::endl;
            }
          }
          delbatch(i1);
          return;
        }
      }
      delbatch(i1);
      if (try_to_ul(pszFileName)) {
        sysoplogf("!!! Couldn't find file \"%s\" in directory.", pszFileName);
        bout << "Deleting - couldn't find data for file " << pszFileName << wwiv::endl;
      }
      return;
    }
  }
  if (try_to_ul(pszFileName)) {
    sysoplogf("!!! Couldn't find \"%s\" in UL batch queue.", pszFileName);
    bout << "Deleting - don't know what to do with file " << pszFileName << wwiv::endl;

    File::Remove(syscfgovr.batchdir, pszFileName);
  }
}


void zmbatchdl(bool bHangupAfterDl) {
  int cur = 0;
  uploadsrec u;

  if (!incom) {
    return;
  }

  string message = StringPrintf("ZModem Download: Files - %d Time - %s", GetSession()->numbatchdl, ctim(batchtime));
  if (bHangupAfterDl) {
    message += ", HAD";
  }
  sysoplog(message);
  bout.nl();
  bout << message;
  bout.nl(2);

  bool bRatioBad = false;
  bool ok = true;
  do {
    GetSession()->localIO()->tleft(true);
    if ((syscfg.req_ratio > 0.0001) && (ratio() < syscfg.req_ratio)) {
      bRatioBad = true;
    }
    if (GetSession()->GetCurrentUser()->IsExemptRatio()) {
      bRatioBad = false;
    }
    if (!batch[cur].sending) {
      bRatioBad = false;
      ++cur;
    }
    if ((nsl() >= batch[cur].time) && !bRatioBad) {
      dliscan1(batch[cur].dir);
      int nRecordNumber = recno(batch[cur].filename);
      if (nRecordNumber <= 0) {
        delbatch(cur);
      } else {
        const string message = StringPrintf("Files left - %d, Time left - %s\r\n", GetSession()->numbatchdl, ctim(batchtime));
        GetSession()->localIO()->LocalPuts(message);
        File file(g_szDownloadFileName);
        file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        FileAreaSetRecord(file, nRecordNumber);
        file.Read(&u, sizeof(uploadsrec));
        file.Close();
        char szSendFileName[ MAX_PATH ];
        sprintf(szSendFileName, "%s%s", directories[batch[cur].dir].path, u.filename);
        if (directories[batch[cur].dir].mask & mask_cdrom) {
          char szOrigFileName[ MAX_PATH ];
          sprintf(szOrigFileName, "%s%s", directories[batch[cur].dir].path, u.filename);
          sprintf(szSendFileName, "%s%s", syscfgovr.tempdir, u.filename);
          if (!File::Exists(szSendFileName)) {
            copyfile(szOrigFileName, szSendFileName, true);
          }
        }
        write_inst(INST_LOC_DOWNLOAD, udir[GetSession()->GetCurrentFileArea()].subnum, INST_FLAGS_NONE);
        StringRemoveWhitespace(szSendFileName);
        double percent;
        zmodem_send(szSendFileName, &ok, &percent);
        if (ok) {
          downloaded(u.filename, 0);
        }
      }
    } else {
      delbatch(cur);
    }
  } while (ok && !hangup && GetSession()->numbatch > cur && !bRatioBad);

  if (ok && !hangup) {
    endbatch();
  }
  if (bRatioBad) {
    bout << "\r\nYour ratio is too low to continue the transfer.\r\n\n\n";
  }
  if (bHangupAfterDl) {
    bihangup(0);
  }
}


void ymbatchdl(bool bHangupAfterDl) {
  int cur = 0;
  uploadsrec u;

  if (!incom) {
    return;
  }
  string message = StringPrintf("Ymodem Download: Files - %d, Time - %s", GetSession()->numbatchdl, ctim(batchtime));
  if (bHangupAfterDl) {
    message += ", HAD";
  }
  sysoplog(message);
  bout.nl();
  bout << message;
  bout.nl(2);

  bool bRatioBad = false;
  bool ok = true;
  do {
    GetSession()->localIO()->tleft(true);
    if ((syscfg.req_ratio > 0.0001) && (ratio() < syscfg.req_ratio)) {
      bRatioBad = true;
    }
    if (GetSession()->GetCurrentUser()->IsExemptRatio()) {
      bRatioBad = false;
    }
    if (!batch[cur].sending) {
      bRatioBad = false;
      ++cur;
    }
    if ((nsl() >= batch[cur].time) && !bRatioBad) {
      dliscan1(batch[cur].dir);
      int nRecordNumber = recno(batch[cur].filename);
      if (nRecordNumber <= 0) {
        delbatch(cur);
      } else {
        const string message = StringPrintf("Files left - %d, Time left - %s\r\n", GetSession()->numbatchdl, ctim(batchtime));
        GetSession()->localIO()->LocalPuts(message);
        File file(g_szDownloadFileName);
        file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        FileAreaSetRecord(file, nRecordNumber);
        file.Read(&u, sizeof(uploadsrec));
        file.Close();
        char szSendFileName[ MAX_PATH ];
        sprintf(szSendFileName, "%s%s", directories[batch[cur].dir].path, u.filename);
        if (directories[batch[cur].dir].mask & mask_cdrom) {
          char szOrigFileName[ MAX_PATH ];
          sprintf(szOrigFileName, "%s%s", directories[batch[cur].dir].path, u.filename);
          sprintf(szSendFileName, "%s%s", syscfgovr.tempdir, u.filename);
          if (!File::Exists(szSendFileName)) {
            copyfile(szOrigFileName, szSendFileName, true);
          }
        }
        write_inst(INST_LOC_DOWNLOAD, udir[GetSession()->GetCurrentFileArea()].subnum, INST_FLAGS_NONE);
        double percent;
        xymodem_send(szSendFileName, &ok, &percent, true, true, true);
        if (ok) {
          downloaded(u.filename, 0);
        }
      }
    } else {
      delbatch(cur);
    }
  } while (ok && !hangup && GetSession()->numbatch > cur && !bRatioBad);

  if (ok && !hangup) {
    endbatch();
  }
  if (bRatioBad) {
    bout << "\r\nYour ratio is too low to continue the transfer.\r\n\n";
  }
  if (bHangupAfterDl) {
    bihangup(0);
  }
}


void handle_dszline(char *l) {
  long lCharsPerSecond = 0;

  // find the filename
  char* ss = strtok(l, " \t");
  for (int i = 0; i < 10 && ss; i++) {
    switch (i) {
    case 4:
      lCharsPerSecond = atol(ss);
      break;
    }
    ss = strtok(nullptr, " \t");
  }

  if (ss) {
    char szFileName[ MAX_PATH ];
    strcpy(szFileName, stripfn(ss));
    align(szFileName);

    switch (*l) {
    case 'Z':
    case 'r':
    case 'R':
    case 'B':
    case 'H':
      // received a file
      uploaded(szFileName, lCharsPerSecond);
      break;
    case 'z':
    case 's':
    case 'S':
    case 'b':
    case 'h':
    case 'Q':
      // sent a file
      downloaded(szFileName, lCharsPerSecond);
      break;
    case 'E':
    case 'e':
    case 'L':
    case 'l':
    case 'U':
      // error
      sysoplogf("Error transferring \"%s\"", ss);
      break;
    }
  }
}

double ratio1(long a) {
  if (GetSession()->GetCurrentUser()->GetDownloadK() == 0 && a == 0) {
    return 99.999;
  }
  double r = static_cast<float>(GetSession()->GetCurrentUser()->GetUploadK()) /
             static_cast<float>(GetSession()->GetCurrentUser()->GetDownloadK() + a);
  r  = std::min< double >(r, 99.998);
  return r;
}

void make_ul_batch_list(char *pszListFileName) {
  sprintf(pszListFileName, "%s%s.%3.3u", GetApplication()->GetHomeDir().c_str(), FILESUL_NOEXT,
          GetApplication()->GetInstanceNumber());

  File::SetFilePermissions(pszListFileName, File::permReadWrite);
  File::Remove(pszListFileName);

  File fileList(pszListFileName);
  if (!fileList.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite)) {
    pszListFileName[0] = '\0';
    return;
  }
  for (int i = 0; i < GetSession()->numbatch; i++) {
    if (!batch[i].sending) {
      char szBatchFileName[MAX_PATH], szCurrentDirectory[MAX_PATH];
      chdir(directories[batch[i].dir].path);
      WWIV_GetDir(szCurrentDirectory, true);
      GetApplication()->CdHome();
      sprintf(szBatchFileName, "%s%s\r\n", szCurrentDirectory, stripfn(batch[i].filename));
      fileList.Write(szBatchFileName, strlen(szBatchFileName));
    }
  }
  fileList.Close();
}

static void make_dl_batch_list(char *pszListFileName) {
  sprintf(pszListFileName, "%s%s.%3.3u", GetApplication()->GetHomeDir().c_str(), FILESDL_NOEXT,
          GetApplication()->GetInstanceNumber());

  File::SetFilePermissions(pszListFileName, File::permReadWrite);
  File::Remove(pszListFileName);

  File fileList(pszListFileName);
  if (!fileList.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite)) {
    pszListFileName[0] = '\0';
    return;
  }

  double at = 0.0;
  long addk = 0;
  for (int i = 0; i < GetSession()->numbatch; i++) {
    if (batch[i].sending) {
      char szFileNameToSend[ MAX_PATH ];
      if (directories[batch[i].dir].mask & mask_cdrom) {
        char szCurrentDirectory[ MAX_PATH ];
        chdir(syscfgovr.tempdir);
        WWIV_GetDir(szCurrentDirectory, true);
        sprintf(szFileNameToSend, "%s%s", szCurrentDirectory, stripfn(batch[i].filename));
        if (!File::Exists(szFileNameToSend)) {
          char szSourceFileName[ MAX_PATH ];
          chdir(directories[batch[i].dir].path);
          WWIV_GetDir(szSourceFileName, true);
          strcat(szSourceFileName, stripfn(batch[i].filename));
          copyfile(szSourceFileName, szFileNameToSend, true);
        }
        strcat(szFileNameToSend, "\r\n");
      } else {
        char szCurrentDirectory[ MAX_PATH ];
        chdir(directories[batch[i].dir].path);
        WWIV_GetDir(szCurrentDirectory, true);
        sprintf(szFileNameToSend, "%s%s\r\n", szCurrentDirectory, stripfn(batch[i].filename));
      }
      bool ok = true;
      GetApplication()->CdHome();
      if (nsl() < (batch[i].time + at)) {
        ok = false;
        bout << "Cannot download " << batch[i].filename << ": Not enough time" << wwiv::endl;
      }
      long thisk = bytes_to_k(batch[i].len);
      if ((syscfg.req_ratio > 0.0001) &&
          (ratio1(addk + thisk) < syscfg.req_ratio) &&
          !GetSession()->GetCurrentUser()->IsExemptRatio()) {
        ok = false;
        bout << "Cannot download " << batch[i].filename << ": Ratio too low" << wwiv::endl;
      }
      if (ok) {
        fileList.Write(szFileNameToSend, strlen(szFileNameToSend));
        at += batch[i].time;
        addk += thisk;
      }
    }
  }
  fileList.Close();
}

void run_cmd(char *pszCommandLine, const char *downlist, const char *uplist, const char *dl, bool bHangupAfterDl) {
  char sx1[21], sx2[21], sx3[21];

  int nSpeed = std::min<int>(com_speed, 57600);

  if (com_speed == 1) {
    strcpy(sx1, "57600");
  } else {
    sprintf(sx1, "%d", nSpeed);
  }

  nSpeed = std::min<int>(modem_speed, 57600);
  sprintf(sx3, "%d", nSpeed);
  sprintf(sx2, "%d", syscfgovr.primaryport);

  string commandLine = stuff_in(pszCommandLine, sx1, sx2, downlist, sx3, uplist);
  if (!commandLine.empty()) {
    WWIV_make_abs_cmd(GetApplication()->GetHomeDir(), &commandLine);
    GetSession()->localIO()->LocalCls();
    const string message = StringPrintf(
        "%s is currently online at %u bps\r\n\r\n%s\r\n%s\r\n",
        GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum),
        modem_speed, dl, commandLine.c_str());
    GetSession()->localIO()->LocalPuts(message);
    if (incom) {
      File::SetFilePermissions(g_szDSZLogFileName, File::permReadWrite);
      File::Remove(g_szDSZLogFileName);
      chdir(syscfgovr.batchdir);
      ExecuteExternalProgram(commandLine, GetApplication()->GetSpawnOptions(SPAWNOPT_PROT_BATCH));
      if (bHangupAfterDl) {
        bihangup(1);
        if (!GetSession()->remoteIO()->carrier()) {
          GetSession()->remoteIO()->dtr(true);
          Wait(0.274);
          holdphone(true);
        }
      } else {
        bout << "\r\n|#9Please wait...\r\n\n";
      }
      ProcessDSZLogFile();
      GetApplication()->UpdateTopScreen();
    }
  }
  if (downlist[0]) {
    File::SetFilePermissions(downlist, File::permReadWrite);
    File::Remove(downlist);
  }
  if (uplist[0 ]) {
    File::SetFilePermissions(uplist, File::permReadWrite);
    File::Remove(uplist);
  }
}

void ProcessDSZLogFile() {
  char **lines = static_cast<char **>(BbsAllocA(GetSession()->max_batch * sizeof(char *) * 2));
  WWIV_ASSERT(lines != nullptr);

  if (!lines) {
    return;
  }

  File fileDszLog(g_szDSZLogFileName);
  if (fileDszLog.Open(File::modeBinary | File::modeReadOnly)) {
    int nFileSize = static_cast<int>(fileDszLog.GetLength());
    char *ss = static_cast<char *>(BbsAllocA(nFileSize));
    WWIV_ASSERT(ss != nullptr);
    if (ss) {
      int nBytesRead = fileDszLog.Read(ss, nFileSize);
      if (nBytesRead > 0) {
        ss[ nBytesRead ] = 0;
        lines[0] = strtok(ss, "\r\n");
        for (int i = 1; (i < GetSession()->max_batch * 2 - 2) && (lines[i - 1]); i++) {
          lines[i] = strtok(nullptr, "\n");
        }
        lines[GetSession()->max_batch * 2 - 2] = nullptr;
        for (int i1 = 0; lines[i1]; i1++) {
          handle_dszline(lines[i1]);
        }
      }
      free(ss);
    }
    fileDszLog.Close();
  }
  free(lines);
}

void dszbatchdl(bool bHangupAfterDl, char *pszCommandLine, char *pszDescription) {
  char szListFileName[MAX_PATH], szDownloadLogEntry[255];

  sprintf(szDownloadLogEntry, "%s BATCH Download: Files - %d, Time - %s", pszDescription, GetSession()->numbatchdl,
          ctim(batchtime));
  if (bHangupAfterDl) {
    strcat(szDownloadLogEntry, ", HAD");
  }
  sysoplog(szDownloadLogEntry);
  bout.nl();
  bout << szDownloadLogEntry;
  bout.nl(2);

  write_inst(INST_LOC_DOWNLOAD, udir[GetSession()->GetCurrentFileArea()].subnum, INST_FLAGS_NONE);
  make_dl_batch_list(szListFileName);
  run_cmd(pszCommandLine, szListFileName, "", szDownloadLogEntry, bHangupAfterDl);
}

void dszbatchul(bool bHangupAfterDl, char *pszCommandLine, char *pszDescription) {
  char szListFileName[MAX_PATH], szDownloadLogEntry[255];

  sprintf(szDownloadLogEntry, "%s BATCH Upload: Files - %d", pszDescription,
          GetSession()->numbatch - GetSession()->numbatchdl);
  if (bHangupAfterDl) {
    strcat(szDownloadLogEntry, ", HAD");
  }
  sysoplog(szDownloadLogEntry);
  bout.nl();
  bout << szDownloadLogEntry;
  bout.nl(2);

  write_inst(INST_LOC_UPLOAD, udir[GetSession()->GetCurrentFileArea()].subnum, INST_FLAGS_NONE);
  make_ul_batch_list(szListFileName);

  double ti = timer();
  run_cmd(pszCommandLine, "", szListFileName, szDownloadLogEntry, bHangupAfterDl);
  ti = timer() - ti;
  if (ti < 0) {
    ti += static_cast< double >(SECONDS_PER_DAY);
  }
  GetSession()->GetCurrentUser()->SetExtraTime(GetSession()->GetCurrentUser()->GetExtraTime() + static_cast< float >(ti));
}

int batchdl(int mode) {
  bool bHangupAfterDl = false;
  bool done = false;
  int  otag = GetSession()->tagging;
  GetSession()->tagging = 0;
  do {
    char ch = 0;
    switch (mode) {
    case 0:
    case 3:
      bout.nl();
      if (mode == 3) {
        bout <<
                           "|#7[|#2L|#7]|#1ist Files, |#7[|#2C|#7]|#1lear Queue, |#7[|#2R|#7]|#1emove File, |#7[|#2Q|#7]|#1uit or |#7[|#2D|#7]|#1ownload : |#0";
        ch = onek("QLRDC\r");
      } else {
        bout << "|#9Batch: L,R,Q,C,D,U,? : ";
        ch = onek("Q?CLRDU");
      }
      break;
    case 1:
      listbatch();
      ch = 'D';
      break;
    case 2:
      listbatch();
      ch = 'U';
      break;
    }
    switch (ch) {
    case '?':
      printfile(TBATCH_NOEXT);
      break;
    case 'Q':
      if (mode == 3) {
        return 1;
      }
      done = true;
      break;
    case 'L':
      listbatch();
      break;
    case 'R': {
      bout.nl();
      bout << "|#9Remove which? ";
      string s;
      input(&s, 4);
      int i = atoi(s.c_str());
      if ((i > 0) && (i <= GetSession()->numbatch)) {
        didnt_upload(i - 1);
        delbatch(i - 1);
      }
      if (GetSession()->numbatch == 0) {
        bout << "\r\nBatch queue empty.\r\n\n";
        done = true;
      }
    } break;
    case 'C':
      bout << "|#5Clear queue? ";
      if (yesno()) {
        for (int i = 0; i < GetSession()->numbatch; i++) {
          didnt_upload(i);
        }
        GetSession()->numbatch = 0;
        GetSession()->numbatchdl = 0;
        batchtime = 0.0;
        done = true;
        bout << "Queue cleared.\r\n";
        if (mode == 3) {
          return 1;
        }
      }
      done = true;
      break;
    case 'U': {
      if (mode != 3) {
        bout.nl();
        bout << "|#5Hang up after transfer? ";
        bHangupAfterDl = yesno();
        bout.nl(2);
        int i = get_protocol(xf_up_batch);
        if (i > 0) {
          dszbatchul(bHangupAfterDl, externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].receivebatchfn,
                     externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].description);
          if (!bHangupAfterDl) {
            bout.bprintf("Your ratio is now: %-6.3f\r\n", ratio());
          }
        }
        done = true;
      }
    } break;
    case 'D':
    case 13:
      if (mode != 3) {
        if (GetSession()->numbatchdl == 0) {
          bout << "\r\nNothing in batch download queue.\r\n\n";
          done = true;
          break;
        }
        bout.nl();
        if (!ratio_ok()) {
          bout << "\r\nSorry, your ratio is too low.\r\n\n";
          done = true;
          break;
        }
        bout << "|#5Hang up after transfer? ";
        bHangupAfterDl = yesno();
        bout.nl();
        int i = get_protocol(xf_down_batch);
        if (i > 0) {
          if (i == WWIV_INTERNAL_PROT_YMODEM) {
            if (over_intern && (over_intern[2].othr & othr_override_internal) &&
                (over_intern[2].sendbatchfn[0])) {
              dszbatchdl(bHangupAfterDl, over_intern[2].sendbatchfn, prot_name(4));
            } else {
              ymbatchdl(bHangupAfterDl);
            }
          } else if (i == WWIV_INTERNAL_PROT_ZMODEM) {
            zmbatchdl(bHangupAfterDl);
          } else {
            dszbatchdl(bHangupAfterDl, externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn,
                       externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].description);
          }
          if (!bHangupAfterDl) {
            bout.nl();
            bout.bprintf("Your ratio is now: %-6.3f\r\n", ratio());
          }
        }
      }
      done = true;
      break;
    }
  } while (!done && !hangup);
  GetSession()->tagging = otag;
  return 0;
}

void bihangup(int up)
// This function returns one character from either the local keyboard or
// remote com port (if applicable).  Every second of inactivity, a
// beep is sounded.  After 10 seconds of inactivity, the user is hung up.
{
  int color = 5;

  dump();
  // Maybe we could use a local instead of timelastchar1
  timelastchar1 = timer1();
  long nextbeep = 18L;
  bout << "\r\n|#2Automatic disconnect in progress.\r\n";
  bout << "|#2Press 'H' to hangup, or any other key to return to system.\r\n";
  bout << "|#" << color << static_cast<int>(182L / nextbeep) << "  " << static_cast<char>(7);

  unsigned char ch = 0;
  do {
    while (!bkbhit() && !hangup) {
      long dd = timer1();
      if (labs(dd - timelastchar1) > 65536L) {
        nextbeep -= 1572480L;
        timelastchar1 -= 1572480L;
      }
      if ((dd - timelastchar1) > nextbeep) {
        bout << "\r|#" << color << (static_cast<int>(182L - nextbeep) / 18L) <<
                           "  " << static_cast<char>(7);
        nextbeep += 18L;
        if ((182L - nextbeep) / 18L <= 6) {
          color = 2;
        }
        if ((182L - nextbeep) / 18L <= 3) {
          color = 6;
        }
      }
      if (labs(dd - timelastchar1) > 182L) {
        bout.nl();
        bout << "Thank you for calling.";
        bout.nl();
        GetSession()->remoteIO()->dtr(false);
        hangup = true;
        if (up) {
          Wait(0.1);
          if (!GetSession()->remoteIO()->carrier()) {
            GetSession()->remoteIO()->dtr(true);
            Wait(0.1);
            holdphone(true);
          }
        }
      }
      giveup_timeslice();
      CheckForHangup();
    }
    ch = bgetch();
    if (ch == 'h' || ch == 'H') {
      hangup = true;
    }
  } while (!ch && !hangup);
}


void upload(int dn) {
  dliscan1(dn);
  directoryrec d = directories[ dn ];
  long lFreeSpace = freek1(d.path);
  if (lFreeSpace < 100) {
    bout << "\r\nNot enough disk space to upload here.\r\n\n";
    return;
  }
  listbatch();
  bout << "Upload - " << lFreeSpace << "k free.";
  bout.nl();
  printfile(TRY2UL_NOEXT);

  bool done = false;
  do {
    bout << "|#2B|#7) |#1Blind batch upload\r\n";
    bout << "|#2N|#7) |#1Normal upload\r\n";
    bout << "|#2Q|#7) |#1Quit\r\n\n";
    bout << "|#2Which |#7(|#2B,n,q,?|#7)|#1: ";

    char key = onek("QB\rN?");
    switch (key) {
    case 'B':
    case '\r':
      batchdl(2);
      done = true;
      break;
    case 'Q':
      done = true;
      break;
    case 'N':
      normalupload(dn);
      done = true;
      break;
    case '?':
      bout << "This is help?";
      bout.nl();
      pausescr();
      done = false;
      break;
    }
  } while (!done && !hangup);
}


char *unalign(char *pszFileName) {
  char* pszTemp = strstr(pszFileName, " ");
  if (pszTemp) {
    *pszTemp++ = '\0';
    char* pszTemp2 = strstr(pszTemp, ".");
    if (pszTemp2) {
      strcat(pszFileName, pszTemp2);
    }
  }
  return pszFileName;
}

