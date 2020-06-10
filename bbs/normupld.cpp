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
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/files/files.h"

#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

//////////////////////////////////////////////////////////////////////////////
// Implementation

void normalupload(int dn) {
  uploadsrec u, u1;
  memset(&u, 0, sizeof(uploadsrec));
  memset(&u1, 0, sizeof(uploadsrec));

  int ok = 1;

  dliscan1(dn);
  directoryrec_422_t d = a()->directories[dn];
  if (a()->current_file_area()->number_of_files() >= d.maxfiles) {
    bout.nl(3);
    bout << "This directory is currently full.\r\n\n";
    return;
  }
  if (d.mask & mask_no_uploads && !dcs()) {
    bout.nl(2);
    bout << "Uploads are not allowed to this directory.\r\n\n";
    return;
  }
  bout << "|#9Filename: ";
  auto input_fn = input(12);
  if (!okfn(input_fn)) {
    input_fn.clear();
  } else {
    if (!is_uploadable(input_fn)) {
      if (so()) {
        bout.nl();
        bout << "|#5In filename database - add anyway? ";
        if (!yesno()) {
          input_fn.clear();
        }
      } else {
        input_fn.clear();
        bout.nl();
        bout << "|#6File either already here or unwanted.\r\n";
      }
    }
  }
  input_fn = aligns(input_fn);
  if (strchr(input_fn.c_str(), '?')) {
    return;
  }
  if (d.mask & mask_archive) {
    ok = 0;
    string supportedExtensions;
    for (int k = 0; k < MAX_ARCS; k++) {
      if (a()->arcs[k].extension[0] && a()->arcs[k].extension[0] != ' ') {
        if (!supportedExtensions.empty()) {
          supportedExtensions += ", ";
        }
        supportedExtensions += a()->arcs[k].extension;
        if (iequals(input_fn.substr(9), a()->arcs[k].extension)) {
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
  to_char_array(u.filename, input_fn);
  u.ownerusr = a()->usernum;
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  const auto unn = a()->names()->UserName(a()->usernum);
  to_char_array(u.upby, unn);
  to_char_array(u.date, date());
  bout.nl();
  ok = 1;
  bool xfer = true;
  if (a()->batch().contains_file(u.filename)) {
    ok = 0;
    bout.nl();
    bout << "That file is already in the batch queue.\r\n\n";
  } else {
    if (!wwiv::strings::iequals(input_fn, "        .   ")) {
      bout << "|#5Upload '" << input_fn << "' to " << d.name << "? ";
    } else {
      ok = 0;
    }
  }
  const auto receive_fn = FilePath(d.path, files::unalign(input_fn));
  if (ok && yesno()) {
    if (File::Exists(receive_fn)) {
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
    } else if (!a()->context().incom()) {
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
        const string message = fmt::format("Wanted to upload \"{}\"", u.filename);
        sysoplog() << "*** ASS-PTS: " << 5 << ", Reason: [" << message << "]";
        a()->user()->IncrementAssPoints(5);
        ok = 0;
      } else {
        u.mask = mask_PD;
      }
    }
    if (ok && !a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
      bout.nl();
      bout << "Checking for same file in other a()->directories...\r\n\n";
      int nLastLineLength = 0;
      for (size_t i = 0; i < a()->directories.size() && a()->udir[i].subnum != -1; i++) {
        string buffer = "Scanning ";
        buffer += a()->directories[a()->udir[i].subnum].name;
        int nBufferLen = buffer.length();
        for (int i3 = nBufferLen; i3 < nLastLineLength; i3++) {
          buffer += " ";
        }
        nLastLineLength = nBufferLen;
        bout << buffer << "\r";
        dliscan1(a()->udir[i].subnum);
        int i1 = recno(u.filename);
        if (i1 >= 0) {
          bout.nl();
          bout << "Same file found on " << a()->directories[a()->udir[i].subnum].name << wwiv::endl;
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

      bout << string(nLastLineLength, SPACE) << "\r";
      if (ok) {
        dliscan1(dn);
      }
      bout.nl();
    }
    if (ok) {
      bout.nl();
      bout << "Please enter a one line description.\r\n:";
      auto desc = input_text(58);
      to_char_array(u.description, desc);
      bout.nl();
      string ext_desc;
      modify_extended_description(&ext_desc, a()->directories[dn].name);
      if (!ext_desc.empty()) {
        a()->current_file_area()->AddExtendedDescription(u.filename, ext_desc);
        u.mask |= mask_extended;
      }
      bout.nl();
      if (xfer) {
        write_inst(INST_LOC_UPLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
        auto ti = std::chrono::system_clock::now();
        receive_file(receive_fn.string(), &ok, u.filename, dn);
        auto used = std::chrono::system_clock::now() - ti;
        a()->user()->add_extratime(used);
      }
      if (ok) {
        File file(receive_fn);
        if (ok == 1) {
          if (!file.Open(File::modeBinary | File::modeReadOnly)) {
            ok = 0;
            bout.nl(2);
            bout << "OS error - File not found.\r\n\n";
            if (u.mask & mask_extended) {
              a()->current_file_area()->DeleteExtendedDescription(u.filename);
            }
          }
          if (ok && !a()->upload_cmd.empty()) {
            file.Close();
            bout << "Please wait...\r\n";
            if (!check_ul_event(dn, &u)) {
              if (u.mask & mask_extended) {
                a()->current_file_area()->DeleteExtendedDescription(u.filename);
              }
              ok = 0;
            } else {
              file.Open(File::modeBinary | File::modeReadOnly);
            }
          }
        }
        if (ok) {
          if (ok == 1) {
            u.numbytes = static_cast<daten_t>(file.length());
            file.Close();
            a()->user()->SetFilesUploaded(a()->user()->GetFilesUploaded() + 1);
            add_to_file_database(u.filename);
            a()->user()->set_uk(a()->user()->uk() + bytes_to_k(u.numbytes));

            get_file_idz(&u, dn);
          } else {
            u.numbytes = 0;
          }
          time_t lCurrentTime;
          time(&lCurrentTime);
          u.daten = static_cast<uint32_t>(lCurrentTime);
          auto* area = a()->current_file_area();
          wwiv::sdk::files::FileRecord f(u);
          if (area->AddFile(f)) {
            area->Save();
          }
          if (ok == 1) {
            a()->status_manager()->Run([](WStatus& s) {
              s.IncrementNumUploadsToday();
              s.IncrementFileChangedFlag(WStatus::fileChangeUpload);
            });
            sysoplog() << fmt::format("+ \"{}\" uploaded on {}", u.filename, a()->directories[dn].name);
            bout.nl(2);
            bout << fmt::sprintf("File uploaded.\r\n\nYour ratio is now: %-6.3f\r\n", ratio());
            bout.nl(2);
            if (a()->IsUserOnline()) {
              a()->UpdateTopScreen();
            }
          }
        }
      } else {
        bout.nl(2);
        bout << "File transmission aborted.\r\n\n";
        if (u.mask & mask_extended) {
          a()->current_file_area()->DeleteExtendedDescription(u.filename);
        }
      }
    }
  }
}

