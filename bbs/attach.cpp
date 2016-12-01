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
#include "bbs/attach.h"

#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/email.h"
#include "bbs/input.h"
#include "bbs/read_message.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/wconstants.h"
#include "bbs/output.h"
#include "bbs/vars.h"
#include "bbs/xfer.h"
#include "sdk/user.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::strings;
using namespace wwiv::sdk;

void attach_file(int mode) {
  bool bFound;
  char szFullPathName[MAX_PATH], szNewFileName[MAX_PATH], szFileToAttach[MAX_PATH];
  User u;
  filestatusrec fsr;

  bout.nl();
  bool bDirectionForward = true;
  unique_ptr<File> pFileEmail(OpenEmailFile(true));
  WWIV_ASSERT(pFileEmail);
  if (!pFileEmail->IsOpen()) {
    bout << "\r\nNo mail.\r\n";
    pFileEmail->Close();
    return;
  }
  auto max = pFileEmail->GetLength() / sizeof(mailrec);
  auto cur = (bDirectionForward) ? max - 1 : 0;

  int ok = 0;
  bool done = false;
  do {
    mailrec m;
    pFileEmail->Seek(cur * sizeof(mailrec), File::Whence::begin);
    pFileEmail->Read(&m, sizeof(mailrec));
    while ((m.fromsys != 0 || m.fromuser != session()->usernum || m.touser == 0) &&
           cur < max && cur >= 0) {
      if (bDirectionForward) {
        --cur;
      } else {
        ++cur;
      }
      if (cur < max && cur >= 0) {
        pFileEmail->Seek(cur * sizeof(mailrec), File::Whence::begin);
        pFileEmail->Read(&m, sizeof(mailrec));
      }
    }
    if (m.fromsys != 0 || m.fromuser != session()->usernum || m.touser == 0 || cur >= max || cur < 0) {
      done = true;
    } else {
      bool done1 = false;
      do {
        done1 = false;
        bout.nl();
        if (m.tosys == 0) {
          bout << "|#1  To|#7: |#2";
          if ((m.anony & (anony_receiver | anony_receiver_pp | anony_receiver_da)) &&
              (getslrec(session()->GetEffectiveSl()).ability & ability_read_email_anony) == 0) {
            bout << ">UNKNOWN<";
          } else {
            bout << session()->names()->UserName(m.touser);
          }
          bout.nl();
        } else {
          bout << "|#1To|#7: |#2User " << m.tosys << " System " << m.touser << wwiv::endl;
        }
        bout << "|#1Subj|#7: |#2" << m.title << wwiv::endl;
        time_t tTimeNow = time(nullptr);
        int nDaysAgo = static_cast<int>((tTimeNow - m.daten) / SECONDS_PER_DAY);
        bout << "|#1Sent|#7: |#2 " << nDaysAgo << " days ago" << wwiv::endl;
        if (m.status & status_file) {
          File fileAttach(session()->config()->datadir(), ATTACH_DAT);
          if (fileAttach.Open(File::modeBinary | File::modeReadOnly)) {
            bFound = false;
            auto lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
            while (lNumRead > 0 && !bFound) {
              if (m.daten == static_cast<uint32_t>(fsr.id)) {
                bout << "|#1Filename|#0.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0";
                bFound = true;
              }
              if (!bFound) {
                lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
              }
            }
            if (!bFound) {
              bout << "|#1Filename|#0.... |#2Unknown or missing|#0\r\n";
            }
            fileAttach.Close();
          } else {
            bout << "|#1Filename|#0.... |#2Unknown or missing|#0\r\n";
          }
        }
        bout.nl();
        char ch = 0;
        if (mode == 0) {
          bout << "|#9(R)ead, (A)ttach, (N)ext, (Q)uit : ";
          ch = onek("QRAN");
        } else {
          bout << "|#9(R)ead, (A)ttach, (Q)uit : ";
          ch = onek("QRA");
        }
        switch (ch) {
        case 'Q':
          done1 = true;
          done = true;
          break;
        case 'A': {
          bool newname    = false;
          bool done2      = false;
          if (m.status & status_file) {
            bout << "|#6File already attached, (D)elete, (O)verwrite, or (Q)uit? : ";
            char ch1 = onek("QDO");
            switch (ch1) {
            case 'Q':
              done2 = true;
              break;
            case 'D':
            case 'O': {
              m.status ^= status_file;
              pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * -1L, File::Whence::current);
              pFileEmail->Write(&m, sizeof(mailrec));
              File attachFile(session()->config()->datadir(), ATTACH_DAT);
              if (attachFile.Open(File::modeReadWrite | File::modeBinary)) {
                bFound = false;
                auto lNumRead = attachFile.Read(&fsr, sizeof(fsr));
                while (lNumRead > 0 && !bFound) {
                  if (m.daten == static_cast<uint32_t>(fsr.id)) {
                    fsr.id = 0;
                    File::Remove(session()->GetAttachmentDirectory(), fsr.filename);
                    attachFile.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
                    attachFile.Write(&fsr, sizeof(fsr));
                  }
                  if (!bFound) {
                    lNumRead = attachFile.Read(&fsr, sizeof(fsr));
                  }
                }
                attachFile.Close();
                bout << "File attachment removed.\r\n";
              }
              if (ch1 == 'D') {
                done2 = true;
              }
              break;
            }
            }
          }
          if (File::GetFreeSpaceForPath(session()->GetAttachmentDirectory()) < 500) {
            bout << "Not enough free space to attach a file.\r\n";
          } else {
            if (!done2) {
              bool bRemoteUpload = false;
              bFound = false;
              bout.nl();
              if (so()) {
                if (incom) {
                  bout << "|#5Upload from remote? ";
                  if (yesno()) {
                    bRemoteUpload = true;
                  }
                }
                if (!bRemoteUpload) {
                  bout << "|#5Path/filename (wildcards okay) : \r\n";
                  input(szFileToAttach, 35, true);
                  if (szFileToAttach[0]) {
                    bout.nl();
                    if (strchr(szFileToAttach, '*') != nullptr || strchr(szFileToAttach, '?') != nullptr) {
                      strcpy(szFileToAttach, get_wildlist(szFileToAttach));
                    }
                    if (!File::Exists(szFileToAttach)) {
                      bFound = true;
                    }
                    if (!okfn(stripfn(szFileToAttach)) || strchr(szFileToAttach, '?')) {
                      bFound = true;
                    }
                  }
                  if (!bFound && szFileToAttach[0]) {
                    sprintf(szFullPathName, "%s%s", session()->GetAttachmentDirectory().c_str(), stripfn(szFileToAttach));
                    bout.nl();
                    bout << "|#5" << szFileToAttach << "? ";
                    if (!yesno()) {
                      bFound = true;
                    }
                  }
                }
              }
              if (!so() || bRemoteUpload) {
                bout << "|#2Filename: ";
                input(szFileToAttach, 12, true);
                sprintf(szFullPathName, "%s%s", session()->GetAttachmentDirectory().c_str(), szFileToAttach);
                if (!okfn(szFileToAttach) || strchr(szFileToAttach, '?')) {
                  bFound = true;
                }
              }
              if (File::Exists(szFullPathName)) {
                bout << "Target file exists.\r\n";
                bool done3 = false;
                do {
                  bFound = true;
                  bout << "|#5New name? ";
                  if (yesno()) {
                    bout << "|#5Filename: ";
                    input(szNewFileName, 12, true);
                    sprintf(szFullPathName, "%s%s", session()->GetAttachmentDirectory().c_str(), szNewFileName);
                    if (okfn(szNewFileName) && !strchr(szNewFileName, '?') && !File::Exists(szFullPathName)) {
                      bFound   = false;
                      done3   = true;
                      newname = true;
                    } else {
                      bout << "Try Again.\r\n";
                    }
                  } else {
                    done3 = true;
                  }
                } while (!done3 && !hangup);
              }
              File fileAttach(session()->config()->datadir(), ATTACH_DAT);
              if (fileAttach.Open(File::modeBinary | File::modeReadOnly)) {
                auto lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
                while (lNumRead > 0 && !bFound) {
                  if (m.daten == static_cast<uint32_t>(fsr.id)) {
                    bFound = true;
                  } else {
                    lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
                  }
                }
                fileAttach.Close();
              }
              if (bFound) {
                bout << "File already exists or invalid filename.\r\n";
              } else {
                if (so() && !bRemoteUpload) {
                  // Copy file s to szFullPathName.
                  if (!File::Copy(szFileToAttach, szFullPathName)) {
                    bout << "done.\r\n";
                    ok = 1;
                  } else {
                    bout << "\r\n|#6Error in copy.\r\n";
                    bout.getkey();
                  }
                } else {
                  sprintf(szFullPathName, "%s%s", session()->GetAttachmentDirectory().c_str(), szFileToAttach);
                  receive_file(szFullPathName, &ok, "", 0);
                }
                if (ok) {
                  File attachmentFile(szFullPathName);
                  if (!attachmentFile.Open(File::modeReadOnly | File::modeBinary)) {
                    ok = 0;
                    bout << "\r\n\nDOS error - File not bFound.\r\n\n";
                  } else {
                    fsr.numbytes = attachmentFile.GetLength();
                    attachmentFile.Close();
                    if (newname) {
                      strcpy(fsr.filename, stripfn(szNewFileName));
                    } else {
                      strcpy(fsr.filename, stripfn(szFileToAttach));
                    }
                    fsr.id = m.daten;
                    bout << "|#5Attach " << fsr.filename << " (" << fsr.numbytes << " bytes) to Email? ";
                    if (yesno()) {
                      m.status ^= status_file;
                      pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * -1L, File::Whence::current);
                      pFileEmail->Write(&m, sizeof(mailrec));
                      File attachFile(session()->config()->datadir(), ATTACH_DAT);
                      if (!attachFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
                        bout << "Could not write attachment data.\r\n";
                        m.status ^= status_file;
                        pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * -1L, File::Whence::current);
                        pFileEmail->Write(&m, sizeof(mailrec));
                      } else {
                        filestatusrec fsr1;
                        fsr1.id = 1;
                        long lNumRead = 0;
                        do {
                          lNumRead = attachFile.Read(&fsr1, sizeof(filestatusrec));
                        } while (lNumRead > 0 && fsr1.id != 0);

                        if (fsr1.id == 0) {
                          attachFile.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
                        } else {
                          attachFile.Seek(0L, File::Whence::end);
                        }
                        attachFile.Write(&fsr, sizeof(filestatusrec));
                        attachFile.Close();
                        const string to_user_name = session()->names()->UserName(m.touser);
                        sysoplog() << StringPrintf("Attached %s (%u bytes) in message to %s",
                                       fsr.filename, fsr.numbytes, to_user_name.c_str());
                        bout << "File attached.\r\n" ;
                      }
                    } else {
                      File::Remove(session()->GetAttachmentDirectory().c_str(), fsr.filename);
                    }
                  }
                }
              }
            }
          }
        }
        break;
        case 'N':
          done1 = true;
          if (bDirectionForward) {
            --cur;
          } else {
            ++cur;
          }
          if (cur >= max || cur < 0) {
            done = true;
          }
          break;
        case 'R': {
          bout.nl(2);
          bool next;
          Type2MessageData msg = read_type2_message(&m.msg, m.anony & 0x0f, false, "email", 0, 0);
          msg.title = m.title;
          display_type2_message(msg, static_cast<char>(m.anony & 0x0f), &next);
          if (m.status & status_file) {
            File f(session()->config()->datadir(), ATTACH_DAT);
            if (f.Open(File::modeReadOnly | File::modeBinary)) {
              bFound = false;
              auto num_read = f.Read(&fsr, sizeof(fsr));
              while (num_read > 0 && !bFound) {
                if (m.daten == static_cast<uint32_t>(fsr.id)) {
                  bout << "Attached file: " << fsr.filename << " (" << fsr.numbytes << " bytes).";
                  bout.nl();
                  bFound = true;
                }
                if (!bFound) {
                  num_read = f.Read(&fsr, sizeof(fsr));
                }
              }
              if (!bFound) {
                bout << "File attached but attachment data missing.  Alert sysop!\r\n";
              }
              f.Close();
            } else {
              bout << "File attached but attachment data missing.  Alert sysop!\r\n";
            }
          }
        }
        break;
        }
      } while (!hangup && !done1);
    }
  } while (!done && !hangup);
}

