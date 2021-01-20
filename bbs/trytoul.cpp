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
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "common/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/dirlist.h"
#include "common/input.h"
#include "bbs/listplus.h"
#include "common/pause.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "core/file.h"
#include "core/numbers.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/files/files.h"
#include <chrono>

using std::chrono::milliseconds;
using std::string;

using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void t2u_error(const string& file_name, const string& msg) {
  bout.nl(2);
  const auto s1 = StrCat("**  ", file_name, " failed T2U qualifications");
  bout << s1 << wwiv::endl;
  sysoplog() << s1;

  const auto s2 = StrCat("** Reason : ", msg);
  bout << s2 << wwiv::endl;
  bout.nl();
  sysoplog() << s2;
}

static int try_to_ul_wh(const string& orig_file_name) {
  wwiv::sdk::files::directory_t d{};
  char key;
  auto ok = 0, dn = 0;

  auto file_name = files::unalign(orig_file_name);

  if (!okfn(file_name)) {
    t2u_error(file_name, "Bad filename");          // bad filename
    return 1;
  }
  bout.cls();
  bout.nl(3);

  bool done = false;
  if (a()->user()->IsRestrictionValidate() || a()->user()->IsRestrictionUpload() ||
      (a()->config()->sysconfig_flags() & sysconfig_all_sysop)) {
    dn = (a()->config()->new_uploads_dir() < a()->dirs().size()) 
      ? a()->config()->new_uploads_dir() : 0;
  } else {
    char temp[10];

    // The a()->sess().hangup() check is below so uploads get uploaded even on a()->sess().hangup()
    done = false;
    while (!done) {
      if (a()->sess().hangup()) {
        if (a()->config()->new_uploads_dir() < a()->dirs().size()) {
          dn = a()->config()->new_uploads_dir();
        } else {
          dn = 0;
        }
        done = true;
        break;
      }
      // The sleep_for used to be a wait_sec_or_hit( 1 )
      sleep_for(milliseconds(500));
      bout << "\r\nUpload " << file_name << " to which dir? <CR>=0 ?=List \r\n";
      bin.input(temp, 5, true);
      StringTrim(temp);
      if (temp[0] == '?') {
        dirlist(1);
        break;
      }
      if (!temp[0]) {
        dn = 0;
        done = true;
        break;
      }
      int x = to_number<int>(temp);
      if (a()->udir[x].subnum >= 0) {
        dliscan1(a()->udir[x].subnum);
        d = a()->dirs()[dn];
        if (d.mask & mask_no_uploads && !dcs()) {
          bout << "Can't upload there...\r\n";
          bout.pausescr();
        } else {
          dn = a()->udir[x].subnum;
          done = true;
        }
      }
    }
  }

  d = a()->dirs()[dn];
  dliscan1(d);
  if (a()->current_file_area()->number_of_files() >= d.maxfiles) {
    t2u_error(file_name, "This directory is currently full.");
    return 1;
  }
  if (d.mask & mask_no_uploads && !dcs()) {
    t2u_error(file_name, "Uploads are not allowed to this directory.");
    return 1;
  }
  if (!is_uploadable(file_name)) {
    if (so()) {
      bout.nl();
      bout << "|#5In filename database - add anyway? ";
      if (!bin.yesno()) {
        t2u_error(file_name, "|#6File either already here or unwanted.");
        return 1;
      }
    } else {
      t2u_error(file_name, "|#6File either already here or unwanted.");
      return 1;
    }
  }
  string aligned_file_name = aligns(file_name);
  if (contains(aligned_file_name, '?')) {
    t2u_error(file_name, "Contains wildcards");
    return 1;
  }
  if (d.mask & mask_archive) {
    ok = 0;
    string s1;
    for (size_t i = 0; i < MAX_ARCS; i++) {
      if (a()->arcs[i].extension[0] && a()->arcs[i].extension[0] != ' ') {
        if (!s1.empty()) s1 += ", ";
        s1 += a()->arcs[i].extension;
        if (wwiv::strings::IsEquals(aligned_file_name.c_str() + 9, a()->arcs[i].extension)) {
          ok = 1;
        }
      }
    }
    if (!ok) {
      bout.nl();
      bout << "Sorry, all uploads to this directory must be archived.  Supported types are:\r\n";
      bout << s1;
      bout.nl(2);

      t2u_error(file_name, "Unsupported archive");
      return 1;
    }
  }
  files::FileRecord f;
  f.set_filename(aligned_file_name);
  f.u().ownerusr = static_cast<uint16_t>(a()->sess().user_num());
  const auto unn = a()->names()->UserName(a()->sess().user_num());
  to_char_array(f.u().upby, unn);
  f.set_date(DateTime::now());

  if (File::Exists(StrCat(d.path, files::unalign(aligned_file_name)))) {
    if (dcs()) {
      bout.nl(2);
      bout << "File already exists.\r\n|#5Add to database anyway? ";
      if (bin.yesno() == 0) {
        t2u_error(file_name, "That file is already here.");
        return 1;
      }
    } else {
      t2u_error(file_name, "That file is already here.");
      return 1;
    }
  }
  const auto src = FilePath(a()->sess().dirs().batch_directory(), file_name);
  const auto dest = FilePath(d.path, file_name);

  if (File::Exists(dest)) {
    File::Remove(dest);
  }

  // s1 and s2 should remain set,they are used below
  File::Move(src, dest);
  f.set_description("NO DESCRIPTION GIVEN");
  auto file_id_avail = get_file_idz(f, a()->dirs()[dn]);
  done = false;

  while (!done && !a()->sess().hangup() && !file_id_avail) {
    bout.cls();
    bout.nl();
    bout << "|#1Upload going to |#7" << d.name << "\r\n\n";
    bout << "   |#1Filename    |01: |#7" << file_name << wwiv::endl;
    bout << "|#2A|#7] |#1Description |01: |#7" << f.description() << wwiv::endl;
    bout << "|#2B|#7] |#1Modify extended description\r\n\n";
    print_extended(f.aligned_filename(), 10, -1, Color::YELLOW, nullptr);
    bout << "|#2<|#7CR|#2> |#1to continue, |#7Q|#1 to abort upload: ";
    key = onek("\rQABC", true);
    switch (key) {
    case 'Q':
      bout << "Are you sure, file will be lost? ";
      if (bin.yesno()) {
        t2u_error(file_name, "Changed mind");
        // move file back to batch dir
        File::Move(dest, src);
        return 1;
      }
      break;

    case 'A': {
      bout.nl();
      bout << "Please enter a one line description.\r\n:";
      auto desc = bin.input_text(58);
      f.set_description(desc);
    } break;

    case 'B':
    {
      auto* area = a()->current_file_area();
      bout.nl();
      auto ss = area->ReadExtendedDescriptionAsString(f).value_or("");
      bout << "|#5Modify extended description? ";
      if (bin.yesno()) {
        bout.nl();
        if (!ss.empty()) {
          bout << "|#5Delete it? ";
          if (bin.yesno()) {
            area->DeleteExtendedDescription(f.filename());
            f.set_mask(mask_extended, false);
          } else {
            f.set_mask(mask_extended, true);
            modify_extended_description(&ss, a()->dirs()[a()->current_user_dir().subnum].name);
            if (!ss.empty()) {
              area->DeleteExtendedDescription(f.filename());
              area->AddExtendedDescription(f.filename(), ss);
            }
          }
        } else {
          modify_extended_description(&ss, a()->dirs()[a()->current_user_dir().subnum].name);
          if (!ss.empty()) {
            area->AddExtendedDescription(f.filename(), ss);
            f.set_mask(mask_extended, true);
          } else {
            f.set_mask(mask_extended, false);
          }
        }
      } else if (!ss.empty()) {
        f.set_mask(mask_extended, true);
      } else {
        f.set_mask(mask_extended, false);
      }
    } break;

    case '\r':
      bout.nl();
      done = true;
    }
  }

  bout.nl(3);

  File file(FilePath(d.path, files::unalign(aligned_file_name)));
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    // dos error, file not found
    if (f.mask(mask_extended)) {
      a()->current_file_area()->DeleteExtendedDescription(f.filename());
    }
    t2u_error(file_name, "DOS error - File not found.");
    return 1;
  }
  if (!a()->upload_cmd.empty()) {
    file.Close();
    bout << "Please wait...\r\n";
    if (!check_ul_event(dn, &f.u())) {
      if (f.mask(mask_extended)) {
        a()->current_file_area()->DeleteExtendedDescription(f.filename());
      }
      t2u_error(file_name, "Failed upload event");
      return 1;
    }
    file.Open(File::modeBinary | File::modeReadOnly);
  }
  f.set_numbytes(file.length());
  file.Close();
  a()->user()->increment_uploaded();

  f.set_date(DateTime::now());
  auto* area = a()->current_file_area();
  if (!area->AddFile(f)) {
    LOG(ERROR) << "Error adding file: " << f;
  } else {
    area->Save();
  }
  add_to_file_database(f);

  a()->user()->set_uk(a()->user()->uk() + bytes_to_k(f.numbytes()));

  a()->status_manager()->Run([&](Status& status) {
    status.increment_uploads_today();
    status.increment_filechanged(Status::file_change_upload);
  });
  sysoplog() << fmt::format("+ \"{}\" uploaded on {}", f, a()->dirs()[dn].name);
  return 0;                                 // This means success
}

int try_to_ul(const string& file_name) {
  auto ac = false;

  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
    ac = true;
    tmp_disable_conf(true);
  }
  if (!try_to_ul_wh(file_name)) {
    if (ac) {
      tmp_disable_conf(false);
    }
    return 0;  // success
  }

  const auto dest_dir = FilePath(a()->config()->dloadsdir(), "TRY2UL");
  File::mkdirs(dest_dir);

  bout << "|#2Your file had problems, it is being moved to a special dir for sysop review\r\n";

  sysoplog() << fmt::format("Failed to upload {}, moving to TRY2UL dir", file_name);

  const auto src = FilePath(a()->sess().dirs().batch_directory(), file_name);
  const auto dest = FilePath(FilePath(a()->config()->dloadsdir(), "TRY2UL"), file_name);

  if (File::Exists(dest)) {                        // this is iffy <sp?/who care I choose to
    File::Remove(dest);                           // remove duplicates in try2ul dir, so keep
  }
  // it clean and up to date
  File::Copy(src, dest); // copy file from batch dir,to try2ul dir */

  if (a()->sess().IsUserOnline()) {
    a()->UpdateTopScreen();
  }

  if (ac) {
    tmp_disable_conf(false);
  }

  return 1;                                 // return failure, removes ul k credits etc...
}
