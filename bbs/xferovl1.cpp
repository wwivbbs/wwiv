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

#include <memory>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/wwiv.h"
#include "bbs/external_edit.h"
#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/xfer_common.h"
#include "core/strings.h"

// How far to indent extended descriptions
static const int INDENTION = 24;

// the archive type to use
static const int ARC_NUMBER = 0;

extern int foundany;
static const unsigned char *invalid_chars =
  (unsigned char *)"Ú¿ÀÙÄ³Ã´ÁÂÉ»È¼ÍºÌ¹ÊËÕ¸Ô¾Í³ÆµÏÑÖ·Ó½ÄºÇ¶ÐÒÅÎØ×°±²ÛßÜÝÞ";

using wwiv::bbs::TempDisablePause;
using namespace wwiv::strings;

void modify_extended_description(char **sss, const char *dest, const char *title) {
  char s[255], s1[255];
  int i, i2;

  bool ii = (*sss) ? true : false;
  int i4  = 0;
  do {
    if (ii) {
      bout.nl();
      if (okfsed() && application()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
        bout << "|#5Modify the extended description? ";
      } else {
        bout << "|#5Enter a new extended description? ";
      }
      if (!yesno()) {
        return;
      }
    } else {
      bout.nl();
      bout << "|#5Enter an extended description? ";
      if (!yesno()) {
        return;
      }
    }
    if (okfsed() && application()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
      sprintf(s, "%sEXTENDED.DSC", syscfgovr.tempdir);
      if (*sss) {
        File fileExtDesc(s);
        fileExtDesc.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        fileExtDesc.Write(*sss, strlen(*sss));
        fileExtDesc.Close();
        free(*sss);
        *sss = nullptr;
      } else {
        File::Remove(s);
      }

      const int saved_screen_chars = session()->user()->GetScreenChars();
      if (session()->user()->GetScreenChars() > (76 - INDENTION)) {
        session()->user()->SetScreenChars(76 - INDENTION);
      }

      bool bEditOK = external_text_edit("extended.dsc", syscfgovr.tempdir,
          session()->max_extend_lines, dest, MSGED_FLAG_NO_TAGLINE);
      session()->user()->SetScreenChars(saved_screen_chars);
      if (bEditOK) {
        if ((*sss = static_cast<char *>(BbsAllocA(10240))) == nullptr) {
          return;
        }
        File fileExtDesc(s);
        fileExtDesc.Open(File::modeBinary | File::modeReadWrite);
        fileExtDesc.Read(*sss, fileExtDesc.GetLength());
        (*sss)[ fileExtDesc.GetLength() ] = 0;
        fileExtDesc.Close();

        for (int i3 = strlen(*sss) - 1; i3 >= 0; i3--) {
          if ((*sss)[i3] == 1) {
            (*sss)[i3] = ' ';
          }
        }
      }
    } else {
      if (*sss) {
        free(*sss);
      }
      if ((*sss = static_cast<char *>(BbsAllocA(10240))) == nullptr) {
        return;
      }
      *sss[0] = 0;
      i = 1;
      bout.nl();
      bout << "Enter up to  " << session()->max_extend_lines << " lines, "
                         << 78 - INDENTION << " chars each.\r\n";
      bout.nl();
      s[0] = '\0';
      int nSavedScreenSize = session()->user()->GetScreenChars();
      if (session()->user()->GetScreenChars() > (76 - INDENTION)) {
        session()->user()->SetScreenChars(76 - INDENTION);
      }
      do {
        bout << "|#2" << i << ": |#0";
        s1[0] = '\0';
        bool bAllowPrevious = (i4 > 0) ? true : false;
        while (inli(s1, s, 90, true, bAllowPrevious, false, true)) {
          if (i > 1) {
            --i;
          }
          sprintf(s1, "%d:", i);
          bout << "|#2" << s1;
          i2 = 0;
          i4 -= 2;
          do {
            s[i2] = *(sss[0] + i4 - 1);
            ++i2;
            --i4;
          } while ((*(sss[0] + i4) != 10) && (i4 != 0));
          if (i4) {
            ++i4;
          }
          *(sss[0] + i4) = 0;
          s[i2] = 0;
          strrev(s);
          if (strlen(s) > static_cast<unsigned int>(session()->user()->GetScreenChars() - 1)) {
            s[ session()->user()->GetScreenChars() - 2 ] = '\0';
          }
        }
        i2 = strlen(s1);
        if (i2 && (s1[i2 - 1] == 1)) {
          s1[i2 - 1] = '\0';
        }
        if (s1[0]) {
          strcat(s1, "\r\n");
          strcat(*sss, s1);
          i4 += strlen(s1);
        }
      } while ((i++ < session()->max_extend_lines) && (s1[0]));
      session()->user()->SetScreenChars(nSavedScreenSize);
      if (*sss[0] == '\0') {
        free(*sss);
        *sss = nullptr;
      }
    }
    bout << "|#5Is this what you want? ";
    i = !yesno();
    if (i) {
      free(*sss);
      *sss = nullptr;
    }
  } while (i);
}


bool valid_desc(const char *pszDescription) {
  // I don't think this function is really doing what it should
  // be doing, but am not sure what it should be doing instead.
  size_t i = 0;

  do {
    if (pszDescription[i] > '@' && pszDescription[i] < '{') {
      return true;
    }
    i++;
  } while (i < strlen(pszDescription));
  return false;
}


bool get_file_idz(uploadsrec * u, int dn) {
  char *b, *ss, cmd[MAX_PATH], s[81];
  int i;
  bool ok = false;

  if (application()->HasConfigFlag(OP_FLAGS_READ_CD_IDZ) && (directories[dn].mask & mask_cdrom)) {
    return false;
  }
  sprintf(s, "%s%s", directories[dn].path, stripfn(u->filename));
  filedate(s, u->actualdate);
  ss = strchr(stripfn(u->filename), '.');
  if (ss == nullptr) {
    return false;
  }
  ++ss;
  for (i = 0; i < MAX_ARCS; i++) {
    if (!ok) {
      ok = wwiv::strings::IsEqualsIgnoreCase(ss, arcs[i].extension);
    }
  }
  if (!ok) {
    return false;
  }

  File::Remove(syscfgovr.tempdir, FILE_ID_DIZ);
  File::Remove(syscfgovr.tempdir, DESC_SDI);

  chdir(directories[dn].path);
  {
	  File file(File::current_directory(), stripfn(u->filename));
	  application()->CdHome();
	  get_arc_cmd(cmd, file.full_pathname().c_str(), 1, "FILE_ID.DIZ DESC.SDI");
  }
  chdir(syscfgovr.tempdir);
  ExecuteExternalProgram(cmd, EFLAG_NOHUP);
  application()->CdHome();
  sprintf(s, "%s%s", syscfgovr.tempdir, FILE_ID_DIZ);
  if (!File::Exists(s)) {
    sprintf(s, "%s%s", syscfgovr.tempdir, DESC_SDI);
  }
  if (File::Exists(s)) {
    bout.nl();
    bout << "|#9Reading in |#2" << stripfn(s) << "|#9 as extended description...";
    ss = read_extended_description(u->filename);
    if (ss) {
      free(ss);
      delete_extended_description(u->filename);
    }
    if ((b = static_cast<char *>(BbsAllocA(session()->max_extend_lines * 256 + 1))) == nullptr) {
      return false;
    }
    File file(s);
    file.Open(File::modeBinary | File::modeReadOnly);
    if (file.GetLength() < (session()->max_extend_lines * 256)) {
      long lFileLen = file.GetLength();
      file.Read(b, lFileLen);
      b[ lFileLen ] = 0;
    } else {
      file.Read(b, session()->max_extend_lines * 256);
      b[session()->max_extend_lines * 256] = 0;
    }
    file.Close();
    if (application()->HasConfigFlag(OP_FLAGS_IDZ_DESC)) {
      ss = strtok(b, "\n");
      if (ss) {
        for (i = 0; i < wwiv::strings::GetStringLength(ss); i++) {
          if ((strchr(reinterpret_cast<char*>(const_cast<unsigned char*>(invalid_chars)), ss[i]) != nullptr) && (ss[i] != CZ)) {
            ss[i] = '\x20';
          }
        }
        if (!valid_desc(ss)) {
          do {
            ss = strtok(nullptr, "\n");
          } while (!valid_desc(ss));
        }
      }
      if (ss[strlen(ss) - 1] == '\r') {
        ss[strlen(ss) - 1] = '\0';
      }
      sprintf(u->description, "%.55s", ss);
      ss = strtok(nullptr, "");
    } else {
      ss = b;
    }
    if (ss) {
      for (i = strlen(ss) - 1; i > 0; i--) {
        if (ss[i] == CZ || ss[i] == 12) {
          ss[i] = '\x20';
        }
      }
      add_extended_description(u->filename, ss);
      u->mask |= mask_extended;
    }
    free(b);
    bout << "Done!\r\n";
  }
  File::Remove(syscfgovr.tempdir, FILE_ID_DIZ);
  File::Remove(syscfgovr.tempdir, DESC_SDI);
  return true;
}


int read_idz_all() {
  int count = 0;

  tmp_disable_conf(true);
  TempDisablePause disable_pause;
  session()->localIO()->set_protect(0);
  for (int i = 0; (i < session()->num_dirs) && (udir[i].subnum != -1) &&
       (!session()->localIO()->LocalKeyPressed()); i++) {
    count += read_idz(0, i);
  }
  tmp_disable_conf(false);
  application()->UpdateTopScreen();
  return count;
}


int read_idz(int mode, int tempdir) {
  char s[81];
  int i, count = 0;
  bool abort = false;
  uploadsrec u;

  std::unique_ptr<TempDisablePause> disable_pause;
  if (mode) {
    disable_pause.reset(new TempDisablePause);
    session()->localIO()->set_protect(0);
    dliscan();
    file_mask(s);
  } else {
    sprintf(s, "*.*");
    align(s);
    dliscan1(udir[tempdir].subnum);
  }
  bout.bprintf("|#9Checking for external description files in |#2%-25.25s #%s...\r\n",
                                    directories[udir[tempdir].subnum].name,
                                    udir[tempdir].keys);
  File fileDownload(g_szDownloadFileName);
  fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  for (i = 1; (i <= session()->numf) && (!hangup) && !abort; i++) {
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    if ((compare(s, u.filename)) &&
        (strstr(u.filename, ".COM") == nullptr) &&
        (strstr(u.filename, ".EXE") == nullptr)) {
      chdir(directories[udir[tempdir].subnum].path);
      File file(File::current_directory(), stripfn(u.filename));
      application()->CdHome();
      if (file.Exists()) {
        if (get_file_idz(&u, udir[tempdir].subnum)) {
          count++;
        }
        FileAreaSetRecord(fileDownload, i);
        fileDownload.Write(&u, sizeof(uploadsrec));
      }
    }
    checka(&abort);
  }
  fileDownload.Close();
  if (mode) {
    application()->UpdateTopScreen();
  }
  return count;
}


void tag_it() {
  int i, i2, i3, i4;
  bool bad;
  double t = 0.0;
  char s[255], s1[255], s2[81], s3[400];
  long fs = 0;

  if (session()->numbatch >= session()->max_batch) {
    bout << "|#6No room left in batch queue.";
    getkey();
    return;
  }
  bout << "|#2Which file(s) (1-" << session()->tagptr << ", *=All, 0=Quit)? ";
  input(s3, 30, true);
  if (s3[0] == '*') {
    s3[0] = '\0';
    for (i2 = 0; i2 < session()->tagptr && i2 < 78; i2++) {
      sprintf(s2, "%d ", i2 + 1);
      strcat(s3, s2);
      if (strlen(s3) > sizeof(s3) - 10) {
        break;
      }
    }
    bout << "\r\n|#2Tagging: |#4" << s3 << wwiv::endl;
  }
  for (i2 = 0; i2 < wwiv::strings::GetStringLength(s3); i2++) {
    sprintf(s1, "%s", s3 + i2);
    i4 = 0;
    bad = false;
    for (i3 = 0; i3 < wwiv::strings::GetStringLength(s1); i3++) {
      if ((s1[i3] == ' ') || (s1[i3] == ',') || (s1[i3] == ';')) {
        s1[i3] = 0;
        i4 = 1;
      } else {
        if (i4 == 0) {
          i2++;
        }
      }
    }
    i = atoi(s1);
    if (i == 0) {
      break;
    }
    i--;
    if ((s1[0]) && (i >= 0) && (i < session()->tagptr)) {
      if (check_batch_queue(filelist[i].u.filename)) {
        bout << "|#6" << filelist[i].u.filename << " is already in the batch queue.\r\n";
        bad = true;
      }
      if (session()->numbatch >= session()->max_batch) {
        bout << "|#6Batch file limit of " << session()->max_batch << " has been reached.\r\n";
        bad = true;
      }
      if ((syscfg.req_ratio > 0.0001) && (ratio() < syscfg.req_ratio) &&
          !session()->user()->IsExemptRatio() && !bad) {
        bout.bprintf("|#2Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n",
                                          ratio(), syscfg.req_ratio);
        bad = true;
      }
      if (!bad) {
        sprintf(s, "%s%s", directories[filelist[i].directory].path,
                stripfn(filelist[i].u.filename));
        if (filelist[i].dir_mask & mask_cdrom) {
          sprintf(s2, "%s%s", directories[filelist[i].directory].path,
                  stripfn(filelist[i].u.filename));
          sprintf(s, "%s%s", syscfgovr.tempdir, stripfn(filelist[i].u.filename));
          if (!File::Exists(s)) {
            copyfile(s2, s, true);
          }
        }
        File fp(s);
        if (!fp.Open(File::modeBinary | File::modeReadOnly)) {
          bout << "|#6The file " << stripfn(filelist[i].u.filename) << " is not there.\r\n";
          bad = true;
        } else {
          fs = fp.GetLength();
          fp.Close();
        }
      }
      if (!bad) {
        t = 12.656 / static_cast<double>(modem_speed) * static_cast<double>(fs);
        if (nsl() <= (batchtime + t)) {
          bout << "|#6Not enough time left in queue for " << filelist[i].u.filename << ".\r\n";
          bad = true;
        }
      }
      if (!bad) {
        batchtime += static_cast<float>(t);
        strcpy(batch[session()->numbatch].filename, filelist[i].u.filename);
        batch[session()->numbatch].dir = filelist[i].directory;
        batch[session()->numbatch].time = (float) t;
        batch[session()->numbatch].sending = 1;
        batch[session()->numbatch].len = fs;
        session()->numbatch++;
        ++session()->numbatchdl;
        bout << "|#1" << filelist[i].u.filename << " added to batch queue.\r\n";
      }
    } else {
      bout << "|#6Bad file number " << i + 1 << wwiv::endl;
    }
    lines_listed = 0;
  }
}


void tag_files() {
  int i, i1, i2;
  char s[255], s1[255], s2[81], ch;
  bool had = false;
  double d;

  if ((lines_listed == 0) || (session()->tagging == 0) || (g_num_listed == 0)) {
    return;
  }
  bool abort = false;
  if (x_only || session()->tagging == 2) {
    session()->tagptr = 0;
    return;
  }
  session()->localIO()->tleft(true);
  if (hangup) {
    return;
  }
  if (session()->user()->IsUseNoTagging()) {
    if (session()->user()->HasPause()) {
      pausescr();
    }
    bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
    if (okansi()) {
      bout << "\r" <<
                         "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                         << wwiv::endl;
    } else {
      bout << "\r" << "------------+-----+-----------------------------------------------------------" <<
                         wwiv::endl;
    }
    session()->tagptr = 0;
    return;
  }
  lines_listed = 0;
  bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
  if (okansi()) {
    bout <<
                       "\r\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\r\n";
  } else {
    bout << "\r--+------------+-----+----+---------------------------------------------------\r\n";
  }

  bool done = false;
  while (!done && !hangup) {
    lines_listed = 0;
    ch = fancy_prompt("File Tagging", "CDEMNQRTV?");
    lines_listed = 0;
    switch (ch) {
    case '?':
      i = session()->tagging;
      session()->tagging = 0;
      printfile(TTAGGING_NOEXT);
      pausescr();
      session()->tagging = i;
      relist();
      break;
    case 'C':
    case SPACE:
    case RETURN:
      lines_listed = 0;
      session()->tagptr = 0;
      session()->titled = 2;
      bout.cls();
      done = true;
      break;
    case 'D':
      batchdl(1);
      session()->tagging = 0;
      if (!had) {
        bout.nl();
        pausescr();
        bout.cls();
      }
      done = true;
      break;
    case 'E':
      lines_listed = 0;
      i1 = session()->tagging;
      session()->tagging = 0;
      bout << "|#9Which file (1-" << session()->tagptr << ")? ";
      input(s, 2, true);
      i = atoi(s) - 1;
      if (s[0] && i >= 0 && i < session()->tagptr) {
        d = XFER_TIME(filelist[i].u.numbytes);
        bout.nl();
        for (i2 = 0; i2 < session()->num_dirs; i2++) {
          if (udir[i2].subnum == filelist[i].directory) {
            break;
          }
        }
        if (i2 < session()->num_dirs) {
          bout << "|#1Directory  : |#2#" << udir[i2].keys << ", " << directories[filelist[i].directory].name <<
                             wwiv::endl;
        } else {
          bout << "|#1Directory  : |#2#" << "??" << ", " << directories[filelist[i].directory].name << wwiv::endl;
        }
        bout << "|#1Filename   : |#2" << filelist[i].u.filename << wwiv::endl;
        bout << "|#1Description: |#2" << filelist[i].u.description << wwiv::endl;
        if (filelist[i].u.mask & mask_extended) {
          strcpy(s1, g_szExtDescrFileName);
          sprintf(g_szExtDescrFileName, "%s%s.ext", syscfg.datadir, directories[filelist[i].directory].filename);
          zap_ed_info();
          bout << "|#1Ext. Desc. : |#2";
          print_extended(filelist[i].u.filename, &abort, session()->max_extend_lines, 2);
          zap_ed_info();
          strcpy(g_szExtDescrFileName, s1);
        }
        bout << "|#1File size  : |#2" << bytes_to_k(filelist[i].u.numbytes) << wwiv::endl;
        bout << "|#1Apprx. time: |#2" << ctim(d) << wwiv::endl;
        bout << "|#1Uploaded on: |#2" << filelist[i].u.date << wwiv::endl;
        bout << "|#1Uploaded by: |#2" << filelist[i].u.upby << wwiv::endl;
        bout << "|#1Times D/L'd: |#2" << filelist[i].u.numdloads << wwiv::endl;
        if (directories[filelist[i].directory].mask & mask_cdrom) {
          bout.nl();
          bout << "|#3CD ROM DRIVE\r\n";
        } else {
          sprintf(s, "|#7%s%s", directories[filelist[i].directory].path, filelist[i].u.filename);
          if (!File::Exists(s)) {
            bout.nl();
            bout << "|#6-=>FILE NOT THERE<=-\r\n";
          }
        }
        bout.nl();
        pausescr();
        relist();

      }
      session()->tagging = i1;
      break;
    case 'N':
      session()->tagging = 2;
      done = true;
      break;
    case 'M':
      if (dcs()) {
        i = session()->tagging;
        session()->tagging = 0;
        move_file_t();
        session()->tagging = i;
        if (g_num_listed == 0) {
          done = true;
          return;
        }
        relist();
      }
      break;
    case 'Q':
      session()->tagging   = 0;
      session()->titled    = 0;
      session()->tagptr    = 0;
      lines_listed    = 0;
      done = true;
      return;
    case 'R':
      relist();
      break;
    case 'T':
      tag_it();
      break;
    case 'V':
      bout << "|#2Which file (1-|#2" << session()->tagptr << ")? ";
      input(s, 2, true);
      i = atoi(s) - 1;
      if ((s[0]) && (i >= 0) && (i < session()->tagptr)) {
        sprintf(s1, "%s%s", directories[filelist[i].directory].path,
                stripfn(filelist[i].u.filename));
        if (directories[filelist[i].directory].mask & mask_cdrom) {
          sprintf(s2, "%s%s", directories[filelist[i].directory].path,
                  stripfn(filelist[i].u.filename));
          sprintf(s1, "%s%s", syscfgovr.tempdir,
                  stripfn(filelist[i].u.filename));
          if (!File::Exists(s1)) {
            copyfile(s2, s1, true);
          }
        }
        if (!File::Exists(s1)) {
          bout << "|#6File not there.\r\n";
          pausescr();
          break;
        }
        get_arc_cmd(s, s1, 0, "");
        if (!okfn(stripfn(filelist[i].u.filename))) {
          s[0] = 0;
        }
        if (s[0] != 0) {
          bout.nl();
          session()->tagging = 0;
          ExecuteExternalProgram(s, application()->GetSpawnOptions(SPAWNOPT_ARCH_L));
          bout.nl();
          pausescr();
          session()->tagging = 1;
          application()->UpdateTopScreen();
          bout.cls();
          relist();
        } else {
          bout << "|#6Unknown archive.\r\n";
          pausescr();
          break;
        }
      }
      break;
    default:
      bout.cls();
      done = true;
      break;
    }
  }
  session()->tagptr = 0;
  lines_listed = 0;
}


int add_batch(char *pszDescription, const char *pszFileName, int dn, long fs) {
  char ch;
  char s1[81], s2[81];
  int i;

  if (find_batch_queue(pszFileName) > -1) {
    return 0;
  }

  double t = 0.0;
  if (modem_speed) {
    t = (12.656) / ((double)(modem_speed)) * ((double)(fs));
  }

  if (nsl() <= (batchtime + t)) {
    bout << "|#6 Insufficient time remaining... press any key.";
    getkey();
  } else {
    if (dn == -1) {
      return 0;
    } else {
      for (i = 0; i < wwiv::strings::GetStringLength(pszDescription); i++) {
        if (pszDescription[i] == RETURN) {
          pszDescription[i] = SPACE;
        }
      }
      bout.backline();
      bout.bprintf(" |#6? |#1%s %3luK |#5%-43.43s |#7[|#2Y/N/Q|#7] |#0", pszFileName,
                                        bytes_to_k(fs), stripcolors(pszDescription));
      ch = onek_ncr("QYN\r");
      bout.backline();
      if (wwiv::UpperCase<char>(ch) == 'Y') {
        if (directories[dn].mask & mask_cdrom) {
          sprintf(s2, "%s%s", directories[dn].path, pszFileName);
          sprintf(s1, "%s%s", syscfgovr.tempdir, pszFileName);
          if (!File::Exists(s1)) {
            if (!copyfile(s2, s1, true)) {
              bout << "|#6 file unavailable... press any key.";
              getkey();
            }
            bout.backline();
            bout.clreol();
          }
        } else {
          sprintf(s2, "%s%s", directories[dn].path, pszFileName);
          StringRemoveWhitespace(s2);
          if ((!File::Exists(s2)) && (!so())) {
            bout << "\r";
            bout.clreol();
            bout << "|#6 file unavailable... press any key.";
            getkey();
            bout << "\r";
            bout.clreol();
            return 0;
          }
        }
        batchtime += static_cast<float>(t);
        strcpy(batch[session()->numbatch].filename, pszFileName);
        batch[session()->numbatch].dir = static_cast<short>(dn);
        batch[session()->numbatch].time = static_cast<float>(t);
        batch[session()->numbatch].sending = 1;
        batch[session()->numbatch].len = fs;
        bout << "\r";
        bout.bprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n",
                                          session()->numbatch + 1, batch[session()->numbatch].filename, batch[session()->numbatch].len,
                                          ctim(batch[session()->numbatch].time),
                                          directories[batch[session()->numbatch].dir].name);
        session()->numbatch++;
        ++session()->numbatchdl;
        bout << "\r";
        bout << "|#5    Continue search? ";
        ch = onek_ncr("YN\r");
        if (wwiv::UpperCase<char>(ch) == 'N') {
          return -3;
        } else {
          return 1;
        }
      } else if (ch == 'Q') {
        bout.backline();
        return -3;
      } else {
        bout.backline();
      }
    }
  }
  return 0;
}


int try_to_download(const char *pszFileMask, int dn) {
  int rtn;
  bool abort = false;
  bool ok = false;
  uploadsrec u;
  char s1[81], s3[81];

  dliscan1(dn);
  int i = recno(pszFileMask);
  if (i <= 0) {
    checka(&abort);
    return ((abort) ? -1 : 0);
  }
  ok = true;
  foundany = 1;
  do {
    session()->localIO()->tleft(true);
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();

    bool ok2 = false;
    if (strncmp(u.filename, "WWIV4", 5) == 0 && !application()->HasConfigFlag(OP_FLAGS_NO_EASY_DL)) {
      ok2 = true;
    }

    if (!ok2 && (!(u.mask & mask_no_ratio)) && (!ratio_ok())) {
      return -2;
    }

    write_inst(INST_LOC_DOWNLOAD, udir[session()->GetCurrentFileArea()].subnum, INST_FLAGS_ONLINE);
    sprintf(s1, "%s%s", directories[dn].path, u.filename);
    sprintf(s3, "%-40.40s", u.description);
    abort = false;
    rtn = add_batch(s3, u.filename, dn, u.numbytes);
    s3[0] = 0;

    if (abort || rtn == -3) {
      ok = false;
    } else {
      i = nrecno(pszFileMask, i);
    }
  } while (i > 0 && ok && !hangup);
  returning = true;
  if (rtn == -2) {
    return -2;
  }
  if (abort || rtn == -3) {
    return -1;
  } else {
    return 1;
  }
}


void download() {
  char ch, s[81], s1[81];
  int i = 0, color = 0, count;
  bool ok = true;
  int dn, ip, rtn = 0, useconf;
  bool done = false;

  returning = false;
  useconf = 0;

  bout.cls();
  bout.litebar("%s Batch Downloads", syscfg.systemname);
  bout.nl();
  do {
    if (!i) {
      bout << "|#2Enter files, one per line, wildcards okay.  [Space] aborts a search.\r\n";
      bout.nl();
      bout << "|#1 #  File Name    Size    Time      Directory\r\n";
      bout <<
                         "|#7\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\r\n";
    }
    if (i < session()->numbatch) {
      if (!returning && batch[i].sending) {
        bout.bprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n", i + 1, batch[i].filename,
                                          batch[i].len, ctim(batch[i].time), directories[batch[i].dir].name);
      }
    } else {
      do {
        count = 0;
        ok = true;
        bout.backline();
        bout.bprintf("|#2%3d ", session()->numbatch + 1);
        bout.Color(1);
        bool onl = newline;
        newline = false;
        input(s, 12);
        newline = onl;
        if ((s[0]) && (s[0] != ' ')) {
          if (strchr(s, '.') == nullptr) {
            strcat(s, ".*");
          }
          align(s);
          rtn = try_to_download(s, udir[session()->GetCurrentFileArea()].subnum);
          if (rtn == 0) {
            if (uconfdir[1].confnum != -10 && okconf(session()->user())) {
              bout.backline();
              bout << " |#5Search all conferences? ";
              ch = onek_ncr("YN\r");
              if (ch == '\r' || wwiv::UpperCase<char>(ch) == 'Y') {
                tmp_disable_conf(true);
                useconf = 1;
              }
            }
            bout.backline();
            sprintf(s1, "%3d %s", session()->numbatch + 1, s);
            bout.Color(1);
            bout << s1;
            foundany = dn = 0;
            while ((dn < session()->num_dirs) && (udir[dn].subnum != -1)) {
              count++;
              if (!x_only) {
                bout << "|#" << color;
                if (count == NUM_DOTS) {
                  bout << "\r";
                  bout.Color(color);
                  bout << s1;
                  color++;
                  count = 0;
                  if (color == 4) {
                    color++;
                  }
                  if (color == 10) {
                    color = 0;
                  }
                }
              }
              rtn = try_to_download(s, udir[dn].subnum);
              if (rtn < 0) {
                break;
              } else {
                dn++;
              }
            }
            if (useconf) {
              tmp_disable_conf(false);
            }
            if (!foundany) {
              bout << "|#6 File not found... press any key.";
              getkey();
              bout.backline();
              ok = false;
            }
          }
        } else {
          bout.backline();
          done = true;
        }
      } while (!ok && !hangup);
    }
    i++;
    if (rtn == -2) {
      rtn = 0;
      i = 0;
    }
  } while (!done && !hangup && (i <= session()->numbatch));

  if (!session()->numbatchdl) {
    return;
  }

  bout.nl();
  if (!ratio_ok()) {
    bout << "\r\nSorry, your ratio is too low.\r\n\n";
    done = true;
    return;
  }
  bout.nl();
  bout << "|#1Files in Batch Queue   : |#2" << session()->numbatch << wwiv::endl;
  bout << "|#1Estimated Download Time: |#2" << ctim2(batchtime) << wwiv::endl;
  bout.nl();
  rtn = batchdl(3);
  if (rtn) {
    return;
  }
  bout.nl();
  if (!session()->numbatchdl) {
    return;
  }
  bout << "|#5Hang up after transfer? ";
  bool had = yesno();
  ip = get_protocol(xf_down_batch);
  if (ip > 0) {
    switch (ip) {
    case WWIV_INTERNAL_PROT_YMODEM: {
      if (over_intern && (over_intern[2].othr & othr_override_internal) &&
          (over_intern[2].sendbatchfn[0])) {
        dszbatchdl(had, over_intern[2].sendbatchfn, prot_name(WWIV_INTERNAL_PROT_YMODEM));
      } else {
        ymbatchdl(had);
      }
    }
    break;
    case WWIV_INTERNAL_PROT_ZMODEM: {
      zmbatchdl(had);
    }
    default: {
      dszbatchdl(had, externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn,
                 externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].description);
    }
    }
    if (!had) {
      bout.nl();
      bout.bprintf("Your ratio is now: %-6.3f\r\n", ratio());
    }
  }
}


char fancy_prompt(const char *pszPrompt, const char *pszAcceptChars) {
  char s1[81], s2[81], s3[81];
  char ch = 0;

  session()->localIO()->tleft(true);
  sprintf(s1, "\r|#2%s (|#1%s|#2)? |#0", pszPrompt, pszAcceptChars);
  sprintf(s2, "%s (%s)? ", pszPrompt, pszAcceptChars);
  int i1 = strlen(s2);
  sprintf(s3, "%s%s", pszAcceptChars, " \r");
  session()->localIO()->tleft(true);
  if (okansi()) {
    bout << s1;
    ch = onek_ncr(s3);
    bout << "\x1b[" << i1 << "D";
    for (int i = 0; i < i1; i++) {
      bputch(' ');
    }
    bout << "\x1b[" << i1 << "D";
  } else {
    bout << s2;
    ch = onek_ncr(s3);
    for (int i = 0; i < i1; i++) {
      bout.bs();
    }
  }
  return ch;
}


void endlist(int mode) {
  // if mode == 1, list files
  // if mode == 2, new files
  if (session()->tagging != 0) {
    if (g_num_listed) {
      if (session()->tagging == 1 && !session()->user()->IsUseNoTagging() && filelist) {
        tag_files();
        return;
      } else {
        bout.Color(session()->user()->IsUseExtraColor() ? FRAME_COLOR : 0);
        if (session()->titled != 2 && session()->tagging == 1 && !session()->user()->IsUseNoTagging()) {
          if (okansi()) {
            bout <<
                               "\r\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\r\n";
          } else {
            bout << "\r--+------------+-----+----+---------------------------------------------------\r\n";
          }
        } else {
          if (okansi()) {
            bout <<
                               "\r\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCA\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\r\n";
          } else {
            bout << "\r------------+-----+-----------------------------------------------------------\r\n";
          }
        }
      }
      bout << "\r|#9Files listed: |#2 " << g_num_listed;
    } else {
      bout << ((mode == 1) ? "\r|#3No matching files found.\r\n\n" : "\r|#1No new files found.\r\n\n");
    }
  }
}


void SetNewFileScanDate() {
  char ag[10];
  bool ok = true;

  bout.nl();
  struct tm *pTm = localtime(&nscandate);

  bout.bprintf("|#9Current limiting date: |#2%02d/%02d/%02d\r\n", pTm->tm_mon + 1, pTm->tm_mday,
                                    (pTm->tm_year % 100));
  bout.nl();
  bout << "|#9Enter new limiting date in the following format: \r\n";
  bout << "|#1 MM/DD/YY\r\n|#7:";
  bout.mpl(8);
  int i = 0;
  char ch = 0;
  do {
    if (i == 2 || i == 5) {
      ag[i++] = '/';
      bputch('/');
    } else {
      switch (i) {
      case 0:
        ch = onek_ncr("01\r");
        break;
      case 3:
        ch = onek_ncr("0123\b");
        break;
      case 8:
        ch = onek_ncr("\b\r");
        break;
      default:
        ch = onek_ncr("0123456789\b");
        break;
      }
      if (hangup) {
        ok = false;
        ag[0] = '\0';
        break;
      }
      switch (ch) {
      case '\r':
        switch (i) {
        case 0:
          ok = false;
          break;
        case 8:
          ag[8] = '\0';
          break;
        default:
          ch = '\0';
          break;
        }
        break;
      case BACKSPACE:
        bout << " \b";
        --i;
        if (i == 2 || i == 5) {
          bout.bs();
          --i;
        }
        break;
      default:
        ag[i++] = ch;
        break;
      }
    }
  } while (ch != '\r' && !hangup);

  bout.nl();
  if (ok) {
    int m = atoi(ag);
    int dd = atoi(&(ag[3]));
    int y = atoi(&(ag[6])) + 1900;
    if (y < 1920) {
      y += 100;
    }
    struct tm newTime;
    if ((((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (dd >= 31)) ||
        ((m == 2) && (((y % 4 != 0) && (dd == 29)) || (dd == 30))) ||
        (dd > 31) || ((m == 0) || (y == 0) || (dd == 0)) ||
        ((m > 12) || (dd > 31))) {
      bout.nl();
      bout.bprintf("|#6%02d/%02d/%02d is invalid... date not changed!\r\n", m, dd, (y % 100));
      bout.nl();
    } else {
      // Rushfan - Note, this needs a better fix, this whole routine should be replaced.
      newTime.tm_min  = 0;
      newTime.tm_hour = 1;
      newTime.tm_sec  = 0;
      newTime.tm_year = y - 1900;
      newTime.tm_mday = dd;
      newTime.tm_mon  = m - 1;
    }
    bout.nl();
    nscandate = mktime(&newTime);

    // Display the new nscan date
    struct tm *pNewTime = localtime(&nscandate);
    bout.bprintf("|#9New Limiting Date: |#2%02d/%02d/%04d\r\n", pNewTime->tm_mon + 1,
                                      pNewTime->tm_mday, (pNewTime->tm_year + 1900));

    // Hack to make sure the date covers everythig since we had to increment the hour by one
    // to show the right date on some versions of MSVC
    nscandate -= SECONDS_PER_HOUR;
  } else {
    bout.nl();
  }
}


void removefilesnotthere(int dn, int *autodel) {
  char ch = '\0';
  uploadsrec u;

  dliscan1(dn);
  char szAllFilesFileMask[MAX_PATH];
  strcpy(szAllFilesFileMask, "*.*");
  align(szAllFilesFileMask);
  int i = recno(szAllFilesFileMask);
  bool abort = false;
  while (!hangup && i > 0 && !abort) {
    char szCandidateFileName[ MAX_PATH ];
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    sprintf(szCandidateFileName, "%s%s", directories[dn].path, u.filename);
    StringRemoveWhitespace(szCandidateFileName);
    if (!File::Exists(szCandidateFileName)) {
      StringTrim(u.description);
      sprintf(szCandidateFileName, "|#2%s :|#1 %-40.40s", u.filename, u.description);
      if (!*autodel) {
        bout.backline();
        bout << szCandidateFileName;
        bout.nl();
        bout << "|#5Remove Entry (Yes/No/Quit/All) : ";
        ch = onek_ncr("QYNA");
      } else {
        bout.nl();
        bout << "|#1Removing entry " << szCandidateFileName;
        ch = 'Y';
      }
      if (ch == 'Y' || ch == 'A') {
        if (ch == 'A') {
          bout << "ll";
          *autodel = 1;
        }
        if (u.mask & mask_extended) {
          delete_extended_description(u.filename);
        }
        sysoplogf("-%s Removed from %s", u.filename, directories[dn].name);
        fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
        for (int i1 = i; i1 < session()->numf; i1++) {
          FileAreaSetRecord(fileDownload, i1 + 1);
          fileDownload.Read(&u, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i1);
          fileDownload.Write(&u, sizeof(uploadsrec));
        }
        --i;
        --session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u, sizeof(uploadsrec));
        u.numbytes = session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u, sizeof(uploadsrec));
        fileDownload.Close();
      } else if (ch == 'Q') {
        abort = true;
      }
    }
    i = nrecno(szAllFilesFileMask, i);
    bool next = true;
    checka(&abort, &next);
    if (!next) {
      i = 0;
    }
  }
}


void removenotthere() {
  if (!so()) {
    return;
  }

  tmp_disable_conf(true);
  TempDisablePause disable_pause;
  int autodel = 0;
  bout.nl();
  bout << "|#5Remove N/A files in all directories? ";
  if (yesno()) {
    for (int i = 0; ((i < session()->num_dirs) && (udir[i].subnum != -1) &&
                     (!session()->localIO()->LocalKeyPressed())); i++) {
      bout.nl();
      bout << "|#1Removing N/A|#0 in " << directories[udir[i].subnum].name;
      bout.nl(2);
      removefilesnotthere(udir[i].subnum, &autodel);
    }
  } else {
    bout.nl();
    bout << "Removing N/A|#0 in " << directories[udir[session()->GetCurrentFileArea()].subnum].name;
    bout.nl(2);
    removefilesnotthere(udir[session()->GetCurrentFileArea()].subnum, &autodel);
  }
  tmp_disable_conf(false);
  application()->UpdateTopScreen();
}


int find_batch_queue(const char *pszFileName) {
  for (int i = 0; i < session()->numbatch; i++) {
    if (wwiv::strings::IsEquals(pszFileName, batch[i].filename)) {
      return i;
    }
  }

  return -1;
}


// Removes a file off the batch queue specified by pszFileNam,e
void remove_batch(const char *pszFileName) {
  int batchNum = find_batch_queue(pszFileName);
  if (batchNum > -1) {
    delbatch(batchNum);
  }
}

