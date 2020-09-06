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
#include "common/com.h"
#include "common/input.h"
#include "bbs/instmsg.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "core/numbers.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
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
  files::FileRecord f;
  int ok = 1;

  const auto& d = a()->dirs()[dn];
  dliscan1(d);
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
        if (!bin.yesno()) {
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
  f.set_filename(input_fn);
  f.u().ownerusr = a()->usernum;
  f.u().ownersys = 0;
  f.u().numdloads = 0;
  f.u().unused_filetype = 0;
  f.u().mask = 0;
  const auto unn = a()->names()->UserName(a()->usernum);
  to_char_array(f.u().upby, unn);
  to_char_array(f.u().date, date());
  bout.nl();
  ok = 1;
  bool xfer = true;
  if (a()->batch().contains_file(f.filename())) {
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
  if (ok && bin.yesno()) {
    if (File::Exists(receive_fn)) {
      if (dcs()) {
        xfer = false;
        bout.nl(2);
        bout << "File already exists.\r\n|#5Add to database anyway? ";
        if (!bin.yesno()) {
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
    if (d.mask & mask_PD && ok) {
      bout.nl();
      bout << "|#5Is this program PD/Shareware? ";
      if (!bin.yesno()) {
        bout.nl();
        bout << "This directory is for Public Domain/\r\nShareware programs ONLY.  Please do not\r\n";
        bout << "upload other programs.  If you have\r\ntrouble with this policy, please contact\r\n";
        bout << "the sysop.\r\n\n";
        const auto message = fmt::format("Wanted to upload \"{}\"", f);
        sysoplog() << "*** ASS-PTS: " << 5 << ", Reason: [" << message << "]";
        a()->user()->IncrementAssPoints(5);
        ok = 0;
      } else {
        f.set_mask(mask_PD, true);
      }
    }
    if (ok) {
      bout.nl();
      bout << "Please enter a one line description.\r\n:";
      auto desc = bin.input_text(58);
      f.set_description(desc);
      bout.nl();
      string ext_desc;
      modify_extended_description(&ext_desc, a()->dirs()[dn].name);
      if (!ext_desc.empty()) {
        a()->current_file_area()->AddExtendedDescription(f, ext_desc);
        f.set_extended_description(true);
      }
      bout.nl();
      if (xfer) {
        write_inst(INST_LOC_UPLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
        auto ti = std::chrono::system_clock::now();
        receive_file(receive_fn.string(), &ok, f.filename().aligned_filename(), dn);
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
            if (f.mask(mask_extended)) {
              a()->current_file_area()->DeleteExtendedDescription(f.filename());
            }
          }
          if (ok && !a()->upload_cmd.empty()) {
            file.Close();
            bout << "Please wait...\r\n";
            if (!check_ul_event(dn, &f.u())) {
              if (f.mask(mask_extended)) {
                a()->current_file_area()->DeleteExtendedDescription(f.filename());
              }
              ok = 0;
            } else {
              file.Open(File::modeBinary | File::modeReadOnly);
            }
          }
        }
        if (ok) {
          if (ok == 1) {
            f.set_numbytes(file.length());
            file.Close();
            a()->user()->SetFilesUploaded(a()->user()->GetFilesUploaded() + 1);
            add_to_file_database(f);
            a()->user()->set_uk(a()->user()->uk() + bytes_to_k(f.numbytes()));

            get_file_idz(f, a()->dirs()[dn]);
          } else {
            f.set_numbytes(0);
          }
          f.set_date(DateTime::now());
          auto* area = a()->current_file_area();
          if (area->AddFile(f)) {
            area->Save();
          }
          if (ok == 1) {
            a()->status_manager()->Run([](WStatus& s) {
              s.IncrementNumUploadsToday();
              s.IncrementFileChangedFlag(WStatus::fileChangeUpload);
            });
            sysoplog() << fmt::format("+ \"{}\" uploaded on {}", f, a()->dirs()[dn].name);
            bout.nl(2);
            bout.bprintf("File uploaded.\r\n\nYour ratio is now: %-6.3f\r\n", ratio());
            bout.nl(2);
            if (a()->context().IsUserOnline()) {
              a()->UpdateTopScreen();
            }
          }
        }
      } else {
        bout.nl(2);
        bout << "File transmission aborted.\r\n\n";
        if (f.mask(mask_extended)) {
          a()->current_file_area()->DeleteExtendedDescription(f.filename());
        }
      }
    }
  }
}

