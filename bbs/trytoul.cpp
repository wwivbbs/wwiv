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

#include "wwiv.h"


int try_to_ul_wh(char *pszFileName);
void t2u_error(const char *pszFileName, const char *msg);


//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//


int try_to_ul(char *pszFileName) {
  bool ac = false;

  if (uconfsub[1].confnum != -1 && okconf(GetSession()->GetCurrentUser())) {
    ac = true;
    tmp_disable_conf(true);
  }
  if (!try_to_ul_wh(pszFileName)) {
    if (ac) {
      tmp_disable_conf(false);
    }
    return 0;  // success
  }

  char dest[MAX_PATH];
  sprintf(dest, "%sTRY2UL", syscfg.dloadsdir);
  WWIV_make_path(dest);
  GetApplication()->CdHome();   // ensure we are in the correct directory

  GetSession()->bout << "|#2Your file had problems, it is being moved to a special dir for sysop review\r\n";

  sysoplogf("Failed to upload %s, moving to TRY2UL dir", pszFileName);

  char src[MAX_PATH];
  sprintf(src, "%s%s", syscfgovr.batchdir, pszFileName);
  sprintf(dest, "%sTRY2UL%c%s", syscfg.dloadsdir, WWIV_FILE_SEPERATOR_CHAR, pszFileName);

  if (WFile::Exists(dest)) {                        // this is iffy <sp?/who care I chooose to
    WFile::Remove(dest);                           // remove duplicates in try2ul dir, so keep
  }
  // it clean and up to date
  copyfile(src, dest, true);                   // copy file from batch dir,to try2ul dir */

  if (GetSession()->IsUserOnline()) {
    GetApplication()->UpdateTopScreen();
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
  GetSession()->bout.ClearScreen();
  GetSession()->bout.NewLine(3);

  bool done = false;
  if (GetSession()->GetCurrentUser()->IsRestrictionValidate() || GetSession()->GetCurrentUser()->IsRestrictionUpload() ||
      (syscfg.sysconfig & sysconfig_all_sysop)) {
    dn = (syscfg.newuploads < GetSession()->num_dirs) ? syscfg.newuploads : 0;
  } else {
    char temp[10];

    // The hangup check is below so uploads get uploaded even on hangup
    done = false;
    while (!done) {
      if (hangup) {
        if (syscfg.newuploads < GetSession()->num_dirs) {
          dn = syscfg.newuploads;
        } else {
          dn = 0;
        }
        done = true;
      } else {
        // The WWIV_Delay used to be a wait_sec_or_hit( 1 )
        WWIV_Delay(500);
        GetSession()->bout << "\r\nUpload " << pszFileName << " to which dir? <CR>=0 ?=List \r\n";
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
              GetSession()->bout << "Can't upload there...\r\n";
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
  if (GetSession()->numf >= d.maxfiles) {
    t2u_error(pszFileName, "This directory is currently full.");
    return 1;
  }
  if ((d.mask & mask_no_uploads) && (!dcs())) {
    t2u_error(pszFileName, "Uploads are not allowed to this directory.");
    return 1;
  }
  if (!is_uploadable(s)) {
    if (so()) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#5In filename database - add anyway? ";
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
      GetSession()->bout.NewLine();
      GetSession()->bout << "Sorry, all uploads to this directory must be archived.  Supported types are:\r\n";
      GetSession()->bout << s1;
      GetSession()->bout.NewLine(2);

      t2u_error(pszFileName, "Unsupported archive");
      return 1;
    }
  }
  strcpy(u.filename, s);
  u.ownerusr = static_cast<unsigned short>(GetSession()->usernum);
  u.ownersys = 0;
  u.numdloads = 0;
  u.unused_filetype = 0;
  u.mask = 0;
  strncpy(u.upby, GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum), sizeof(u.upby));
  u.upby[36]  = '\0';
  strcpy(u.date, date());

  sprintf(s1, "%s%s", d.path, s);
  if (WFile::Exists(s1)) {
    if (dcs()) {
      GetSession()->bout.NewLine(2);
      GetSession()->bout << "File already exists.\r\n|#5Add to database anyway? ";
      if (yesno() == 0) {
        t2u_error(pszFileName, "That file is already here.");
        return 1;
      }
    } else {
      t2u_error(pszFileName, "That file is already here.");
      return 1;
    }
  }
  if (ok && (!GetApplication()->HasConfigFlag(OP_FLAGS_FAST_SEARCH))) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "Checking for same file in other directories...\r\n\n";
    i2 = 0;

    for (i = 0; (i < GetSession()->num_dirs) && (udir[i].subnum != -1); i++) {
      strcpy(s1, "Scanning ");
      strcat(s1, directories[udir[i].subnum].name);

      for (i3 = i4 = strlen(s1); i3 < i2; i3++) {
        s1[i3] = ' ';
        s1[i3 + 1] = 0;
      }

      i2 = i4;
      GetSession()->bout << s1;
      bputch('\r');

      dliscan1(udir[i].subnum);
      i1 = recno(u.filename);
      if (i1 >= 0) {
        GetSession()->bout.NewLine();
        GetSession()->bout << "Same file found on " << directories[udir[i].subnum].name << wwiv::endl;

        if (dcs()) {
          GetSession()->bout.NewLine();
          GetSession()->bout << "|#5Upload anyway? ";
          if (!yesno()) {
            t2u_error(pszFileName, "That file is already here.");
            return 1;
          }
          GetSession()->bout.NewLine();
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
    GetSession()->bout << s1 << "\r";

    dliscan1(dn);
    GetSession()->bout.NewLine();
  }
  sprintf(s1, "%s%s", syscfgovr.batchdir, pszFileName);
  sprintf(s2, "%s%s", d.path, pszFileName);

  if (WFile::Exists(s2)) {
    WFile::Remove(s2);
  }

  // s1 and s2 should remain set,they are used below
  movefile(s1, s2, true);
  strcpy(u.description, "NO DESCRIPTION GIVEN");
  bool file_id_avail = get_file_idz(&u, dn);
  done = false;

  while (!done && !hangup && !file_id_avail) {
    bool abort = false;

    GetSession()->bout.ClearScreen();
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#1Upload going to |#7" << d.name << "\r\n\n";
    GetSession()->bout << "   |#1Filename    |01: |#7" << pszFileName << wwiv::endl;
    GetSession()->bout << "|#2A|#7] |#1Description |01: |#7" << u.description << wwiv::endl;
    GetSession()->bout << "|#2B|#7] |#1Modify extended description\r\n\n";
    print_extended(u.filename, &abort, 10, 0);
    GetSession()->bout << "|#2<|#7CR|#2> |#1to continue, |#7Q|#1 to abort upload: ";
    key = onek("\rQABC", true);
    switch (key) {
    case 'Q':
      GetSession()->bout << "Are you sure, file will be lost? ";
      if (yesno()) {
        t2u_error(pszFileName, "Changed mind");
        // move file back to batch dir
        movefile(s2, s1, true);
        return 1;
      }
      break;

    case 'A':
      GetSession()->bout.NewLine();
      GetSession()->bout << "Please enter a one line description.\r\n:";
      inputl(u.description, 58);
      break;

    case 'B':
      GetSession()->bout.NewLine();
      ss = read_extended_description(u.filename);
      GetSession()->bout << "|#5Modify extended description? ";
      if (yesno()) {
        GetSession()->bout.NewLine();
        if (ss) {
          GetSession()->bout << "|#5Delete it? ";
          if (yesno()) {
            free(ss);
            delete_extended_description(u.filename);
            u.mask &= ~mask_extended;
          } else {
            u.mask |= mask_extended;
            modify_extended_description(&ss, directories[udir[GetSession()->GetCurrentFileArea()].subnum].name, u.filename);
            if (ss) {
              delete_extended_description(u.filename);
              add_extended_description(u.filename, ss);
              free(ss);
            }
          }
        } else {
          modify_extended_description(&ss, directories[udir[GetSession()->GetCurrentFileArea()].subnum].name, u.filename);
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
      GetSession()->bout.NewLine();
      done = true;
    }
  }

  GetSession()->bout.NewLine(3);

  WFile file(d.path, s);
  if (!file.Open(WFile::modeBinary | WFile::modeReadOnly)) {
    // dos error, file not found
    if (u.mask & mask_extended) {
      delete_extended_description(u.filename);
    }
    t2u_error(pszFileName, "DOS error - File not found.");
    return 1;
  }
  if (syscfg.upload_c[0]) {
    file.Close();
    GetSession()->bout << "Please wait...\r\n";
    if (!check_ul_event(dn, &u)) {
      if (u.mask & mask_extended) {
        delete_extended_description(u.filename);
      }
      t2u_error(pszFileName, "Failed upload event");
      return 1;
    } else {
      file.Open(WFile::modeBinary | WFile::modeReadOnly);
    }
  }
  long lFileLength = file.GetLength();
  u.numbytes = lFileLength;
  file.Close();
  GetSession()->GetCurrentUser()->SetFilesUploaded(GetSession()->GetCurrentUser()->GetFilesUploaded() + 1);

  time_t tCurrentDate;
  time(&tCurrentDate);
  u.daten = static_cast<unsigned long>(tCurrentDate);
  WFile fileDownload(g_szDownloadFileName);
  fileDownload.Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown,
                    WFile::permReadWrite);
  for (i = GetSession()->numf; i >= 1; i--) {
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u1, sizeof(uploadsrec));
    FileAreaSetRecord(fileDownload, i + 1);
    fileDownload.Write(&u1, sizeof(uploadsrec));
  }

  FileAreaSetRecord(fileDownload, 1);
  fileDownload.Write(&u, sizeof(uploadsrec));
  ++GetSession()->numf;
  FileAreaSetRecord(fileDownload, 0);
  fileDownload.Read(&u1, sizeof(uploadsrec));
  u1.numbytes = GetSession()->numf;
  u1.daten = static_cast<unsigned long>(tCurrentDate);
  GetSession()->m_DirectoryDateCache[dn] = static_cast<unsigned int>(tCurrentDate);
  FileAreaSetRecord(fileDownload, 0);
  fileDownload.Write(&u1, sizeof(uploadsrec));
  fileDownload.Close();

  modify_database(u.filename, true);

  GetSession()->GetCurrentUser()->SetUploadK(GetSession()->GetCurrentUser()->GetUploadK() + bytes_to_k(u.numbytes));

  WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
  pStatus->IncrementNumUploadsToday();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeUpload);
  GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
  sysoplogf("+ \"%s\" uploaded on %s", u.filename, directories[dn].name);
  return 0;                                 // This means success
}


void t2u_error(const char *pszFileName, const char *msg) {
  char szBuffer[ 255 ];

  GetSession()->bout.NewLine(2);
  sprintf(szBuffer, "**  %s failed T2U qualifications", pszFileName);
  GetSession()->bout << szBuffer;
  GetSession()->bout.NewLine();
  sysoplog(szBuffer);

  sprintf(szBuffer, "** Reason : %s", msg);
  GetSession()->bout << szBuffer;
  GetSession()->bout.NewLine();
  sysoplog(szBuffer);
}

