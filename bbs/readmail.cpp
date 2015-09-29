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
#include <cstdint>
#include <memory>
#include <string>

#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "bbs/instmsg.h"
#include "bbs/input.h"
#include "bbs/wconstants.h"
#include "bbs/subxtr.h"
#include "bbs/printfile.h"
#include "bbs/uedit.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivassert.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::strings;

// Local Functions
bool same_email(tmpmailrec * tm, mailrec * m);
void purgemail(tmpmailrec * mloc, int mw, int *curmail, mailrec * m1, slrec * ss);
void resynch_email(tmpmailrec * mloc, int mw, int rec, mailrec * m, int del, unsigned short stat);
bool read_same_email(tmpmailrec * mloc, int mw, int rec, mailrec * m, int del, unsigned short stat);
void add_netsubscriber(int nSystemNumber);

// Implementation
bool same_email(tmpmailrec * tm, mailrec * m) {
  if (tm->fromsys != m->fromsys ||
      tm->fromuser != m->fromuser ||
      m->tosys != 0 ||
      m->touser != session()->usernum ||
      tm->daten != m->daten ||
      tm->index == -1 ||
      memcmp(&tm->msg, &m->msg, sizeof(messagerec)) != 0) {
    return false;
  }
  return true;
}

void purgemail(tmpmailrec * mloc, int mw, int *curmail, mailrec * m1, slrec * ss) {
  mailrec m;

  if ((m1->anony & anony_sender) && ((ss->ability & ability_read_email_anony) == 0)) {
    bout << "|#5Delete all mail to you from this user? ";
  } else {
    bout << "|#5Delete all mail to you from ";
    if (m1->fromsys) {
      bout << "#" << m1->fromuser << " @" << m1->fromsys << "? ";
    } else {
      if (m1->fromuser == 65535) {
        bout << "Networks? ";
      } else {
        bout << "#" << m1->fromuser << "? ";
      }
    }
  }
  if (yesno()) {
    unique_ptr<File> pFileEmail(OpenEmailFile(true));
    if (!pFileEmail->IsOpen()) {
      return;
    }
    for (int i = 0; i < mw; i++) {
      if (mloc[i].index >= 0) {
        pFileEmail->Seek(mloc[i].index * sizeof(mailrec), File::seekBegin);
        pFileEmail->Read(&m, sizeof(mailrec));
        if (same_email(mloc + i, &m)) {
          if (m.fromuser == m1->fromuser && m.fromsys == m1->fromsys) {
            bout << "Deleting mail msg #" << i + 1 << wwiv::endl;
            delmail(pFileEmail.get(), mloc[i].index);
            mloc[i].index = -1;
            if (*curmail == i) {
              ++(*curmail);
            }
          }
        }
      } else {
        if (*curmail == i) {
          ++(*curmail);
        }
      }
    }
    pFileEmail->Close();
  }
}

void resynch_email(tmpmailrec * mloc, int mw, int rec, mailrec * m, int del, unsigned short stat) {
  int i, i1;
  mailrec m1;

  unique_ptr<File> pFileEmail(OpenEmailFile(del || stat));
  if (pFileEmail->IsOpen()) {
    int mfl = pFileEmail->GetLength() / sizeof(mailrec);

    for (i = 0; i < mw; i++) {
      if (mloc[i].index >= 0) {
        mloc[i].index = -2;
      }
    }

    int mp = 0;

    for (i = 0; i < mfl; i++) {
      pFileEmail->Seek(i * sizeof(mailrec), File::seekBegin);
      pFileEmail->Read(&m1, sizeof(mailrec));

      if (m1.tosys == 0 && m1.touser == session()->usernum) {
        for (i1 = mp; i1 < mw; i1++) {
          if (same_email(mloc + i1, &m1)) {
            mloc[i1].index = static_cast< short >(i);
            mp = i1 + 1;
            if (i1 == rec) {
              *m = m1;
            }
            break;
          }
        }
      }
    }

    for (i = 0; i < mw; i++) {
      if (mloc[i].index == -2) {
        mloc[i].index = -1;
      }
    }

    if (stat && !del && (mloc[rec].index >= 0)) {
      m->status |= stat;
      pFileEmail->Seek(mloc[rec].index * sizeof(mailrec), File::seekBegin);
      pFileEmail->Write(m, sizeof(mailrec));
    }
    if (del && (mloc[rec].index >= 0)) {
      if (del == 2) {
        m->touser = 0;
        m->tosys = 0;
        m->daten = 0xffffffff;
        m->msg.storage_type = 0;
        m->msg.stored_as = 0xffffffff;
        pFileEmail->Seek(mloc[rec].index * sizeof(mailrec), File::seekBegin);
        pFileEmail->Write(m, sizeof(mailrec));
      } else {
        delmail(pFileEmail.get(), mloc[rec].index);
      }
      mloc[rec].index = -1;
    }
    pFileEmail->Close();
  } else {
    mloc[rec].index = -1;
  }
}

bool read_same_email(tmpmailrec * mloc, int mw, int rec, mailrec * m, int del, unsigned short stat) {
  if (mloc[rec].index < 0) {
    return false;
  }

  unique_ptr<File> pFileEmail(OpenEmailFile(del || stat));
  pFileEmail->Seek(mloc[rec].index * sizeof(mailrec), File::seekBegin);
  pFileEmail->Read(m, sizeof(mailrec));

  if (!same_email(mloc + rec, m)) {
    pFileEmail->Close();
    application()->GetStatusManager()->RefreshStatusCache();
    if (emchg) {
      resynch_email(mloc, mw, rec, m, del, stat);
    } else {
      mloc[rec].index = -1;
    }
  } else {
    if (stat && !del && (mloc[rec].index >= 0)) {
      m->status |= stat;
      pFileEmail->Seek(mloc[rec].index * sizeof(mailrec), File::seekBegin);
      pFileEmail->Write(m, sizeof(mailrec));
    }
    if (del) {
      if (del == 2) {
        m->touser = 0;
        m->tosys = 0;
        m->daten = 0xffffffff;
        m->msg.storage_type = 0;
        m->msg.stored_as = 0xffffffff;
        pFileEmail->Seek(mloc[rec].index * sizeof(mailrec), File::seekBegin);
        pFileEmail->Write(m, sizeof(mailrec));
      } else {
        delmail(pFileEmail.get(), mloc[rec].index);
      }
      mloc[rec].index = -1;
    }
    pFileEmail->Close();
  }
  return (mloc[rec].index != -1);
}

void add_netsubscriber(int nSystemNumber) {
  char s[10], s1[10];

  if (!valid_system(nSystemNumber)) {
    nSystemNumber = 0;
  }

  bout.nl();
  bout << "|#1Adding subscriber to subscriber list...\r\n\n";
  bout << "|#2SubType: ";
  input(s, 7, true);
  if (s[0] == 0) {
    return;
  }
  strcpy(s1, s);
  char szNetworkFileName[ MAX_PATH ];
  sprintf(szNetworkFileName, "%sn%s.net", session()->GetNetworkDataDirectory().c_str(), s);
  if (!File::Exists(szNetworkFileName)) {
    bout.nl();
    bout << "|#6Subscriber file not found: " << szNetworkFileName << wwiv::endl;
    return;
  }
  bout.nl();
  if (nSystemNumber) {
    bout << "Add @" << nSystemNumber << "." << session()->GetNetworkName() << " to subtype " << s << "? ";
  }
  if (!nSystemNumber || !noyes()) {
    bout << "|#2System Number: ";
    input(s, 5, true);
    if (!s[0]) {
      return;
    }
    nSystemNumber = atoi(s);
    if (!valid_system(nSystemNumber)) {
      bout << "@" << nSystemNumber << " is not a valid system in " << session()->GetNetworkName() <<
                         ".\r\n\n";
      return;
    }
  }
  sprintf(s, "%u\n", nSystemNumber);
  TextFile hostFile(szNetworkFileName, "a+t");
  if (hostFile.IsOpen()) {
    hostFile.Write(s);
    hostFile.Close();
    // TODO find replacement for autosend.exe
    if (File::Exists("autosend.exe")) {
      bout << "AutoSend starter messages? ";
      if (yesno()) {
        char szAutoSendCommand[ MAX_PATH ];
        sprintf(szAutoSendCommand, "AUTOSEND.EXE %s %u .%d", s1, nSystemNumber, session()->GetNetworkNumber());
        ExecuteExternalProgram(szAutoSendCommand, EFLAG_NONE);
      }
    }
  }
}

void delete_attachment(unsigned long daten, int forceit) {
  bool delfile;
  filestatusrec fsr;

  bool found = false;
  File fileAttach(syscfg.datadir, ATTACH_DAT);
  if (fileAttach.Open(File::modeBinary | File::modeReadWrite)) {
    long l = fileAttach.Read(&fsr, sizeof(fsr));
    while (l > 0 && !found) {
      if (daten == (unsigned long) fsr.id) {
        found = true;
        fsr.id  = 0;
        fileAttach.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::seekCurrent);
        fileAttach.Write(&fsr, sizeof(filestatusrec));
        delfile = true;
        if (!forceit) {
          if (so()) {
            bout << "|#5Delete attached file? ";
            delfile = yesno();
          }
        }
        if (delfile) {
          File::Remove(application()->GetAttachmentDirectory().c_str(), fsr.filename);
        } else {
          bout << "\r\nOrphaned attach " << fsr.filename << " remains in " <<
                             application()->GetAttachmentDirectory() << wwiv::endl;
          pausescr();
        }
      } else {
        l = fileAttach.Read(&fsr, sizeof(filestatusrec));
      }
    }
    fileAttach.Close();
  }
}

void readmail(int mode) {
  int i, i1, i2, i3, curmail = 0, tp, nn = 0, delme;
  bool done, okmail;
  unsigned short xx;
  char s[201], s1[205], s2[81], *ss2, mnu[81];
  mailrec m, m1;
  postrec p;
  char ch;
  long len, num_mail, num_mail1;
  int nUserNumber, nSystemNumber;
  net_system_list_rec *csne;
  filestatusrec fsr;
  bool attach_exists = false;
  bool found = false;
  long l1;

  emchg = false;

  bool next = false, abort = false;

  tmpmailrec *mloc = static_cast<tmpmailrec *>(BbsAllocA(MAXMAIL * sizeof(tmpmailrec)));
  if (!mloc) {
    bout << "|#6Not enough memory.\r\n";
    return;
  }
  write_inst(INST_LOC_RMAIL, 0, INST_FLAGS_NONE);
  slrec ss = getslrec(session()->GetEffectiveSl());
  int mw = 0;
  {
    unique_ptr<File> pFileEmail(OpenEmailFile(false));
    if (!pFileEmail->IsOpen()) {
      bout << "\r\n\nNo mail file exists!\r\n\n";
      free(mloc);
      return;
    }
    int mfl = pFileEmail->GetLength() / sizeof(mailrec);
    for (i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
      pFileEmail->Seek(i * sizeof(mailrec), File::seekBegin);
      pFileEmail->Read(&m, sizeof(mailrec));
      if ((m.tosys == 0) && (m.touser == session()->usernum)) {
        mloc[mw].index = static_cast< short >(i);
        mloc[mw].fromsys = m.fromsys;
        mloc[mw].fromuser = m.fromuser;
        mloc[mw].daten = m.daten;
        mloc[mw].msg = m.msg;
        mw++;
      }
    }
    pFileEmail->Close();
  }
  session()->user()->SetNumMailWaiting(mw);
  if (session()->usernum == 1) {
    fwaiting = mw;
  }
  if (mw == 0) {
    bout << "\r\n\n|#3You have no mail.\r\n\n";
    free(mloc);
    return;
  }
  if (mw == 1) {
    curmail = 0;
  } else {
    bout << "\r\n\n|#7You have mail from:\r\n\n";

    if ((session()->user()->GetScreenChars() >= 80) && session()->mail_who_field_len) {
      if (okansi()) {
        strcpy(s1, "\xC1\xC2\xC4");
      } else {
        strcpy(s1, "++-");
      }
      sprintf(s, "|#7%s%c", charstr(4, s1[2]), s1[1]);
      strcat(s, charstr(session()->mail_who_field_len - 4, s1[2]));
      strcat(s, charstr(1, s1[1]));
      strcat(s, charstr(session()->user()->GetScreenChars() - session()->mail_who_field_len - 3, s1[2]));
      pla(s, &abort);
    }
    for (i = 0; (i < mw && !abort); i++) {
      if (!read_same_email(mloc, mw, i, &m, 0, 0)) {
        continue;
      }
      if (mode == 1 && (m.status & status_seen)) {
        ++curmail;
        continue;
      }

      tp = 80;
      if (m.status & status_source_verified) {
        tp -= 2;
      }
      if (m.status & status_new_net) {
        tp -= 1;
        if (static_cast< int >(strlen(m.title)) <= tp) {
          nn = static_cast< unsigned char >(m.title[tp + 1]);
        } else {
          nn = 0;
        }
      } else {
        nn = 0;
      }
      sprintf(s, "|#2%3d%s|#7%c|#1 ", i + 1, m.status & status_seen ? " " : "|#3*", okansi() ? '\xB3' : '|');

      if ((m.anony & anony_sender) && ((ss.ability & ability_read_email_anony) == 0)) {
        strcat(s, ">UNKNOWN<");
      } else {
        if (m.fromsys == 0) {
          if (m.fromuser == 65535) {
            if (nn != 255) {
              strcat(s, net_networks[nn].name);
            }
          } else {
            WUser u;
            application()->users()->ReadUser(&u, m.fromuser);
            strcat(s, u.GetUserNameAndNumber(m.fromuser));
          }
        } else {
          set_net_num(nn);
          csne = next_system(m.fromsys);
          string system_name;
          if (csne) {
            system_name = csne->name;
          } else {
            system_name = "Unknown System";
          }
          if (nn == 255) {
            sprintf(s1, "#%u @%u.%s", m.fromuser, m.fromsys, "<deleted network>");
          } else {
            unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));
            if (b) {
              strncpy(s2, b.get(), sizeof(s2) - 1);
              ss2 = strtok(s2, "\r");
              if (m.fromsys == 32767) {
                sprintf(s1, "%s", stripcolors(ss2));
              } else {
                sprintf(s1, "%s %u@%u.%s (%s)",
                        stripcolors(strip_to_node(ss2).c_str()),
                        m.fromuser,
                        m.fromsys,
                        net_networks[nn].name,
                        system_name.c_str());
              }
              if (strlen(s1) > session()->mail_who_field_len) {
                s1[ session()->mail_who_field_len ] = '\0';
              }
            } else {
              if (session()->GetMaxNetworkNumber() > 1) {
                sprintf(s1, "#%u @%u.%s (%s)", m.fromuser, m.fromsys, net_networks[nn].name, system_name.c_str());
              } else {
                sprintf(s1, "#%u @%u (%s)", m.fromuser, m.fromsys, system_name.c_str());
              }
            }
          }
          strcat(s, s1);
        }
      }

      if (session()->user()->GetScreenChars() >= 80 && session()->mail_who_field_len) {
        if (strlen(stripcolors(s)) > session()->mail_who_field_len) {
          while (strlen(stripcolors(s)) > session()->mail_who_field_len) {
            s[ strlen(s) - 1 ] = '\0';
          }
        }
        strcat(s, charstr((session()->mail_who_field_len + 1) - strlen(stripcolors(s)), ' '));
        if (okansi()) {
          strcat(s, "|#7\xB3|#1");
        } else {
          strcat(s, "|");
        }
        strcat(s, " ");
        strcat(s, stripcolors(m.title));
        while (wwiv::strings::GetStringLength(stripcolors(s)) > session()->user()->GetScreenChars() - 1) {
          s[strlen(s) - 1] = '\0';
        }
      }
      pla(s, &abort);
      if ((i == (mw - 1)) && (session()->user()->GetScreenChars() >= 80) && (!abort)
          && (session()->mail_who_field_len)) {
        (okansi()) ? strcpy(s1, "\xC1\xC3\xC4") : strcpy(s1, "++-");
        sprintf(s, "|#7%s%c", charstr(4, s1[2]), s1[0]);
        strcat(s, charstr(session()->mail_who_field_len - 4, s1[2]));
        strcat(s, charstr(1, s1[0]));
        strcat(s, charstr(session()->user()->GetScreenChars() - session()->mail_who_field_len - 3, s1[2]));
        pla(s, &abort);
      }
    }
    bout.nl();
    bout << "|#9(|#2Q|#9=|#2Quit|#9, |#2Enter|#9=|#2First Message|#9) \r\n|#9Enter message number: ";
    input(s, 3, true);
    if (strchr(s, 'Q') != nullptr) {
      free(mloc);
      return;
    }
    i = atoi(s);
    if (i) {
      if (!mode) {
        if (i <= mw) {
          curmail = i - 1;
        } else {
          curmail = 0;
        }
      }
    }
  }
  done = false;

  do {
    abort = false;
    bout.nl(2);
    sprintf(s, "|#9Msg|#7:  [|#2%u|#7/|#2%d|#7] |#%dE-Mail\r\n", curmail + 1, mw, session()->GetMessageColor());
    osan(s, &abort, &next);
    sprintf(s, "|#9Subj|#7: ");
    osan(s, &abort, &next);
    next = false;
    bout.Color(session()->GetMessageColor());
    s[0] = '\0';

    if (!read_same_email(mloc, mw, curmail, &m, 0, 0)) {
      strcat(s, ">>> MAIL DELETED <<<");
      okmail = false;
      bout << s;
      bout.nl(3);
    } else {
      strcat(s, m.title);
      strcpy(irt, m.title);
      irt_name[0] = '\0';
      abort = false;
      i = ((ability_read_email_anony & ss.ability) != 0);
      okmail = true;
      pla(s, &abort);
      if (m.fromsys && !m.fromuser) {
        grab_user_name(&(m.msg), "email");
      } else {
        net_email_name[0] = '\0';
      }
      tp = 80;
      if (m.status & status_source_verified) {
        tp -= 2;
        if (static_cast<int>(strlen(m.title)) <= tp) {
          xx = *(short *)(m.title + tp + 1);
          strcpy(s, "-=> Source Verified Type ");
          sprintf(s + strlen(s), "%u", xx);
          if (xx == 1) {
            strcat(s, " (From NC)");
          } else if (xx > 256 && xx < 512) {
            strcpy(s2, " (From GC-");
            sprintf(s2 + strlen(s2), "%d)", xx - 256);
            strcat(s, s2);
          }
        } else {
          strcpy(s, "-=> Source Verified (unknown type)");
        }
        if (!abort) {
          bout.Color(4);
          pla(s, &abort);
        }
      }
      nn = 0;
      if (m.status & status_new_net) {
        tp -= 1;
        if (static_cast<int>(strlen(m.title)) <= tp) {
          nn = m.title[ tp + 1 ];
        }
      }
      set_net_num(nn);
      int nFromSystem = 0;
      int nFromUser = 0;
      if (nn != 255) {
        nFromSystem = m.fromsys;
        nFromUser = m.fromuser;
      }
      if (!abort) {
        read_message1(&m.msg, (m.anony & 0x0f), (i) ? true : false, &next, "email", nFromSystem, nFromUser);
        if (!(m.status & status_seen)) {
          read_same_email(mloc, mw, curmail, &m, 0, status_seen);
        }
      }
      found = false;
      attach_exists = false;
      if (m.status & status_file) {
        File fileAttach(syscfg.datadir, ATTACH_DAT);
        if (fileAttach.Open(File::modeBinary | File::modeReadOnly)) {
          l1 = fileAttach.Read(&fsr, sizeof(fsr));
          while (l1 > 0 && !found) {
            if (m.daten == static_cast<unsigned long>(fsr.id)) {
              found = true;
              sprintf(s, "%s%s", application()->GetAttachmentDirectory().c_str(), fsr.filename);
              if (File::Exists(s)) {
                bout << "'T' to download attached file \"" << fsr.filename << "\" (" << fsr.numbytes << " bytes).\r\n";
                attach_exists = true;
              } else {
                bout << "Attached file \"" << fsr.filename << "\" (" << fsr.numbytes << " bytes) is missing!\r\n";
              }
            }
            if (!found) {
              l1 = fileAttach.Read(&fsr, sizeof(fsr));
            }
          }
          if (!found) {
            bout << "File attached but attachment data missing.  Alert sysop!\r\n";
          }
        } else {
          bout << "File attached but attachment data missing.  Alert sysop!\r\n";
        }
        fileAttach.Close();
      }
    }

    do {
      write_inst(INST_LOC_RMAIL, 0, INST_FLAGS_NONE);
      set_net_num(nn);
      i1 = 1;
      irt_name[0] = '\0';
      if (!application()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
        strcpy(mnu, EMAIL_NOEXT);
        bout << "|#2Mail {?} : ";
      }
      if (so()) {
        strcpy(mnu, SY_EMAIL_NOEXT);
        if (application()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
          bout << "|#2Mail |#7{|#1QSRIDAF?-+GEZPVUOLCNY@|#7} |#2: ";
        }
        strcpy(s, "QSRIDAF?-+GEZPVUOLCNY@BM");
      } else {
        if (cs()) {
          strcpy(mnu, CS_EMAIL_NOEXT);
          if (application()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
            bout << "|#2Mail |#7{|#1QSRIDAF?-+GZPVUOCY@|#7} |#2: ";
          }
          strcpy(s, "QSRIDAF?-+GZPVUOCY@");
        } else {
          if (!okmail) {
            strcpy(mnu, RS_EMAIL_NOEXT);
            if (application()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
              bout << "|#2Mail |#7{|#1QI?-+GY|#7} |#2: ";
            }
            strcpy(s, "QI?-+G");
          } else {
            strcpy(mnu, EMAIL_NOEXT);
            if (application()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
              bout << "|#2Mail |#7{|#1QSRIDAF?+-GY@|#7} |#2: ";
            }
            strcpy(s, "QSRIDAF?-+GY@");
          }
        }
      }
      if ((m.status & status_file) && found && attach_exists) {
        if (application()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
          bout << "\b\b|#7{|#1T|#7} |#2: |#0";
        }
        strcat(s, "T");
      }
      ch = onek(s);
      if (okmail && !read_same_email(mloc, mw, curmail, &m, 0, 0)) {
        bout << "\r\nMail got deleted.\r\n\n";
        ch = 'R';
      }
      delme = 0;
      switch (ch) {
      case 'T':
        bout.nl();
        sprintf(s1, "%s%s", application()->GetAttachmentDirectory().c_str(), fsr.filename);
        bool sentt;
        bool abortt;
        send_file(s1, &sentt, &abortt, fsr.filename, -1, fsr.numbytes);
        if (sentt) {
          bout << "\r\nAttached file sent.\r\n";
          sysoplogf("Downloaded %ldk of attached file %s.", (fsr.numbytes + 1023) / 1024, fsr.filename);
        } else {
          bout << "\r\nAttached file not completely sent.\r\n";
          sysoplogf("Tried to download attached file %s.", fsr.filename);
        }
        bout.nl();
        break;
      case 'N':
        if (m.fromuser == 1) {
          add_netsubscriber(m.fromsys);
        } else {
          add_netsubscriber(0);
        }
        break;
      case 'E':
        if (so() && okmail) {
          unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));
          extract_out(b.get(), len, m.title, m.daten);
        }
        i1 = 0;
        break;
      case 'Q':
        done = true;
        break;
      case 'B':
        sysoplog("@ Ran Board Edit");
        boardedit();
        break;
      case 'O':
        if (cs() && okmail && m.fromuser != 65535 && nn != 255) {
          show_files("*.frm", syscfg.gfilesdir);
          bout << "|#2Which form letter: ";
          input(s, 8, true);
          if (!s[0]) {
            break;
          }
          sprintf(s1, "%s%s.frm", syscfg.gfilesdir, s);
          if (!File::Exists(s1)) {
            sprintf(s1, "%sform%s.msg", syscfg.gfilesdir, s);
          }
          if (File::Exists(s1)) {
            LoadFileIntoWorkspace(s1, true);
            num_mail =  static_cast<long>(session()->user()->GetNumFeedbackSent()) +
                        static_cast<long>(session()->user()->GetNumEmailSent()) +
                        static_cast<long>(session()->user()->GetNumNetEmailSent());
            grab_quotes(nullptr, nullptr);
            if (m.fromuser != 65535) {
              email(m.fromuser, m.fromsys, false, m.anony);
            }
            num_mail1 = static_cast<long>(session()->user()->GetNumFeedbackSent()) +
                        static_cast<long>(session()->user()->GetNumEmailSent()) +
                        static_cast<long>(session()->user()->GetNumNetEmailSent());
            if (num_mail != num_mail1) {
              if (m.fromsys != 0) {
                sprintf(s, "%s: %s", session()->GetNetworkName(),
                        session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum));
              } else {
                strcpy(s, session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum));
              }
              if (m.anony & anony_receiver) {
                strcpy(s, ">UNKNOWN<");
              }
              strcat(s, " read your mail on ");
              strcat(s, fulldate());
              if (!(m.status & status_source_verified)) {
                ssm(m.fromuser, m.fromsys, s);
              }
              read_same_email(mloc, mw, curmail, &m, 1, 0);
              ++curmail;
              if (curmail >= mw) {
                done = true;
              }
            } else {
              // need instance
              File::Remove(syscfgovr.tempdir, INPUT_MSG);
            }
          } else {
            bout << "\r\nFile not found.\r\n\n";
            i1 = 0;
          }
        }
        break;
      case 'G':
        bout << "|#2Go to which (1-" << mw << ") ? |#0";
        input(s, 3);
        i1 = atoi(s);
        if (i1 > 0 && i1 <= mw) {
          curmail = i1 - 1;
          i1 = 1;
        } else {
          i1 = 0;
        }
        break;
      case 'I':
      case '+':
        ++curmail;
        if (curmail >= mw) {
          done = true;
        }
        break;
      case '-':
        if (curmail) {
          --curmail;
        }
        break;
      case 'R':
        break;
      case '?':
        printfile(mnu);
        i1 = 0;
        break;
      case 'M':
        if (!okmail) {
          break;
        }
        if (so()) {
          if (!session()->IsUserOnline()) {
            session()->SetCurrentMessageArea(0);
            session()->SetCurrentReadMessageArea(0);
            session()->SetCurrentConferenceMessageArea(0);
          }
          tmp_disable_conf(true);
          bout.nl();
          char *ss1;
          do {
            bout << "|#2Move to which sub? ";
            ss1 = mmkey(0);
            if (ss1[0] == '?') {
              old_sublist();
            }
          } while ((!hangup) && (ss1[0] == '?'));
          i = -1;
          if ((ss1[0] == 0) || hangup) {
            i1 = 0;
            bout.nl();
            tmp_disable_conf(false);
            break;
          }
          for (i1 = 0; (i1 < session()->num_subs) && (usub[i1].subnum != -1); i1++) {
            if (wwiv::strings::IsEquals(usub[i1].keys, ss1)) {
              i = i1;
            }
          }
          if (i != -1) {
            if (session()->GetEffectiveSl() < subboards[usub[i].subnum].postsl) {
              bout << "\r\nSorry, you don't have post access on that sub.\r\n\n";
              i = -1;
            }
          }
          if (i != -1) {
            unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));

            strcpy(p.title, m.title);
            p.anony = m.anony;
            p.ownersys = m.fromsys;
            session()->SetNumMessagesInCurrentMessageArea(p.owneruser);
            p.owneruser = static_cast< unsigned short >(session()->usernum);
            p.msg = m.msg;
            p.daten = m.daten;
            p.status = 0;

            iscan(i);
            open_sub(true);
            if (xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
              p.status |= status_pending_net;
            }
            p.msg.storage_type = (uint8_t) subboards[session()->GetCurrentReadMessageArea()].storage_type;
            savefile(b.get(), len, &(p.msg), subboards[session()->GetCurrentReadMessageArea()].filename);
            WStatus* pStatus = application()->GetStatusManager()->BeginTransaction();
            p.qscan = pStatus->IncrementQScanPointer();
            application()->GetStatusManager()->CommitTransaction(pStatus);
            if (session()->GetNumMessagesInCurrentMessageArea() >=
                subboards[session()->GetCurrentReadMessageArea()].maxmsgs) {
              i1 = 1;
              i2 = 0;
              while (i2 == 0 && i1 <= session()->GetNumMessagesInCurrentMessageArea()) {
                if ((get_post(i1)->status & status_no_delete) == 0) {
                  i2 = i1;
                }
                ++i1;
              }
              if (i2 == 0) {
                i2 = 1;
              }
              delete_message(i2);
            }
            add_post(&p);
            pStatus = application()->GetStatusManager()->BeginTransaction();
            pStatus->IncrementNumMessagesPostedToday();
            pStatus->IncrementNumLocalPosts();
            application()->GetStatusManager()->CommitTransaction(pStatus);
            close_sub();
            tmp_disable_conf(false);
            iscan(session()->GetCurrentMessageArea());
            bout << "\r\n\n|#9Message moved.\r\n\n";
            int nTempNumMsgs = session()->GetNumMessagesInCurrentMessageArea();
            resynch(&nTempNumMsgs, &p);
            session()->SetNumMessagesInCurrentMessageArea(nTempNumMsgs);
          } else {
            tmp_disable_conf(false);
          }
        }
      case 'D': {
        string message;
        if (!okmail) {
          break;
        }
        bout << "|#5Delete this message? ";
        if (!noyes()) {
          break;
        }
        if (m.fromsys != 0) {
          message = session()->GetNetworkName();
          message += ": ";
          message += session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum);
        } else {
          message = session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum);
        }

        if (m.anony & anony_receiver) {
          message = ">UNKNOWN<";
        }

        message += " read your mail on ";
        message += fulldate();
        if (!(m.status & status_source_verified) && nn != 255) {
          ssm(m.fromuser, m.fromsys, message.c_str());
        }
      }
      case 'Z':
        if (!okmail) {
          break;
        }
        read_same_email(mloc, mw, curmail, &m, 1, 0);
        ++curmail;
        if (curmail >= mw) {
          done = true;
        }
        found = false;
        if (m.status & status_file) {
          delete_attachment(m.daten, 1);
        }
        break;
      case 'P':
        if (!okmail) {
          break;
        }
        purgemail(mloc, mw, &curmail, &m, &ss);
        if (curmail >= mw) {
          done = true;
        }
        break;
      case 'F':
        if (!okmail) {
          break;
        }
        if (m.status & status_multimail) {
          bout << "\r\nCan't forward multimail.\r\n\n";
          break;
        }
        bout.nl(2);
        if (okfsed() && session()->user()->IsUseAutoQuote()) {
          unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));
          auto_quote(b.get(), len, 4, m.daten);
          send_email();
          break;
        }

        bout << "|#2Forward to: ";
        input(s, 75);
        if (((i3 = strcspn(s, "@")) != (wwiv::strings::GetStringLength(s))) && (isalpha(s[i3 + 1]))) {
          if (strstr(s, "@32767") == nullptr) {
            strlwr(s1);
            strcat(s, " @32767");
          }
        }
        parse_email_info(s, &nUserNumber, &nSystemNumber);
        if (nUserNumber || nSystemNumber) {
          if (ForwardMessage(&nUserNumber, &nSystemNumber)) {
            bout << "Mail forwarded.\r\n";
          }
          if ((nUserNumber == session()->usernum) && (nSystemNumber == 0) && (!cs())) {
            bout << "Can't forward to yourself.\r\n";
            nUserNumber = 0;
          }
          if (nUserNumber || nSystemNumber) {
            if (nSystemNumber) {
              if (nSystemNumber == 1 && nUserNumber == 0 &&
                  wwiv::strings::IsEqualsIgnoreCase(session()->GetNetworkName(), "Internet")) {
                strcpy(s1, net_email_name);
              } else if (session()->GetMaxNetworkNumber() > 1) {
                if (nUserNumber) {
                  sprintf(s1, "#%d @%d.%s", nUserNumber, nSystemNumber, session()->GetNetworkName());
                } else {
                  sprintf(s1, "%s @%d.%s", net_email_name, nSystemNumber, session()->GetNetworkName());
                }
              } else {
                if (nUserNumber) {
                  sprintf(s1, "#%d @%d", nUserNumber, nSystemNumber);
                } else {
                  sprintf(s1, "%s @%d", net_email_name, nSystemNumber);
                }
              }
            } else {
              set_net_num(nn);
              WUser u;
              application()->users()->ReadUser(&u, nUserNumber);
              strcpy(s1, u.GetUserNameNumberAndSystem(nUserNumber, net_sysnum));
            }
            if (ok_to_mail(nUserNumber, nSystemNumber, false)) {
              bout << "|#5Forward to " << s1 << "? ";
              if (yesno()) {
                unique_ptr<File> pFileEmail(OpenEmailFile(true));
                if (!pFileEmail->IsOpen()) {
                  break;
                }
                pFileEmail->Seek(mloc[curmail].index * sizeof(mailrec), File::seekBegin);
                pFileEmail->Read(&m, sizeof(mailrec));
                if (!same_email(mloc + curmail, &m)) {
                  bout << "Error, mail moved.\r\n";
                  break;
                }
                bout << "|#5Delete this message? ";
                if (yesno()) {
                  if (m.status & status_file) {
                    delete_attachment(m.daten, 0);
                  }
                  delme = 1;
                  m1.touser = 0;
                  m1.tosys = 0;
                  m1.daten = 0xffffffff;
                  m1.msg.storage_type = 0;
                  m1.msg.stored_as = 0xffffffff;
                  pFileEmail->Seek(mloc[curmail].index * sizeof(mailrec), File::seekBegin);
                  pFileEmail->Write(&m1, sizeof(mailrec));
                } else {
                  unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));
                  savefile(b.get(), len, &(m.msg), "email");
                }
                m.status |= status_forwarded;
                m.status |= status_seen;
                pFileEmail->Close();

                i = session()->GetNetworkNumber();
                sprintf(s, "\r\nForwarded to %s from %s.", s1,
                        session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum));

                set_net_num(nn);
                lineadd(&m.msg, s, "email");
                sprintf(s, "%s %s %s", session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum),
                        "forwarded your mail to", s1);
                if (!(m.status & status_source_verified)) {
                  ssm(m.fromuser, m.fromsys, s);
                }
                set_net_num(i);
                sprintf(s, "Forwarded mail to %s", s1);
                if (delme) {
                  session()->user()->SetNumMailWaiting(session()->user()->GetNumMailWaiting() - 1);
                  if (session()->usernum == 1) {
                    --fwaiting;
                  }
                }
                bout << "Forwarding: ";
                if (nn != 255 && nn == session()->GetNetworkNumber()) {
                  sendout_email(m.title, &m.msg, m.anony,
                                nUserNumber, nSystemNumber,
                                1, m.fromuser,
                                m.fromsys ? m.fromsys :
                                net_networks[nn].sysnum, delme, nn);
                } else {
                  sendout_email(m.title, &m.msg, m.anony, nUserNumber, nSystemNumber, 1,
                                session()->usernum, net_sysnum, delme, session()->GetNetworkNumber());
                }
                ++curmail;
                if (curmail >= mw) {
                  done = true;
                }
                mailcheck = true;
              }
            }
          }
        }
        delme = 0;
        break;
      case 'A':
      case 'S':
      case '@':
        if (!okmail) {
          break;
        }
        num_mail = static_cast<long>(session()->user()->GetNumFeedbackSent()) +
                   static_cast<long>(session()->user()->GetNumEmailSent()) +
                   static_cast<long>(session()->user()->GetNumNetEmailSent());
        if (nn == 255) {
          bout << "|#6Deleted network.\r\n";
          i1 = 0;
          break;
        } else if (m.fromuser != 65535) {
          if (okfsed() && session()->user()->IsUseAutoQuote()) {
            unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));
            if (s[0] == '@') {
              auto_quote(b.get(), len, 1, m.daten);
            } else {
              auto_quote(b.get(), len, 2, m.daten);
            }
          }

          grab_quotes(&(m.msg), "email");
          if (ch == '@') {
            bout << "\r\n|#9Enter user name or number:\r\n:";
            input(s, 75, true);
            if (((i = strcspn(s, "@")) != wwiv::strings::GetStringLength(s))
                && isalpha(s[i + 1])) {
              if (strstr(s, "@32767") == nullptr) {
                strlwr(s);
                strcat(s, " @32767");
              }
            }
            parse_email_info(s, &nUserNumber, &nSystemNumber);
            if (nUserNumber || nSystemNumber) {
              email(nUserNumber, nSystemNumber, false, 0);
            }
          } else {
            email(m.fromuser, m.fromsys, false, m.anony);
          }
          grab_quotes(nullptr, nullptr);
        }
        num_mail1 = static_cast<long>(session()->user()->GetNumFeedbackSent()) +
                    static_cast<long>(session()->user()->GetNumEmailSent()) +
                    static_cast<long>(session()->user()->GetNumNetEmailSent());
        if (ch == 'A' || ch == '@') {
          if (num_mail != num_mail1) {
            string message;
            if (m.fromsys != 0) {
              message = session()->GetNetworkName();
              message += ": ";
              message += session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum);
            } else {
              message = session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum);
            }
            if (m.anony & anony_receiver) {
              message = ">UNKNOWN<";
            }
            message += " read your mail on ";
            message += fulldate();
            if (!(m.status & status_source_verified)) {
              ssm(m.fromuser, m.fromsys, message.c_str());
            }
            read_same_email(mloc, mw, curmail, &m, 1, 0);
            ++curmail;
            if (curmail >= mw) {
              done = true;
            }
            found = false;
            if (m.status & status_file) {
              delete_attachment(m.daten, 0);
            }
          } else {
            bout << "\r\nNo mail sent.\r\n\n";
            i1 = 0;
          }
        } else {
          if (num_mail != num_mail1) {
            if (!(m.status & status_replied)) {
              read_same_email(mloc, mw, curmail, &m, 0, status_replied);
            }
            ++curmail;
            if (curmail >= mw) {
              done = true;
            }
          }
        }
        break;
      case 'U':
      case 'V':
      case 'C':
        if (!okmail) {
          break;
        }
        if (m.fromsys == 0 && cs() && m.fromuser != 65535) {
          if (ch == 'V') {
            valuser(m.fromuser);
          } else if (ch == 'U') {
            uedit(m.fromuser, UEDIT_NONE);
          } else {
            uedit(m.fromuser, UEDIT_FULLINFO | UEDIT_CLEARSCREEN);
          }
        } else if (cs()) {
          bout << "\r\nMail from another system.\r\n\n";
        }
        i1 = 0;
        break;
      case 'L': {
        if (!so()) {
          break;
        }
        bout << "\r\n|#2Filename: ";
        string fileName;
        input(&fileName, 50);
        if (!fileName.empty()) {
          bout.nl();
          bout << "|#5Allow editing? ";
          if (yesno()) {
            bout.nl();
            LoadFileIntoWorkspace(fileName.c_str(), false);
          } else {
            bout.nl();
            LoadFileIntoWorkspace(fileName.c_str(),  true);
          }
        }
      }
      break;
      case 'Y':   // Add from here
        if (curmail >= 0) {
          string downloadFileName;
          unique_ptr<char[]> b(readfile(&(m.msg), "email", &len));
          bout << "E-mail download -\r\n\n|#2Filename: ";
          input(&downloadFileName, 12);
          if (!okfn(downloadFileName.c_str())) {
            break;
          }
          File fileTemp(syscfgovr.tempdir, downloadFileName.c_str());
          fileTemp.Delete();
          fileTemp.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
          fileTemp.Write(b.get(), len);
          fileTemp.Close();
          bool bSent;
          bool bAbort;
          send_file(fileTemp.full_pathname().c_str(), &bSent, &bAbort, fileTemp.full_pathname().c_str(), -1, len);
          if (bSent) {
            bout << "E-mail download successful.\r\n";
            sysoplog("Downloaded E-mail");
          } else {
            bout << "E-mail download aborted.\r\n";
          }
        }
        break;
      }
    } while (!i1 && !hangup);
  } while (!hangup && !done);

  free(mloc);
}

int check_new_mail(int nUserNumber) {
  int nNumNewMessages = 0;            // number of new mail

  unique_ptr<File> pFileEmail(OpenEmailFile(false));
  if (pFileEmail->Exists() && pFileEmail->IsOpen()) {
    int mfLength = pFileEmail->GetLength() / sizeof(mailrec);
    int mWaiting = 0;   // number of mail waiting
    for (int i = 0; (i < mfLength) && (mWaiting < MAXMAIL); i++) {
      mailrec m;
      pFileEmail->Seek(i * sizeof(mailrec), File::seekBegin);
      pFileEmail->Read(&m, sizeof(mailrec));
      if (m.tosys == 0 && m.touser == nUserNumber) {
        if (!(m.status & status_seen)) {
          nNumNewMessages++;
        }
      }
    }
    pFileEmail->Close();
  }
  return nNumNewMessages;
}
