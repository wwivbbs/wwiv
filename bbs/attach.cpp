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
#include "bbs/attach.h"

#include <memory>
#include <string>
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "common/com.h"
#include "bbs/email.h"
#include "common/input.h"
#include "bbs/read_message.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "local_io/wconstants.h"
#include "common/output.h"
#include "bbs/xfer.h"
#include "sdk/user.h"
#include "core/file.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;

void attach_file(int mode) {
  bool found;
  std::string full_pathname;
  std::string file_to_attach;
  filestatusrec fsr{};

  bout.nl();
  bool bDirectionForward = true;
  auto pFileEmail(OpenEmailFile(true));
  if (!pFileEmail->IsOpen()) {
    bout << "\r\nNo mail.\r\n";
    pFileEmail->Close();
    return;
  }
  auto max = pFileEmail->length() / sizeof(mailrec);
  auto cur = (bDirectionForward) ? max - 1 : 0;

  int ok = 0;
  bool done = false;
  do {
    mailrec m{};
    pFileEmail->Seek(cur * sizeof(mailrec), File::Whence::begin);
    pFileEmail->Read(&m, sizeof(mailrec));
    while ((m.fromsys != 0 || m.fromuser != a()->sess().user_num() || m.touser == 0) &&
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
    if (m.fromsys != 0 || m.fromuser != a()->sess().user_num() || m.touser == 0 || cur >= max ||
        cur < 0) {
      done = true;
    } else {
      bool done1 = false;
      do {
        done1 = false;
        bout.nl();
        if (m.tosys == 0) {
          bout << "|#1  To|#7: |#2";
          if ((m.anony & (anony_receiver | anony_receiver_pp | anony_receiver_da)) &&
              (a()->config()->sl(a()->sess().effective_sl()).ability & ability_read_email_anony) == 0) {
            bout << ">UNKNOWN<";
          } else {
            bout << a()->names()->UserName(m.touser);
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
          File fileAttach(FilePath(a()->config()->datadir(), ATTACH_DAT));
          if (fileAttach.Open(File::modeBinary | File::modeReadOnly)) {
            found = false;
            auto lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
            while (lNumRead > 0 && !found) {
              if (m.daten == static_cast<uint32_t>(fsr.id)) {
                bout << "|#1Filename|#0.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0";
                found = true;
              }
              if (!found) {
                lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
              }
            }
            if (!found) {
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
          bool newname = false;
          bool done2 = false;
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
              File attachFile(FilePath(a()->config()->datadir(), ATTACH_DAT));
              if (attachFile.Open(File::modeReadWrite | File::modeBinary)) {
                found = false;
                auto lNumRead = attachFile.Read(&fsr, sizeof(fsr));
                while (lNumRead > 0 && !found) {
                  if (m.daten == static_cast<uint32_t>(fsr.id)) {
                    fsr.id = 0;
                    File::Remove(FilePath(a()->GetAttachmentDirectory(), fsr.filename));
                    attachFile.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
                    attachFile.Write(&fsr, sizeof(fsr));
                  }
                  if (!found) {
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
          if (File::freespace_for_path(a()->GetAttachmentDirectory()) < 500) {
            bout << "Not enough free space to attach a file.\r\n";
          } else {
            if (!done2) {
              bool bRemoteUpload = false;
              found = false;
              bout.nl();
              if (so()) {
                if (a()->sess().incom()) {
                  bout << "|#5Upload from remote? ";
                  if (bin.yesno()) {
                    bRemoteUpload = true;
                  }
                }
                if (!bRemoteUpload) {
                  bout << "|#5Path/filename (wildcards okay) : \r\n";
                  file_to_attach = bin.input(35, true);
                  if (!file_to_attach.empty()) {
                    bout.nl();
                    if (strchr(file_to_attach.c_str(), '*') != nullptr || strchr(file_to_attach.c_str(), '?') != nullptr) {
                      file_to_attach = get_wildlist(file_to_attach);
                    }
                    if (!File::Exists(file_to_attach)) {
                      found = true;
                    }
                    if (!okfn(stripfn(file_to_attach)) || strchr(file_to_attach.c_str(), '?')) {
                      found = true;
                    }
                  }
                  if (!found && !file_to_attach.empty()) {
                    full_pathname = FilePath(a()->GetAttachmentDirectory(), stripfn(file_to_attach)).string();
                    bout.nl();
                    bout << "|#5" << file_to_attach << "? ";
                    if (!bin.yesno()) {
                      found = true;
                    }
                  }
                }
              }
              std::string new_filename;
              if (!so() || bRemoteUpload) {
                bout << "|#2Filename: ";
                file_to_attach = bin.input(12, true);
                new_filename = FilePath(a()->GetAttachmentDirectory(), file_to_attach).string();
                if (!okfn(file_to_attach) || strchr(file_to_attach.c_str(), '?')) {
                  found = true;
                }
              }
              if (File::Exists(new_filename)) {
                bout << "Target file exists.\r\n";
                bool done3 = false;
                do {
                  found = true;
                  bout << "|#5New name? ";
                  if (bin.yesno()) {
                    bout << "|#5Filename: ";
                    new_filename = bin.input(12, true);
                    full_pathname = FilePath(a()->GetAttachmentDirectory(), new_filename).string();
                    if (okfn(new_filename) && !strchr(new_filename.c_str(), '?') && !File::Exists(new_filename)) {
                      found = false;
                      done3 = true;
                      newname = true;
                    } else {
                      bout << "Try Again.\r\n";
                    }
                  } else {
                    done3 = true;
                  }
                } while (!done3 && !a()->sess().hangup());
              }
              File fileAttach(FilePath(a()->config()->datadir(), ATTACH_DAT));
              if (fileAttach.Open(File::modeBinary | File::modeReadOnly)) {
                auto lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
                while (lNumRead > 0 && !found) {
                  if (m.daten == static_cast<uint32_t>(fsr.id)) {
                    found = true;
                  } else {
                    lNumRead = fileAttach.Read(&fsr, sizeof(fsr));
                  }
                }
                fileAttach.Close();
              }
              if (found) {
                bout << "File already exists or invalid filename.\r\n";
              } else {
                if (so() && !bRemoteUpload) {
                  std::error_code ec;
                  if (!std::filesystem::copy_file(file_to_attach, full_pathname, ec)) {
                    bout << "done.\r\n";
                    ok = 1;
                  } else {
                    bout << "\r\n|#6Error in copy.\r\n";
                    bin.getkey();
                  }
                } else {
                  const auto full_path = FilePath(a()->GetAttachmentDirectory(), file_to_attach);
                  receive_file(full_path.string(), &ok, "", 0);
                }
                if (ok) {
                  File attachmentFile(full_pathname);
                  if (!attachmentFile.Open(File::modeReadOnly | File::modeBinary)) {
                    ok = 0;
                    bout << "\r\n\nDOS error - File not bFound.\r\n\n";
                  } else {
                    fsr.numbytes = static_cast<decltype(fsr.numbytes)>(attachmentFile.length());
                    attachmentFile.Close();
                    if (newname) {
                      to_char_array(fsr.filename, stripfn(new_filename));
                    } else {
                      to_char_array(fsr.filename, stripfn(file_to_attach));
                    }
                    fsr.id = m.daten;
                    bout << "|#5Attach " << fsr.filename << " (" << fsr.numbytes << " bytes) to Email? ";
                    if (bin.yesno()) {
                      m.status ^= status_file;
                      pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * -1L, File::Whence::current);
                      pFileEmail->Write(&m, sizeof(mailrec));
                      File attachFile(FilePath(a()->config()->datadir(), ATTACH_DAT));
                      if (!attachFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
                        bout << "Could not write attachment data.\r\n";
                        m.status ^= status_file;
                        pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * -1L, File::Whence::current);
                        pFileEmail->Write(&m, sizeof(mailrec));
                      } else {
                        filestatusrec fsr1{};
                        fsr1.id = 1;
                        File::size_type num_read = 0;
                        do {
                          num_read = attachFile.Read(&fsr1, sizeof(filestatusrec));
                        } while (num_read > 0 && fsr1.id != 0);

                        if (fsr1.id == 0) {
                          attachFile.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
                        } else {
                          attachFile.Seek(0L, File::Whence::end);
                        }
                        attachFile.Write(&fsr, sizeof(filestatusrec));
                        attachFile.Close();
                        const auto to_user_name = a()->names()->UserName(m.touser);
                        sysoplog()
                            << fmt::sprintf("Attached %s (%u bytes) in message to %s", fsr.filename,
                                            fsr.numbytes, to_user_name);
                        bout << "File attached.\r\n" ;
                      }
                    } else {
                      File::Remove(FilePath(a()->GetAttachmentDirectory(), fsr.filename));
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
          auto msg = read_type2_message(&m.msg, m.anony & 0x0f, false, "email", 0, 0);
          msg.title = m.title;
          int fake_msgno = -1;
          display_type2_message(fake_msgno, msg, &next);
          if (m.status & status_file) {
            File f(FilePath(a()->config()->datadir(), ATTACH_DAT));
            if (f.Open(File::modeReadOnly | File::modeBinary)) {
              found = false;
              auto num_read = f.Read(&fsr, sizeof(fsr));
              while (num_read > 0 && !found) {
                if (m.daten == static_cast<uint32_t>(fsr.id)) {
                  bout << "Attached file: " << fsr.filename << " (" << fsr.numbytes << " bytes).";
                  bout.nl();
                  found = true;
                }
                if (!found) {
                  num_read = f.Read(&fsr, sizeof(fsr));
                }
              }
              if (!found) {
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
      } while (!a()->sess().hangup() && !done1);
    }
  } while (!done && !a()->sess().hangup());
}

