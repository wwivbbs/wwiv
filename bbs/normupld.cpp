/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "bbs/wwiv.h"
#include "core/strings.h"

using std::string;
using namespace wwiv::strings;

//////////////////////////////////////////////////////////////////////////////
// Implementation

void normalupload(int dn) {
  uploadsrec u, u1;
  memset(&u, 0, sizeof(uploadsrec));
  memset(&u1, 0, sizeof(uploadsrec));

  int ok = 1;

  dliscan1(dn);
  directoryrec d = directories[dn];
  if (session()->numf >= d.maxfiles) {
    bout.nl(3);
    bout << "This directory is currently full.\r\n\n";
    return;
  }
  if ((d.mask & mask_no_uploads) && (!dcs())) {
    bout.nl(2);
    bout << "Uploads are not allowed to this directory.\r\n\n";
    return;
  }
  bout << "|#9Filename: ";
  char szInputFileName[ MAX_PATH ];
  input(szInputFileName, 12);
  if (!okfn(szInputFileName)) {
    szInputFileName[0] = '\0';
  } else {
    if (!is_uploadable(szInputFileName)) {
      if (so()) {
        bout.nl();
        bout << "|#5In filename database - add anyway? ";
        if (!yesno()) {
          szInputFileName[0] = '\0';
        }
      } else {
        szInputFileName[0] = '\0';
        bout.nl();
        bout << "|#6File either already here or unwanted.\r\n";
      }
    }
  }
  align(szInputFileName);
  if (strchr(szInputFileName, '?')) {
    return;
  }
  if (d.mask & mask_archive) {
    ok = 0;
    string supportedExtensions;
    for (int k = 0; k < MAX_ARCS; k++) {
      if (arcs[k].extension[0] && arcs[k].extension[0] != ' ') {
        if (!supportedExtensions.empty()) {
          supportedExtensions += ", ";
        }
        supportedExtensions += arcs[k].extension;
        if (wwiv::strings::IsEquals(szInputFileName + 9, arcs[k].extension)) {
          ok = 1;
        }
      }
    }
    if (!ok) {
      bout.nl();
      bout << "Sorry, all uploads to this dir must be archived.  Supported types are:\r\n" <<
                         supportedExtensions << "\r\n\n";
      return;
    }
  }
  strcpy(u.filename, szInputFileName);
  u.ownerusr = static_cast<unsigned short>(session()->usernum);
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  strcpy(u.upby, session()->user()->GetUserNameAndNumber(session()->usernum));
  strcpy(u.date, date());
  bout.nl();
  ok = 1;
  bool xfer = true;
  if (check_batch_queue(u.filename)) {
    ok = 0;
    bout.nl();
    bout << "That file is already in the batch queue.\r\n\n";
  } else {
    if (!wwiv::strings::IsEquals(szInputFileName, "        .   ")) {
      bout << "|#5Upload '" << szInputFileName << "' to " << d.name << "? ";
    } else {
      ok = 0;
    }
  }
  char szUnalignedFile[MAX_PATH];
  strcpy(szUnalignedFile, szInputFileName);
  unalign(szUnalignedFile);
  char szReceiveFileName[ MAX_PATH ];
  sprintf(szReceiveFileName, "%s%s", d.path, szUnalignedFile);
  if (ok && yesno()) {
    if (File::Exists(d.path, szUnalignedFile)) {
      if (dcs()) {
        xfer = false;
        bout.nl(2);
        bout << "File already exists.\r\n|#5Add to database anyway? ";
        if (!yesno()) {
          ok = 0;
        }
      } else {
        bout.nl(2);
        bout << "That file is already here.\r\n\n";
        ok = 0;
      }
    } else if (!incom) {
      bout.nl();
      bout << "File isn't already there.\r\nCan't upload locally.\r\n\n";
      ok = 0;
    }
    if ((d.mask & mask_PD) && ok) {
      bout.nl();
      bout << "|#5Is this program PD/Shareware? ";
      if (!yesno()) {
        bout.nl();
        bout << "This directory is for Public Domain/\r\nShareware programs ONLY.  Please do not\r\n";
        bout << "upload other programs.  If you have\r\ntrouble with this policy, please contact\r\n";
        bout << "the sysop.\r\n\n";
        const string message = StringPrintf("Wanted to upload \"%s\"", u.filename);
        add_ass(5, message.c_str());
        ok = 0;
      } else {
        u.mask = mask_PD;
      }
    }
    if (ok && !application()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
      bout.nl();
      bout << "Checking for same file in other directories...\r\n\n";
      int nLastLineLength = 0;
      for (int i = 0; i < session()->num_dirs && udir[i].subnum != -1; i++) {
        string buffer = "Scanning ";
        buffer += directories[udir[i].subnum].name;
        int nBufferLen = buffer.length();
        for (int i3 = nBufferLen; i3 < nLastLineLength; i3++) {
          buffer += " ";
        }
        nLastLineLength = nBufferLen;
        bout << buffer << "\r";
        dliscan1(udir[i].subnum);
        int i1 = recno(u.filename);
        if (i1 >= 0) {
          bout.nl();
          bout << "Same file found on " << directories[udir[i].subnum].name << wwiv::endl;
          if (dcs()) {
            bout.nl();
            bout << "|#5Upload anyway? ";
            if (!yesno()) {
              ok = 0;
              break;
            }
            bout.nl();
          } else {
            ok = 0;
            break;
          }
        }
      }
      string filler = charstr(nLastLineLength, SPACE);
      bout << filler << "\r";
      if (ok) {
        dliscan1(dn);
      }
      bout.nl();
    }
    if (ok) {
      bout.nl();
      bout << "Please enter a one line description.\r\n:";
      inputl(u.description, 58);
      bout.nl();
      char *pszExtendedDescription = nullptr;
      modify_extended_description(&pszExtendedDescription, directories[dn].name, u.filename);
      if (pszExtendedDescription) {
        add_extended_description(u.filename, pszExtendedDescription);
        u.mask |= mask_extended;
        free(pszExtendedDescription);
      }
      bout.nl();
      if (xfer) {
        write_inst(INST_LOC_UPLOAD, udir[session()->GetCurrentFileArea()].subnum, INST_FLAGS_ONLINE);
        double ti = timer();
        receive_file(szReceiveFileName, &ok, u.filename, dn);
        ti = timer() - ti;
        if (ti < 0) {
          ti += SECONDS_PER_HOUR_FLOAT * HOURS_PER_DAY_FLOAT;
        }
        session()->user()->SetExtraTime(session()->user()->GetExtraTime() + static_cast<float>(ti));
      }
      if (ok) {
        File file(szReceiveFileName);
        if (ok == 1) {
          if (!file.Open(File::modeBinary | File::modeReadOnly)) {
            ok = 0;
            bout.nl(2);
            bout << "OS error - File not found.\r\n\n";
            if (u.mask & mask_extended) {
              delete_extended_description(u.filename);
            }
          }
          if (ok && !syscfg.upload_cmd.empty()) {
            file.Close();
            bout << "Please wait...\r\n";
            if (!check_ul_event(dn, &u)) {
              if (u.mask & mask_extended) {
                delete_extended_description(u.filename);
              }
              ok = 0;
            } else {
              file.Open(File::modeBinary | File::modeReadOnly);
            }
          }
        }
        if (ok) {
          if (ok == 1) {
            long lFileLength = file.GetLength();
            u.numbytes = lFileLength;
            file.Close();
            session()->user()->SetFilesUploaded(session()->user()->GetFilesUploaded() + 1);
            modify_database(u.filename, true);
            session()->user()->SetUploadK(session()->user()->GetUploadK() + bytes_to_k(u.numbytes));

            get_file_idz(&u, dn);
          } else {
            u.numbytes = 0;
          }
          time_t lCurrentTime;
          time(&lCurrentTime);
          u.daten = static_cast<unsigned long>(lCurrentTime);
          File fileDownload(g_szDownloadFileName);
          fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
          for (int j = session()->numf; j >= 1; j--) {
            FileAreaSetRecord(fileDownload, j);
            fileDownload.Read(&u1, sizeof(uploadsrec));
            FileAreaSetRecord(fileDownload, j + 1);
            fileDownload.Write(&u1, sizeof(uploadsrec));
          }
          FileAreaSetRecord(fileDownload, 1);
          fileDownload.Write(&u, sizeof(uploadsrec));
          ++session()->numf;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          u1.numbytes = session()->numf;
          u1.daten = lCurrentTime;
          session()->m_DirectoryDateCache[dn] = lCurrentTime;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Write(&u1, sizeof(uploadsrec));
          fileDownload.Close();
          if (ok == 1) {
            WStatus *pStatus = application()->GetStatusManager()->BeginTransaction();
            pStatus->IncrementNumUploadsToday();
            pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
            application()->GetStatusManager()->CommitTransaction(pStatus);
            sysoplogf("+ \"%s\" uploaded on %s", u.filename, directories[dn].name);
            bout.nl(2);
            bout.bprintf("File uploaded.\r\n\nYour ratio is now: %-6.3f\r\n", ratio());
            bout.nl(2);
            if (session()->IsUserOnline()) {
              application()->UpdateTopScreen();
            }
          }
        }
      } else {
        bout.nl(2);
        bout << "File transmission aborted.\r\n\n";
        if (u.mask & mask_extended) {
          delete_extended_description(u.filename);
        }
      }
    }
  }
}

