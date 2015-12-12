/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include "bbs/wstatus.h"
#include "core/os.h"
#include "core/strings.h"

using std::chrono::milliseconds;
using std::string;

using namespace wwiv::os;
using namespace wwiv::strings;

int try_to_ul_wh(char *pszFileName);
void t2u_error(const char *pszFileName, const char *msg);


int try_to_ul(char *pszFileName) {
  bool ac = false;

  if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
    ac = true;
    tmp_disable_conf(true);
  }
  if (!try_to_ul_wh(pszFileName)) {
    if (ac) {
      tmp_disable_conf(false);
    }
    return 0;  // success
  }

  const string dest_dir = StringPrintf("%sTRY2UL", syscfg.dloadsdir);
  File::mkdirs(dest_dir);
  session()->CdHome();   // ensure we are in the correct directory

  bout << "|#2Your file had problems, it is being moved to a special dir for sysop review\r\n";

  sysoplogf("Failed to upload %s, moving to TRY2UL dir", pszFileName);

  const string src = StringPrintf("%s%s", syscfgovr.batchdir, pszFileName);
  const string dest = StringPrintf("%sTRY2UL%c%s", syscfg.dloadsdir, File::pathSeparatorChar, pszFileName);

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

int try_to_ul_wh(char *pszFileName) {
  directoryrec d;
  char s[101], s1[MAX_PATH], s2[MAX_PATH], *ss;
  int i, i1, i2, i3, i4, key, ok = 0, dn = 0;
  uploadsrec u, u1;

  unalign(pszFileName);
  StringTrim(pszFileName);

  strcpy(s, pszFileName);
  if (!okfn(pszFileName)) {
    t2u_error(pszFileName, "Bad filename");          // bad filename
    return 1;
  }
  bout.cls();
  bout.nl(3);

  bool done = false;
  if (session()->user()->IsRestrictionValidate() || session()->user()->IsRestrictionUpload() ||
      (syscfg.sysconfig & sysconfig_all_sysop)) {
    dn = (syscfg.newuploads < session()->num_dirs) ? syscfg.newuploads : 0;
  } else {
    char temp[10];

    // The hangup check is below so uploads get uploaded even on hangup
    done = false;
    while (!done) {
      if (hangup) {
        if (syscfg.newuploads < session()->num_dirs) {
          dn = syscfg.newuploads;
        } else {
          dn = 0;
        }
        done = true;
      } else {
        // The sleep_for used to be a wait_sec_or_hit( 1 )
        sleep_for(milliseconds(500));
        bout << "\r\nUpload " << pszFileName << " to which dir? <CR>=0 ?=List \r\n";
        input(temp, 5, true);
        StringTrim(temp);
        if (temp[0] == '?') {
          dirlist(1);
        } else if (!temp[0]) {
          dn = 0;
          done = true;
        } else {
          int x = atoi(temp);
          if (udir[x].subnum >= 0) {
            dliscan1(udir[x].subnum);
            d = directories[dn];
            if ((d.mask & mask_no_uploads) && (!dcs())) {
              bout << "Can't upload there...\r\n";
              pausescr();
            } else {
              dn = udir[x].subnum;
              done = true;
            }
          }
        }
      }
    }
  }

  dliscan1(dn);
  d = directories[dn];
  if (session()->numf >= d.maxfiles) {
    t2u_error(pszFileName, "This directory is currently full.");
    return 1;
  }
  if ((d.mask & mask_no_uploads) && (!dcs())) {
    t2u_error(pszFileName, "Uploads are not allowed to this directory.");
    return 1;
  }
  if (!is_uploadable(s)) {
    if (so()) {
      bout.nl();
      bout << "|#5In filename database - add anyway? ";
      if (!yesno()) {
        t2u_error(pszFileName, "|#6File either already here or unwanted.");
        return 1;
      }
    } else {
      t2u_error(pszFileName, "|#6File either already here or unwanted.");
      return 1;
    }
  }
  align(s);
  if (strchr(s, '?')) {
    t2u_error(pszFileName, "Contains wildcards");
    return 1;
  }
  if (d.mask & mask_archive) {
    ok = 0;
    s1[0] = '\0';
    for (i = 0; i < MAX_ARCS; i++) {
      if (arcs[i].extension[0] && arcs[i].extension[0] != ' ') {
        if (s1[0]) {
          strcat(s1, ", ");
        }
        strcat(s1, arcs[i].extension);
        if (wwiv::strings::IsEquals(s + 9, arcs[i].extension)) {
          ok = 1;
        }
      }
    }
    if (!ok) {
      bout.nl();
      bout << "Sorry, all uploads to this directory must be archived.  Supported types are:\r\n";
      bout << s1;
      bout.nl(2);

      t2u_error(pszFileName, "Unsupported archive");
      return 1;
    }
  }
  strcpy(u.filename, s);
  u.ownerusr = static_cast<unsigned short>(session()->usernum);
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  strncpy(u.upby, session()->user()->GetUserNameAndNumber(session()->usernum), sizeof(u.upby));
  u.upby[36]  = '\0';
  strcpy(u.date, date());

  sprintf(s1, "%s%s", d.path, s);
  if (File::Exists(s1)) {
    if (dcs()) {
      bout.nl(2);
      bout << "File already exists.\r\n|#5Add to database anyway? ";
      if (yesno() == 0) {
        t2u_error(pszFileName, "That file is already here.");
        return 1;
      }
    } else {
      t2u_error(pszFileName, "That file is already here.");
      return 1;
    }
  }
  if (ok && (!session()->HasConfigFlag(OP_FLAGS_FAST_SEARCH))) {
    bout.nl();
    bout << "Checking for same file in other directories...\r\n\n";
    i2 = 0;

    for (i = 0; (i < session()->num_dirs) && (udir[i].subnum != -1); i++) {
      strcpy(s1, "Scanning ");
      strcat(s1, directories[udir[i].subnum].name);

      for (i3 = i4 = strlen(s1); i3 < i2; i3++) {
        s1[i3] = ' ';
        s1[i3 + 1] = 0;
      }

      i2 = i4;
      bout << s1;
      bputch('\r');

      dliscan1(udir[i].subnum);
      i1 = recno(u.filename);
      if (i1 >= 0) {
        bout.nl();
        bout << "Same file found on " << directories[udir[i].subnum].name << wwiv::endl;

        if (dcs()) {
          bout.nl();
          bout << "|#5Upload anyway? ";
          if (!yesno()) {
            t2u_error(pszFileName, "That file is already here.");
            return 1;
          }
          bout.nl();
        } else {
          t2u_error(pszFileName, "That file is already here.");
          return 1;
        }
      }
    }


    for (i1 = 0; i1 < i2; i1++) {
      s1[i1] = ' ';
    }
    s1[i1] = '\0';
    bout << s1 << "\r";

    dliscan1(dn);
    bout.nl();
  }
  sprintf(s1, "%s%s", syscfgovr.batchdir, pszFileName);
  sprintf(s2, "%s%s", d.path, pszFileName);

  if (File::Exists(s2)) {
    File::Remove(s2);
  }

  // s1 and s2 should remain set,they are used below
  movefile(s1, s2, true);
  strcpy(u.description, "NO DESCRIPTION GIVEN");
  bool file_id_avail = get_file_idz(&u, dn);
  done = false;

  while (!done && !hangup && !file_id_avail) {
    bool abort = false;

    bout.cls();
    bout.nl();
    bout << "|#1Upload going to |#7" << d.name << "\r\n\n";
    bout << "   |#1Filename    |01: |#7" << pszFileName << wwiv::endl;
    bout << "|#2A|#7] |#1Description |01: |#7" << u.description << wwiv::endl;
    bout << "|#2B|#7] |#1Modify extended description\r\n\n";
    print_extended(u.filename, &abort, 10, 0);
    bout << "|#2<|#7CR|#2> |#1to continue, |#7Q|#1 to abort upload: ";
    key = onek("\rQABC", true);
    switch (key) {
    case 'Q':
      bout << "Are you sure, file will be lost? ";
      if (yesno()) {
        t2u_error(pszFileName, "Changed mind");
        // move file back to batch dir
        movefile(s2, s1, true);
        return 1;
      }
      break;

    case 'A':
      bout.nl();
      bout << "Please enter a one line description.\r\n:";
      inputl(u.description, 58);
      break;

    case 'B':
      bout.nl();
      ss = read_extended_description(u.filename);
      bout << "|#5Modify extended description? ";
      if (yesno()) {
        bout.nl();
        if (ss) {
          bout << "|#5Delete it? ";
          if (yesno()) {
            free(ss);
            delete_extended_description(u.filename);
            u.mask &= ~mask_extended;
          } else {
            u.mask |= mask_extended;
            modify_extended_description(&ss, directories[udir[session()->GetCurrentFileArea()].subnum].name);
            if (ss) {
              delete_extended_description(u.filename);
              add_extended_description(u.filename, ss);
              free(ss);
            }
          }
        } else {
          modify_extended_description(&ss, directories[udir[session()->GetCurrentFileArea()].subnum].name);
          if (ss) {
            add_extended_description(u.filename, ss);
            free(ss);
            u.mask |= mask_extended;
          } else {
            u.mask &= ~mask_extended;
          }
        }
      } else if (ss) {
        free(ss);
        u.mask |= mask_extended;
      } else {
        u.mask &= ~mask_extended;
      }
      break;

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
    t2u_error(pszFileName, "DOS error - File not found.");
    return 1;
  }
  if (!syscfg.upload_cmd.empty()) {
    file.Close();
    bout << "Please wait...\r\n";
    if (!check_ul_event(dn, &u)) {
      if (u.mask & mask_extended) {
        delete_extended_description(u.filename);
      }
      t2u_error(pszFileName, "Failed upload event");
      return 1;
    } else {
      file.Open(File::modeBinary | File::modeReadOnly);
    }
  }
  long lFileLength = file.GetLength();
  u.numbytes = lFileLength;
  file.Close();
  session()->user()->SetFilesUploaded(session()->user()->GetFilesUploaded() + 1);

  time_t tCurrentDate;
  time(&tCurrentDate);
  u.daten = static_cast<unsigned long>(tCurrentDate);
  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  for (i = session()->numf; i >= 1; i--) {
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
  u1.daten = static_cast<unsigned long>(tCurrentDate);
  FileAreaSetRecord(fileDownload, 0);
  fileDownload.Write(&u1, sizeof(uploadsrec));
  fileDownload.Close();

  modify_database(u.filename, true);

  session()->user()->SetUploadK(session()->user()->GetUploadK() + bytes_to_k(u.numbytes));

  WStatus *pStatus = session()->GetStatusManager()->BeginTransaction();
  pStatus->IncrementNumUploadsToday();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
  session()->GetStatusManager()->CommitTransaction(pStatus);
  sysoplogf("+ \"%s\" uploaded on %s", u.filename, directories[dn].name);
  return 0;                                 // This means success
}


void t2u_error(const char *pszFileName, const char *msg) {
  char szBuffer[ 255 ];

  bout.nl(2);
  sprintf(szBuffer, "**  %s failed T2U qualifications", pszFileName);
  bout << szBuffer;
  bout.nl();
  sysoplog(szBuffer);

  sprintf(szBuffer, "** Reason : %s", msg);
  bout << szBuffer;
  bout.nl();
  sysoplog(szBuffer);
}

