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
#include <chrono>

#include "bbs/batch.h"
#include "bbs/bbsovl3.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "sdk/status.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"

using std::chrono::milliseconds;
using std::string;

using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void t2u_error(const string& file_name, const string& msg) {
  bout.nl(2);
  string s1 = StrCat("**  ", file_name, " failed T2U qualifications");
  bout << s1 << wwiv::endl;
  sysoplog() << s1;

  string s2 = StrCat("** Reason : ", msg);
  bout << s2 << wwiv::endl;
  bout.nl();
  sysoplog() << s2;
}

static int try_to_ul_wh(const string& orig_file_name) {
  directoryrec d = {};
  int i1, i2, i4, key, ok = 0, dn = 0;
  uploadsrec u, u1;

  string file_name = orig_file_name;
  StringRemoveWhitespace(&file_name);

  if (!okfn(file_name)) {
    t2u_error(file_name, "Bad filename");          // bad filename
    return 1;
  }
  bout.cls();
  bout.nl(3);

  bool done = false;
  if (session()->user()->IsRestrictionValidate() || session()->user()->IsRestrictionUpload() ||
      (syscfg.sysconfig & sysconfig_all_sysop)) {
    dn = (syscfg.newuploads < session()->directories.size()) ? syscfg.newuploads : 0;
  } else {
    char temp[10];

    // The hangup check is below so uploads get uploaded even on hangup
    done = false;
    while (!done) {
      if (hangup) {
        if (syscfg.newuploads < session()->directories.size()) {
          dn = syscfg.newuploads;
        } else {
          dn = 0;
        }
        done = true;
      } else {
        // The sleep_for used to be a wait_sec_or_hit( 1 )
        sleep_for(milliseconds(500));
        bout << "\r\nUpload " << file_name << " to which dir? <CR>=0 ?=List \r\n";
        input(temp, 5, true);
        StringTrim(temp);
        if (temp[0] == '?') {
          dirlist(1);
        } else if (!temp[0]) {
          dn = 0;
          done = true;
        } else {
          int x = atoi(temp);
          if (session()->udir[x].subnum >= 0) {
            dliscan1(session()->udir[x].subnum);
            d = session()->directories[dn];
            if ((d.mask & mask_no_uploads) && (!dcs())) {
              bout << "Can't upload there...\r\n";
              pausescr();
            } else {
              dn = session()->udir[x].subnum;
              done = true;
            }
          }
        }
      }
    }
  }

  dliscan1(dn);
  d = session()->directories[dn];
  if (session()->numf >= d.maxfiles) {
    t2u_error(file_name, "This directory is currently full.");
    return 1;
  }
  if ((d.mask & mask_no_uploads) && (!dcs())) {
    t2u_error(file_name, "Uploads are not allowed to this directory.");
    return 1;
  }
  if (!is_uploadable(file_name.c_str())) {
    if (so()) {
      bout.nl();
      bout << "|#5In filename database - add anyway? ";
      if (!yesno()) {
        t2u_error(file_name, "|#6File either already here or unwanted.");
        return 1;
      }
    } else {
      t2u_error(file_name, "|#6File either already here or unwanted.");
      return 1;
    }
  }
  string s = file_name;
  align(&s);
  if (contains(s, '?')) {
    t2u_error(file_name, "Contains wildcards");
    return 1;
  }
  if (d.mask & mask_archive) {
    ok = 0;
    string s1;
    for (size_t i = 0; i < MAX_ARCS; i++) {
      if (session()->arcs[i].extension[0] && session()->arcs[i].extension[0] != ' ') {
        if (!s1.empty()) s1 += ", ";
        s1 += session()->arcs[i].extension;
        if (wwiv::strings::IsEquals(s.c_str() + 9, session()->arcs[i].extension)) {
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
  strcpy(u.filename, s.c_str());
  u.ownerusr = static_cast<uint16_t>(session()->usernum);
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  const string unn = session()->names()->UserName(session()->usernum);
  strncpy(u.upby, unn.c_str(), sizeof(u.upby));
  u.upby[36]  = '\0';
  strcpy(u.date, date());

  if (File::Exists(StrCat(d.path, s))) {
    if (dcs()) {
      bout.nl(2);
      bout << "File already exists.\r\n|#5Add to database anyway? ";
      if (yesno() == 0) {
        t2u_error(file_name, "That file is already here.");
        return 1;
      }
    } else {
      t2u_error(file_name, "That file is already here.");
      return 1;
    }
  }
  if (ok && (!session()->HasConfigFlag(OP_FLAGS_FAST_SEARCH))) {
    bout.nl();
    bout << "Checking for same file in other session()->directories...\r\n\n";
    i2 = 0;

    for (size_t i = 0; (i < session()->directories.size()) && (session()->udir[i].subnum != -1); i++) {
      string s1 = StrCat("Scanning ", session()->directories[session()->udir[i].subnum].name);

      i4 = s1.size();
      //s1 += string(i3 - i2, ' ');

      i2 = i4;
      bout << s1;
      bputch('\r');

      dliscan1(session()->udir[i].subnum);
      i1 = recno(u.filename);
      if (i1 >= 0) {
        bout.nl();
        bout << "Same file found on " << session()->directories[session()->udir[i].subnum].name << wwiv::endl;

        if (dcs()) {
          bout.nl();
          bout << "|#5Upload anyway? ";
          if (!yesno()) {
            t2u_error(file_name, "That file is already here.");
            return 1;
          }
          bout.nl();
        } else {
          t2u_error(file_name, "That file is already here.");
          return 1;
        }
      }
    }

    bout << string(i2, ' ') << "\r";

    dliscan1(dn);
    bout.nl();
  }
  const string src = StrCat(session()->batch_directory(), file_name);
  const string dest = StrCat(d.path, file_name);

  if (File::Exists(dest)) {
    File::Remove(dest);
  }

  // s1 and s2 should remain set,they are used below
  movefile(src, dest, true);
  strcpy(u.description, "NO DESCRIPTION GIVEN");
  bool file_id_avail = get_file_idz(&u, dn);
  done = false;

  while (!done && !hangup && !file_id_avail) {
    bool abort = false;

    bout.cls();
    bout.nl();
    bout << "|#1Upload going to |#7" << d.name << "\r\n\n";
    bout << "   |#1Filename    |01: |#7" << file_name << wwiv::endl;
    bout << "|#2A|#7] |#1Description |01: |#7" << u.description << wwiv::endl;
    bout << "|#2B|#7] |#1Modify extended description\r\n\n";
    print_extended(u.filename, &abort, 10, 0);
    bout << "|#2<|#7CR|#2> |#1to continue, |#7Q|#1 to abort upload: ";
    key = onek("\rQABC", true);
    switch (key) {
    case 'Q':
      bout << "Are you sure, file will be lost? ";
      if (yesno()) {
        t2u_error(file_name, "Changed mind");
        // move file back to batch dir
        movefile(dest, src, true);
        return 1;
      }
      break;

    case 'A':
      bout.nl();
      bout << "Please enter a one line description.\r\n:";
      inputl(u.description, 58);
      break;

    case 'B':
    {
      bout.nl();
      string ss = read_extended_description(u.filename);
      bout << "|#5Modify extended description? ";
      if (yesno()) {
        bout.nl();
        if (!ss.empty()) {
          bout << "|#5Delete it? ";
          if (yesno()) {
            delete_extended_description(u.filename);
            u.mask &= ~mask_extended;
          } else {
            u.mask |= mask_extended;
            modify_extended_description(&ss, session()->directories[session()->current_user_dir().subnum].name);
            if (!ss.empty()) {
              delete_extended_description(u.filename);
              add_extended_description(u.filename, ss);
            }
          }
        } else {
          modify_extended_description(&ss, session()->directories[session()->current_user_dir().subnum].name);
          if (!ss.empty()) {
            add_extended_description(u.filename, ss);
            u.mask |= mask_extended;
          } else {
            u.mask &= ~mask_extended;
          }
        }
      } else if (!ss.empty()) {
        u.mask |= mask_extended;
      } else {
        u.mask &= ~mask_extended;
      }
    } break;

    case '\r':
      bout.nl();
      done = true;
    }
  }

  bout.nl(3);

  File file(d.path, s);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    // dos error, file not found
    if (u.mask & mask_extended) {
      delete_extended_description(u.filename);
    }
    t2u_error(file_name, "DOS error - File not found.");
    return 1;
  }
  if (!syscfg.upload_cmd.empty()) {
    file.Close();
    bout << "Please wait...\r\n";
    if (!check_ul_event(dn, &u)) {
      if (u.mask & mask_extended) {
        delete_extended_description(u.filename);
      }
      t2u_error(file_name, "Failed upload event");
      return 1;
    } else {
      file.Open(File::modeBinary | File::modeReadOnly);
    }
  }
  long lFileLength = file.GetLength();
  u.numbytes = lFileLength;
  file.Close();
  session()->user()->SetFilesUploaded(session()->user()->GetFilesUploaded() + 1);

  time_t tCurrentDate = time(nullptr);
  u.daten = static_cast<uint32_t>(tCurrentDate);
  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  for (int i = session()->numf; i >= 1; i--) {
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u1, sizeof(uploadsrec));
    FileAreaSetRecord(fileDownload, i + 1);
    fileDownload.Write(&u1, sizeof(uploadsrec));
  }

  FileAreaSetRecord(fileDownload, 1);
  fileDownload.Write(&u, sizeof(uploadsrec));
  ++session()->numf;
  FileAreaSetRecord(fileDownload, 0);
  fileDownload.Read(&u1, sizeof(uploadsrec));
  u1.numbytes = session()->numf;
  u1.daten = static_cast<uint32_t>(tCurrentDate);
  FileAreaSetRecord(fileDownload, 0);
  fileDownload.Write(&u1, sizeof(uploadsrec));
  fileDownload.Close();

  modify_database(u.filename, true);

  session()->user()->SetUploadK(session()->user()->GetUploadK() + bytes_to_k(u.numbytes));

  WStatus *pStatus = session()->status_manager()->BeginTransaction();
  pStatus->IncrementNumUploadsToday();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
  session()->status_manager()->CommitTransaction(pStatus);
  sysoplog() << StringPrintf("+ \"%s\" uploaded on %s", u.filename, session()->directories[dn].name);
  return 0;                                 // This means success
}

int try_to_ul(const string& file_name) {
  bool ac = false;

  if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
    ac = true;
    tmp_disable_conf(true);
  }
  if (!try_to_ul_wh(file_name)) {
    if (ac) {
      tmp_disable_conf(false);
    }
    return 0;  // success
  }

  const string dest_dir = StringPrintf("%sTRY2UL", syscfg.dloadsdir);
  File::mkdirs(dest_dir);
  session()->CdHome();   // ensure we are in the correct directory

  bout << "|#2Your file had problems, it is being moved to a special dir for sysop review\r\n";

  sysoplog() << StringPrintf("Failed to upload %s, moving to TRY2UL dir", file_name.c_str());

  const string src = StrCat(session()->batch_directory(), file_name);
  const string dest = StringPrintf("%sTRY2UL%c%s", syscfg.dloadsdir, File::pathSeparatorChar, file_name.c_str());

  if (File::Exists(dest)) {                        // this is iffy <sp?/who care I chooose to
    File::Remove(dest);                           // remove duplicates in try2ul dir, so keep
  }
  // it clean and up to date
  copyfile(src, dest, true);                   // copy file from batch dir,to try2ul dir */

  if (session()->IsUserOnline()) {
    session()->UpdateTopScreen();
  }

  if (ac) {
    tmp_disable_conf(false);
  }

  return 1;                                 // return failure, removes ul k credits etc...
}
