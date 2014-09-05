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
#include <qwk.h>

#include <memory>
#include <ctype.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>

#include "bbs/archivers.h"
#include "bbs/bbs.h"
#include "bbs/subxtr.h"
#include "bbs/vardec.h"
#include "bbs/vars.h"
#include "bbs/wwiv.h"

#include "bbs/wconstants.h"
#include "bbs/wwivcolors.h"

using std::string;

#define SET_BLOCK(file, pos, size) lseek(file, (long)pos * (long)size, SEEK_SET)


#define qwk_iscan_literal(x) (iscan1(x, 1))

#define MAXMAIL 255
#define EMAIL_STORAGE 2

extern const char *QWKFrom;
extern int qwk_percent;
extern bool qwk_bi_mode;

// from inmsg.cpp
void AddLineToMessageBuffer(char *pszMessageBuffer, const char *pszLineToAdd, long *plBufferLength);

// from readmail.cp
int  read_same_email(tmpmailrec * mloc, int mw, int rec, mailrec * m, int del, unsigned short stat);

void qwk_remove_email(void) {
  emchg = false;

  tmpmailrec* mloc = (tmpmailrec *)malloc(MAXMAIL * sizeof(tmpmailrec));
  if (!mloc) {
    GetSession()->bout.Write("Not enough memory.");
    return;
  }

  std::unique_ptr<WFile> f(OpenEmailFile(true));
  
  if (!f->IsOpen()) {
    free(mloc);
    return;
  }

  int mfl = f->GetLength() / sizeof(mailrec);
  uint8_t mw = 0;

  mailrec m;
  for (long i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
    f->Seek(i * sizeof(mailrec), WFile::seekBegin);
    f->Read(&m, sizeof(mailrec));
    if ((m.tosys == 0) && (m.touser == GetSession()->usernum)) {
      mloc[mw].index = static_cast<short>(i);
      mloc[mw].fromsys = m.fromsys;
      mloc[mw].fromuser = m.fromuser;
      mloc[mw].daten = m.daten;
      mloc[mw].msg = m.msg;
      mw++;
    }
  }
  GetSession()->GetCurrentUser()->data.waiting = mw;

  if (GetSession()->usernum == 1) {
    fwaiting = mw;
  }
  
  if (mw == 0) {
    free(mloc);
    return;
  }

  int curmail = 0;
  bool done = false;
  do {
    delmail(f.get(), mloc[curmail].index);

    ++curmail;
    if (curmail >= mw) {
      done = true;
    }

  } while (!hangup && !done);
}


void qwk_gather_email(struct qwk_junk *qwk_info) {
  int i, mfl, curmail, done, tp, nn;
  char filename[201];
  mailrec m;
  postrec junk;

  emchg = false;
  tmpmailrec *mloc = (tmpmailrec *)malloc(MAXMAIL * sizeof(tmpmailrec));
  if (!mloc) {
    GetSession()->bout.Write("Not enough memory");
    return;
  }

  slrec ss = getslrec(GetSession()->GetEffectiveSl());
  std::unique_ptr<WFile> f(OpenEmailFile(false));
  if (!f->IsOpen()) {
    GetSession()->bout.NewLine(2);
    GetSession()->bout.Write("No mail file exists!");
    GetSession()->bout.NewLine();
    free(mloc);
    return;
  }
  mfl = f->GetLength() / sizeof(mailrec);
  uint8_t mw = 0;
  for (i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
    f->Seek(((long)(i)) * (sizeof(mailrec)), WFile::seekBegin);
    f->Read(&m, sizeof(mailrec));
    if ((m.tosys == 0) && (m.touser == GetSession()->usernum)) {
      mloc[mw].index = static_cast<short>(i);
      mloc[mw].fromsys = m.fromsys;
      mloc[mw].fromuser = m.fromuser;
      mloc[mw].daten = m.daten;
      mloc[mw].msg = m.msg;
      mw++;
    }
  }
  f->Close();
  GetSession()->GetCurrentUser()->data.waiting = mw;

  if (GetSession()->usernum == 1) {
    fwaiting = mw;
  }
  
  if (mw == 0) {
    GetSession()->bout.NewLine();
    GetSession()->bout.Write("You have no mail.");
    GetSession()->bout.NewLine();
    free(mloc);
    return;
  }

  GetSession()->bout.Color(7);
  GetSession()->bout.Write("Gathering Email");

  if (mw == 1) {
    curmail = 0;
  }

  curmail = 0;
  done = 0;
  
  qwk_info->in_email = 1;

  sprintf(filename, "%sPERSONAL.NDX", QWK_DIRECTORY);
  qwk_info->personal = open(filename, O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  sprintf(filename, "%s000.NDX", QWK_DIRECTORY);
  qwk_info->zero = open(filename, O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

  do {
    read_same_email(mloc, mw, curmail, &m, 0, 0);

    strupr(m.title);
    strncpy(qwk_info->email_title, m.title, 25);
    
    i = ((ability_read_email_anony & ss.ability) != 0);

    if ((m.fromsys) && (!m.fromuser)) {
      grab_user_name(&(m.msg), "email");
    } else {
      net_email_name[0] = 0;
    }
    tp = 80;

    if (m.status & status_new_net) {
      tp -= 1;
      if (strlen(m.title) <= tp) {
        nn = m.title[tp + 1];
      } else {
        nn = 0;
      }
    } else {
      nn = 0;
    }

    set_net_num(nn);

    // Hope this isn't killed in the future
    strcpy(junk.title, m.title);
    junk.anony = m.anony;
    junk.status = m.status;
    junk.ownersys = m.fromsys;
    junk.owneruser = m.fromuser;
    junk.daten = m.daten;
    junk.msg = m.msg;
    
    put_in_qwk(&junk, "email", curmail, qwk_info);
    
    ++curmail;
    if (curmail >= mw) {
      done = 1;
    }

  } while ((!hangup) && (!done));

  qwk_info->in_email = 0;
  free(mloc);
}

int select_qwk_archiver(struct qwk_junk *qwk_info, int ask) {
  int x;
  int archiver;
  char temp[101];
  char allowed[20];

  strcpy(allowed, "Q\r");

  GetSession()->bout.NewLine();
  GetSession()->bout.Write("Select an archiver");
  GetSession()->bout.NewLine();
  if (ask) {
    GetSession()->bout.Write("0) Ask me later");
  }
  for (x = 0; x < 4; ++x) {
    strcpy(temp, arcs[x].extension);
    StringTrim(temp);

    if (temp[0]) {
      sprintf(temp, "%d", x + 1);
      strcat(allowed, temp);
      GetSession()->bout.WriteFormatted("1%d) 3%s", x + 1, arcs[x].extension);
      GetSession()->bout.NewLine();
    }
  }
  GetSession()->bout.NewLine();
  GetSession()->bout.WriteFormatted("Enter #  Q to Quit <CR>=1 :");

  if (ask) {
    strcat(allowed, "0");
  }

  archiver = onek(allowed);

  if (archiver == '\r') {
    archiver = '1';
  }

  if (archiver == 'Q') {
    qwk_info->abort = 1;
    return 0;
  }
  archiver = archiver - '0';
  return (archiver);

}

void qwk_which_zip(char *thiszip) {
  if (GetSession()->GetCurrentUser()->data.qwk_archive > 4) {
    GetSession()->GetCurrentUser()->data.qwk_archive = 0;
  }

  if (arcs[GetSession()->GetCurrentUser()->data.qwk_archive - 1].extension[0] == 0) {
    GetSession()->GetCurrentUser()->data.qwk_archive = 0;
  }

  if (GetSession()->GetCurrentUser()->data.qwk_archive == 0) {
    strcpy(thiszip, "ASK");
  } else {
    strcpy(thiszip, arcs[GetSession()->GetCurrentUser()->data.qwk_archive - 1].extension);
  }
}

void qwk_which_protocol(char *thisprotocol) {
  if (GetSession()->GetCurrentUser()->data.qwk_protocol == 1) {
    GetSession()->GetCurrentUser()->data.qwk_protocol = 0;
  }

  if (GetSession()->GetCurrentUser()->data.qwk_protocol == 0) {
    strcpy(thisprotocol, "ASK");
  } else {
    strncpy(thisprotocol, prot_name(GetSession()->GetCurrentUser()->data.qwk_protocol), 22);
    thisprotocol[22] = 0;
  }
}
void upload_reply_packet(void) {
  char name[21], namepath[101];
  bool rec = true;
  int save_conf = 0, save_sub;
  struct qwk_config qwk_cfg;


  read_qwk_cfg(&qwk_cfg);

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = time(NULL);
  }

  ++qwk_cfg.timesu;
  write_qwk_cfg(&qwk_cfg);
  close_qwk_cfg(&qwk_cfg);

  save_sub = GetSession()->GetCurrentMessageArea();
  if ((uconfsub[1].confnum != -1) && (okconf(GetSession()->GetCurrentUser()))) {
    save_conf = 1;
    tmp_disable_conf(true);
  }

  qwk_system_name(name);
  strcat(name, ".REP");

  GetSession()->bout.WriteFormatted("Hit 'Y' to upload reply packet %s :", name);

  sprintf(namepath, "%s%s", QWK_DIRECTORY, name);
  
  bool do_it = true;
  if (!qwk_bi_mode) {
    do_it = yesno();
  }

  if (do_it) {
    if (!qwk_bi_mode && incom) {
      qwk_receive_file(namepath, &rec, GetSession()->GetCurrentUser()->data.qwk_protocol);
      WWIV_Delay(500);
    }

    if (rec) {
      qwk_system_name(name);
      strcat(name, ".MSG");

      ready_reply_packet(namepath, name);

      sprintf(namepath, "%s%s", QWK_DIRECTORY, name);
      process_reply_dat(namepath);
    } else {
      sysoplog("Aborted");
      GetSession()->bout.NewLine();
      GetSession()->bout.WriteFormatted("%s not found", name);
      GetSession()->bout.NewLine();
    }
  }
  if (save_conf) {
    tmp_disable_conf(false);
  }

  GetSession()->SetCurrentMessageArea(save_sub);
}

void ready_reply_packet(const char *packet_name, const char *msg_name) {
  int archiver = match_archiver(packet_name);
  string command = stuff_in(arcs[archiver].arce, packet_name, msg_name, "", "", "");

  chdir(QWK_DIRECTORY);
  ExecuteExternalProgram(command, EFLAG_NOPAUSE);
  GetApplication()->CdHome();
}

// Takes reply packet and converts '227' (ã) to '13'
void make_text_ready(char *text, long len) {
  int pos = 0;

  while (pos < len && !hangup) {
    if (text[pos] == '\xE3') {
      text[pos] = 13;
    }
    ++pos;
  }
}

char * make_text_file(int filenumber, long *size, int curpos, int blocks) {
  struct qwk_junk *qwk;
  char *temp;


  *size = 0;

  // This memory has to be freed later, after text is 'emailed' or 'posted'
  // Enough memory is allocated for all blocks, plus 2k extra for other
  // 'addline' stuff
  qwk = (struct qwk_junk *) malloc((blocks * sizeof(struct qwk_junk)) + 2048);
  if (!qwk) {
    return nullptr;
  }


  SET_BLOCK(filenumber, curpos, sizeof(struct qwk_record));

  read(filenumber, qwk, sizeof(struct qwk_record) * blocks);
  make_text_ready((char *)qwk, sizeof(struct qwk_record)*blocks);

  *size = sizeof(struct qwk_record) * blocks;

  // Remove trailing spaces
  temp = (char *)qwk;
  while (isspace(temp[*size - 1]) && *size && !hangup) {
    --*size;
  }

  return (temp);
}

void qwk_email_text(char *text, long size, char *title, char *to) {
  int sy, un;
  long thetime;

  strupr(to);

  // Remove text name from address, if it doesn't contain " AT " in it
  char* st = strstr(to, " AT ");
  if (!st) {
    st = strchr(to, '#');
    if (st) {
      strcpy(to, st + 1);
    }
  } else { // Also try and strip off name of a gated user
    st = strstr(to, "``");
    if (st) {
      st = strstr(st + 1, "``");
      if (st) {
        strcpy(to, st + 2);
      }
    }
  }

  irt[0] = 0;
  irt_name[0] = 0;
  parse_email_info(to, &un, &sy);
  grab_quotes(nullptr, nullptr);


  if (un || sy) {
    messagerec msg;
    char s2[81];
    net_system_list_rec *csne = nullptr;

    if (freek1(syscfg.msgsdir) < 10) {
      GetSession()->bout.NewLine();
      GetSession()->bout.Write("Sorry, not enough disk space left.");
      GetSession()->bout.NewLine();
      pausescr();
      return;
    }

    if (ForwardMessage(&un, &sy)) {
      GetSession()->bout.NewLine();
      GetSession()->bout.Write("Mail Forwarded.]");
      GetSession()->bout.NewLine();
      if ((un == 0) && (sy == 0)) {
        GetSession()->bout.Write("Forwarded to unknown user.");
        pausescr();
        return;
      }
    }

    if (!un && !sy) {
      return;
    }

    if (sy) {
      csne = next_system(sy);
    }

    if (sy == 0) {
      set_net_num(0);
      WUser u;
      GetApplication()->GetUserManager()->ReadUser(&u, un);
      strcpy(s2, u.GetUserNameAndNumber(un));
    } else {
      if (GetSession()->GetMaxNetworkNumber() > 1) {
        if (un == 0) {
          sprintf(s2, "%s @%u.%s", net_email_name, sy, GetSession()->GetNetworkName());
        } else {
          sprintf(s2, "%u @%u.%s", un, sy, GetSession()->GetNetworkName());
        }
      } else {
        if (un == 0) {
          sprintf(s2, "%s @%u", net_email_name, sy);
        } else {
          sprintf(s2, "%u @%u", un, sy);
        }
      }
    }

    if (sy != 0) {
      GetSession()->bout.NewLine();
      GetSession()->bout.WriteFormatted("Name of system: ");
      GetSession()->bout.Write(csne->name);
      GetSession()->bout.WriteFormatted("Number of hops:");
      GetSession()->bout.WriteFormatted("%d", csne->numhops);
      GetSession()->bout.NewLine(2);
    }

    GetSession()->bout.ClearScreen();
    GetSession()->bout.Color(2);
    GetSession()->bout.WriteFormatted("Sending to: %s", s2);
    GetSession()->bout.NewLine();
    GetSession()->bout.Color(2);
    GetSession()->bout.WriteFormatted("Titled    : %s", title);
    GetSession()->bout.NewLine(2);
    GetSession()->bout.Color(5);
    GetSession()->bout.WriteFormatted("Correct? ");

    if (!yesno()) {
      return;
    }

    msg.storage_type = EMAIL_STORAGE;

    time(&thetime);

    const char* nam1 = GetSession()->GetCurrentUser()->GetUserNameNumberAndSystem(GetSession()->usernum, net_sysnum);
    qwk_inmsg(text, size, &msg, "email", nam1, thetime);

    if (msg.stored_as == 0xffffffff) {
      return;
    }

    GetSession()->bout.Color(8);
    sendout_email(title, &msg, 0, un, sy, 1, GetSession()->usernum, net_sysnum, 0, GetSession()->GetNetworkNumber());
  }
}

void qwk_inmsg(const char *text, long size, messagerec *m1, const char *aux, const char *name, long thetime) {
  char s[181];
  int oiia = iia;
  setiia(0);

  messagerec m = *m1;
  char* t = (char *)malloc(size + 2048);
  long pos = 0;
  AddLineToMessageBuffer(t, name, &pos);

  strcpy(s, ctime(&thetime));
  s[strlen(s) - 1] = 0;
  AddLineToMessageBuffer(t, s, &pos);

  memmove(t + pos, text, size);
  pos += size;

  if (t[pos - 1] != 26) {
    t[pos++] = 26;
  }
  savefile(t, pos, &m, aux);

  *m1 = m;

  charbufferpointer = 0;
  charbuffer[0] = 0;
  setiia(oiia);
}

void process_reply_dat(char *name) {
  struct qwk_record qwk;
  char *text;
  long size;
  int curpos = 0;
  int done = 0;
  int to_email = 0;

  int repfile = open(name, O_RDONLY | O_BINARY);

  if (repfile < 0) {
    GetSession()->bout.NewLine();
    GetSession()->bout.Color(3);
    GetSession()->bout.Write("Can't open packet.");
    pausescr();
    return;
  }

  SET_BLOCK(repfile, curpos, sizeof(struct qwk_record));
  read(repfile, &qwk, sizeof(struct qwk_record));

  // Should check to makesure first block contains our bbs id
  ++curpos;

  GetSession()->bout.ClearScreen();

  while (!done && !hangup) {
    to_email = 0;

    SET_BLOCK(repfile, curpos, sizeof(struct qwk_record));
    ++curpos;

    if (read(repfile, &qwk, sizeof(struct qwk_record)) < 1) {
      done = 1;
    } else {
      char blocks[7];
      char to[201];
      char title[26];
      char tosub[8];

      strncpy(blocks, qwk.amount_blocks, 6);
      blocks[6] = 0;

      strncpy(tosub, qwk.msgnum, 7);
      tosub[7] = 0;

      strncpy(title, qwk.subject, 25);
      title[25] = 0;

      strncpy(to, qwk.to, 25);
      to[25] = 0;
      strupr(to);
      StringTrim(to);

      // If in sub 0 or not public, possibly route into email
      if (atoi(tosub) == 0) {
        to_email = 1;
      } else if (qwk.status != ' ' && qwk.status != '-') { // if not public
        GetSession()->bout.ClearScreen();
        GetSession()->bout.Color(1);
        GetSession()->bout.WriteFormatted("Message '2%s1' is marked 3PRIVATE", title);
        GetSession()->bout.NewLine();
        GetSession()->bout.Color(1);
        GetSession()->bout.WriteFormatted("It is addressed to 2%s", to);
        GetSession()->bout.NewLine(2);
        GetSession()->bout.Color(7);
        GetSession()->bout.WriteFormatted("Route into E-Mail?");
        if (noyes()) {
          to_email = 1;
        }
      }

      text = make_text_file(repfile, &size, curpos, atoi(blocks) - 1);
      if (!text) {
        curpos += atoi(blocks) - 1;
        continue;
      }

      if (to_email) {
        char *temp;

        if ((temp = strstr(text, QWKFrom + 2)) != nullptr) {
          char *s;

          temp += strlen(QWKFrom + 2); // Get past 'QWKFrom:'
          s = strchr(temp, '\r');

          if (s) {
            int x;

            s[0] = 0;

            StringTrim(temp);
            strupr(temp);

            if (strlen(s) != strlen(temp)) {
              GetSession()->bout.NewLine();
              GetSession()->bout.Color(3);
              GetSession()->bout.WriteFormatted("1) %s", to);
              GetSession()->bout.NewLine();
              GetSession()->bout.Color(3);
              GetSession()->bout.WriteFormatted("2) %s", temp);
              GetSession()->bout.NewLine(2);

              GetSession()->bout.WriteFormatted("Which address is correct?");
              GetSession()->bout.ColorizedInputField(1);

              x = onek("12");

              if (x == '2') {
                strcpy(to, temp);
              }
            }
          }
        }
      }
            
      if (to_email) {
        qwk_email_text(text, size, title, to);
      } else if (freek1(syscfg.msgsdir) < 10) {
        // Not enough disk space
        GetSession()->bout.NewLine();
        GetSession()->bout.Write("Sorry, not enough disk space left.");
        pausescr();
      } else {
        qwk_post_text(text, size, title, atoi(tosub) - 1);
      }

      free(text);

      curpos += atoi(blocks) - 1;
    }
  }
  repfile = close(repfile);
}

void qwk_post_text(char *text, long size, char *title, int sub) {
  messagerec m;
  postrec p;

  int i, dm, f, done = 0, pass = 0;
  slrec ss;
  long thetime;
  char user_name[101];
  uint8_t a;

  while (!done && !hangup) {
    if (pass > 0) {
      int done5 = 0;
      char substr[5];

      while (!done5 && !hangup) {
        GetSession()->bout.NewLine();
        GetSession()->bout.WriteFormatted("Then which sub?  ?=List  Q=Don't Post :");
        input(substr, 3);

        StringTrim(substr);
        sub = usub[atoi(substr) - 1].subnum;

        if (substr[0] == 'Q') {
          return;
        } else if (substr[0] == '?') {
          SubList();
        } else {
          done5 = 1;
        }
      }
    }


    if (sub >= GetSession()->num_subs || sub < 0) {
      GetSession()->bout.Color(5);
      GetSession()->bout.Write("Sub out of range");

      ++pass;
      continue;
    }
    GetSession()->SetCurrentMessageArea(sub);

    // Busy files... allow to retry
    while (!hangup) {
      if (!qwk_iscan_literal(GetSession()->GetCurrentMessageArea())) {
        GetSession()->bout.NewLine();
        GetSession()->bout.WriteFormatted("MSG file is busy on another instance, try again?");
        if (!noyes()) {
          ++pass;
          continue;
        }
      } else {
        break;
      }
    }

    if (GetSession()->GetCurrentReadMessageArea() < 0) {
      GetSession()->bout.Color(5);
      GetSession()->bout.Write("Sub out of range");

      ++pass;
      continue;
    }

    ss = getslrec(GetSession()->GetEffectiveSl());

    // User is restricked from posting
    if ((restrict_post & GetSession()->GetCurrentUser()->data.restrict)
        || (GetSession()->GetCurrentUser()->data.posttoday >= ss.posts)) {
      GetSession()->bout.NewLine();
      GetSession()->bout.Write("Too many messages posted today.");
      GetSession()->bout.NewLine();

      ++pass;
      continue;
    }

    // User doesn't have enough sl to post on sub
    if (GetSession()->GetEffectiveSl() < subboards[GetSession()->GetCurrentReadMessageArea()].postsl) {
      GetSession()->bout.NewLine();
      GetSession()->bout.Write("You can't post here.");
      GetSession()->bout.NewLine();
      ++pass;
      continue;
    }

    m.storage_type = subboards[GetSession()->GetCurrentReadMessageArea()].storage_type;

    a = 0;

    if (xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets) {
      a &= (anony_real_name);

      if (GetSession()->GetCurrentUser()->data.restrict & restrict_net) {
        GetSession()->bout.NewLine();
        GetSession()->bout.Write("You can't post on networked sub-boards.");
        GetSession()->bout.NewLine();
        ++pass;
        continue;
      }
    }

    GetSession()->bout.ClearScreen();
    GetSession()->bout.Color(2);
    GetSession()->bout.WriteFormatted("Posting    :");
    GetSession()->bout.Color(3);
    GetSession()->bout.Write(title);
    GetSession()->bout.NewLine();

    GetSession()->bout.Color(2);
    GetSession()->bout.WriteFormatted("Posting on :");
    GetSession()->bout.Color(3);
    GetSession()->bout.Write(stripcolors(subboards[GetSession()->GetCurrentReadMessageArea()].name));
    GetSession()->bout.NewLine();

    if (xsubs[GetSession()->GetCurrentReadMessageArea()].nets) {
      GetSession()->bout.Color(2);
      GetSession()->bout.WriteFormatted("Going on   :");
      GetSession()->bout.Color(3);
      GetSession()->bout.Write(
        net_networks[xsubs[GetSession()->GetCurrentReadMessageArea()].nets[xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets].net_num].name);
      GetSession()->bout.NewLine();
    }

    GetSession()->bout.NewLine();
    GetSession()->bout.Color(5);
    GetSession()->bout.WriteFormatted("Correct?");

    if (noyes()) {
      done = 1;
    } else {
      ++pass;
    }
  }
  GetSession()->bout.NewLine();

  if (subboards[GetSession()->GetCurrentReadMessageArea()].anony & anony_real_name) {
    strcpy(user_name, GetSession()->GetCurrentUser()->GetRealName());
    properize(user_name);
  } else {
    const char* nam1 = GetSession()->GetCurrentUser()->GetUserNameNumberAndSystem(GetSession()->usernum, net_sysnum);
    strcpy(user_name, nam1);
  }

  time(&thetime);
  qwk_inmsg(text, size, &m, subboards[GetSession()->GetCurrentReadMessageArea()].filename, user_name, thetime);

  if (m.stored_as != 0xffffffff) {
    char s[201];

    while (!hangup) {
      f = qwk_iscan_literal(GetSession()->GetCurrentReadMessageArea());

      if (f == -1) {
        GetSession()->bout.NewLine();
        GetSession()->bout.WriteFormatted("MSG file is busy on another instance, try again?");
        if (!noyes()) {
          return;
        }
      } else {
        break;
      }
    }

    // Anonymous
    if (a) {
      GetSession()->bout.Color(1);
      GetSession()->bout.WriteFormatted("Anonymous?");
      a = yesno() ? 1 : 0;
    }
    GetSession()->bout.NewLine();

    strcpy(p.title, title);
    p.anony = a;
    p.msg = m;
    p.ownersys = 0;
    p.owneruser = GetSession()->usernum;
    {
      WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
      p.qscan = pStatus->IncrementQScanPointer();
      GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
    }
    time((long *)(&p.daten));
    if (GetSession()->GetCurrentUser()->data.restrict & restrict_validate) {
      p.status = status_unvalidated;
    } else {
      p.status = 0;
    }

    open_sub(1);

    if ((xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets) &&
        (subboards[GetSession()->GetCurrentReadMessageArea()].anony & anony_val_net) && (!lcs() || irt[0])) {
      p.status |= status_pending_net;
      dm = 1;

      for (i = GetSession()->GetNumMessagesInCurrentMessageArea(); (i >= 1)
           && (i > (GetSession()->GetNumMessagesInCurrentMessageArea() - 28)); i--) {
        if (get_post(i)->status & status_pending_net) {
          dm = 0;
          break;
        }
      }
      if (dm) {
        sprintf(s, "Unvalidated net posts on %s.", subboards[GetSession()->GetCurrentReadMessageArea()].name);
        ssm(1, 0, s);
      }
    }

    if (GetSession()->GetNumMessagesInCurrentMessageArea() >=
        subboards[GetSession()->GetCurrentReadMessageArea()].maxmsgs) {
      i = 1;
      dm = 0;
      while ((dm == 0) && (i <= GetSession()->GetNumMessagesInCurrentMessageArea()) && !hangup) {
        if ((get_post(i)->status & status_no_delete) == 0) {
          dm = i;
        }
        ++i;
      }
      if (dm == 0) {
        dm = 1;
      }
      delete_message(dm);
    }

    add_post(&p);

    ++GetSession()->GetCurrentUser()->data.msgpost;
    ++GetSession()->GetCurrentUser()->data.posttoday;

    {
      WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
      pStatus->IncrementNumLocalPosts();
      pStatus->IncrementNumMessagesPostedToday();
      GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
    }

    close_sub();

    sprintf(s, "+ \"%s\" posted on %s", p.title, subboards[GetSession()->GetCurrentReadMessageArea()].name);
    sysoplog(s);

    if (xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets) {
      ++GetSession()->GetCurrentUser()->data.postnet;
      if (!(p.status & status_pending_net)) {
        send_net_post(&p, subboards[GetSession()->GetCurrentReadMessageArea()].filename,
                      GetSession()->GetCurrentReadMessageArea());
      }
    }
  }
}

int find_qwk_sub(struct qwk_sub_conf *subs, int amount, int fromsub) {
  int x = 0;
  while (x < amount && !hangup) {
    if (subs[x].import_num == fromsub) {
      return subs[x].to_num;
    }

    ++x;
  }
  return -1;
}

/* Start DAW */
void qwk_receive_file(char *fn, bool *received, int i) {
  if ((i <= 1) || (i == 5)) {
    i = get_protocol(xf_up_temp);
  }

  switch (i) {
  case -1:
  case 0:
  case WWIV_INTERNAL_PROT_ASCII:
  case WWIV_INTERNAL_PROT_BATCH:
    *received = 0;
    break;
  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM:
    maybe_internal(fn, received, nullptr, false, i);
    break;
  default:
    if (incom) {
      extern_prot(i - 6, fn, 0);
      *received = WFile::Exists(fn);
    }
    break;
  }
}
/* End DAW */

void qwk_sysop(void) {
  struct qwk_config qwk_cfg;
  char temp[10];
  char sn[10];
  int done = 0;
  int x;

  if (!so()) {
    return;
  }

  read_qwk_cfg(&qwk_cfg);

  while (!done && !hangup) {
    qwk_system_name(sn);
    GetSession()->bout.ClearScreen();
    GetSession()->bout.WriteFormatted("[1] Hello   file : %s\r\n", qwk_cfg.hello);
    GetSession()->bout.WriteFormatted("[2] News    file : %s\r\n", qwk_cfg.news);
    GetSession()->bout.WriteFormatted("[3] Goodbye file : %s\r\n", qwk_cfg.bye);
    GetSession()->bout.WriteFormatted("[4] Packet name  : %s\r\n", sn);
    GetSession()->bout.WriteFormatted("[5] Max messages per packet (0=No max): %d\r\n", qwk_cfg.max_msgs);
    GetSession()->bout.WriteFormatted("[6] Modify Bulletins - Current amount= %d\r\n\n", qwk_cfg.amount_blts);
    GetSession()->bout.WriteFormatted("Hit <Enter> or Q to save and exit: [12345<CR>] ");

    x = onek("Q123456\r\n");
    if (x == '1' || x == '2' || x == '3') {
      GetSession()->bout.NewLine();
      GetSession()->bout.Color(1);
      GetSession()->bout.WriteFormatted("Enter new filename:");
      GetSession()->bout.ColorizedInputField(12);
    }

    switch (x) {
    case '1':
      input(qwk_cfg.hello, 12);
      break;
    case '2':
      input(qwk_cfg.news, 12);
      break;
    case '3':
      input(qwk_cfg.bye, 12);
      break;

    case '4':
      write_qwk_cfg(&qwk_cfg);
      qwk_system_name(sn);
      GetSession()->bout.NewLine();
      GetSession()->bout.Color(1);
      GetSession()->bout.WriteFormatted("Current name : %s", sn);
      GetSession()->bout.NewLine();
      GetSession()->bout.WriteFormatted("Enter new packet name: ");
      input(sn, 8);
      if (sn[0]) {
        strcpy(qwk_cfg.packet_name, sn);
      }

      write_qwk_cfg(&qwk_cfg);
      break;

    case '5':
      GetSession()->bout.Color(1);
      GetSession()->bout.WriteFormatted("Enter max messages per packet, 0=No Max: ");
      GetSession()->bout.ColorizedInputField(5);
      input(temp, 5);
      qwk_cfg.max_msgs = static_cast<uint16_t>(atoi(temp));
      break;
    case '6':
      modify_bulletins(&qwk_cfg);
      break;
    default:
      done = 1;
    }
  }

  write_qwk_cfg(&qwk_cfg);
  close_qwk_cfg(&qwk_cfg);
}

void modify_bulletins(struct qwk_config *qwk_cfg) {
  int x, abort = 0, key, done = 0;
  char s[101], t[101];

  while (!done && !hangup) {
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("Add - Delete - ? List - Quit");
    GetSession()->bout.ColorizedInputField(1);

    key = onek("Q\rAD?");

    switch (key) {
    case 'Q':
    case '\r':
      return;

    case 'D':
      GetSession()->bout.NewLine();
      GetSession()->bout.WriteFormatted("Which one?");
      GetSession()->bout.ColorizedInputField(2);

      input(s, 2);
      x = atoi(s);

      if (x <= qwk_cfg->amount_blts) {
        strcpy(qwk_cfg->blt[x], qwk_cfg->blt[qwk_cfg->amount_blts - 1]);
        strcpy(qwk_cfg->bltname[x], qwk_cfg->bltname[qwk_cfg->amount_blts - 1]);

        free(qwk_cfg->blt[qwk_cfg->amount_blts - 1]);
        free(qwk_cfg->bltname[qwk_cfg->amount_blts - 1]);

        --qwk_cfg->amount_blts;
      }
      break;

    case 'A':
      GetSession()->bout.NewLine();
      GetSession()->bout.Write("Enter complete path to Bulletin");
      input(s, 80);

      if (!WFile::Exists(s)) {
        GetSession()->bout.WriteFormatted("File doesn't exist, continue?");
        if (!yesno()) {
          break;
        }
      }

      GetSession()->bout.Write("Now enter its bulletin name, in the format BLT-????.???");
      input(t, BNAME_SIZE);

      if (strncasecmp(t, "BLT-", 4) != 0) {
        GetSession()->bout.Write("Improper format");
        break;
      }

      qwk_cfg->blt[qwk_cfg->amount_blts] = (char *)calloc(BULL_SIZE, sizeof(char));
      qwk_cfg->bltname[qwk_cfg->amount_blts] = (char *)calloc(BNAME_SIZE, sizeof(char));

      strcpy(qwk_cfg->blt[qwk_cfg->amount_blts], s);
      strcpy(qwk_cfg->bltname[qwk_cfg->amount_blts], t);
      ++qwk_cfg->amount_blts;
      break;


    case '?':
      abort = 0;
      x = 0;
      while (x < qwk_cfg->amount_blts && !abort && !hangup) {
        GetSession()->bout.WriteFormatted("[%d] %s Is copied over from", x + 1, qwk_cfg->bltname[x]);
        GetSession()->bout.NewLine();
        repeat_char(' ', 5);
        GetSession()->bout.WriteFormatted(qwk_cfg->blt[x]);
        GetSession()->bout.NewLine();

        abort = checka();

        ++x;
      }
      break;
    }
  }
}

void config_qwk_bw(void) {
  char text[101];
  int done = 0, key;

  while (!done && !hangup) {
    GetSession()->bout.WriteFormatted("A) Scan E-Mail ");
    GetSession()->bout.WriteFormatted(qwk_current_text(0, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("B) Delete Scanned E-Mail ");
    GetSession()->bout.WriteFormatted(qwk_current_text(1, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("C) Set N-Scan of messages ");
    GetSession()->bout.WriteFormatted(qwk_current_text(2, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("D) Remove WWIV color codes ");
    GetSession()->bout.WriteFormatted(qwk_current_text(3, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("E) Convert WWIV color to ANSI ");
    GetSession()->bout.WriteFormatted(qwk_current_text(4, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("F) Pack Bulletins ");
    GetSession()->bout.WriteFormatted(qwk_current_text(5, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("G) Scan New Files ");
    GetSession()->bout.WriteFormatted(qwk_current_text(6, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("H) Remove routing information ");
    GetSession()->bout.WriteFormatted(qwk_current_text(7, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("I) Archive to pack QWK with ");
    GetSession()->bout.WriteFormatted(qwk_current_text(8, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("J) Default transfer protocol ");
    GetSession()->bout.WriteFormatted(qwk_current_text(9, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.WriteFormatted("K) Max messages per pack ");
    GetSession()->bout.WriteFormatted(qwk_current_text(10, text));
    GetSession()->bout.NewLine();
    GetSession()->bout.Write("Q) Done");

    key = onek("QABCDEFGHIJK");

    if (key == 'Q') {
      done = 1;
    }

    key = key - 'A';

    switch (key) {
    case 0:
      GetSession()->GetCurrentUser()->data.qwk_dont_scan_mail = !GetSession()->GetCurrentUser()->data.qwk_dont_scan_mail;
      break;
    case 1:
      GetSession()->GetCurrentUser()->data.qwk_delete_mail = !GetSession()->GetCurrentUser()->data.qwk_delete_mail;
      break;
    case 2:
      GetSession()->GetCurrentUser()->data.qwk_dontsetnscan = !GetSession()->GetCurrentUser()->data.qwk_dontsetnscan;
      break;
    case 3:
      GetSession()->GetCurrentUser()->data.qwk_remove_color = !GetSession()->GetCurrentUser()->data.qwk_remove_color;
      break;
    case 4:
      GetSession()->GetCurrentUser()->data.qwk_convert_color = !GetSession()->GetCurrentUser()->data.qwk_convert_color;
      break;
    case 5:
      GetSession()->GetCurrentUser()->data.qwk_leave_bulletin = !GetSession()->GetCurrentUser()->data.qwk_leave_bulletin;
      break;
    case 6:
      GetSession()->GetCurrentUser()->data.qwk_dontscanfiles = !GetSession()->GetCurrentUser()->data.qwk_dontscanfiles;
      break;
    case 7:
      GetSession()->GetCurrentUser()->data.qwk_keep_routing = !GetSession()->GetCurrentUser()->data.qwk_keep_routing;
      break;

    case 8: {
      struct qwk_junk qj;
      int a;

      memset(&qj, 0, sizeof(struct qwk_junk));
      GetSession()->bout.ClearScreen();

      a = select_qwk_archiver(&qj, 1);
      if (!qj.abort) {
        GetSession()->GetCurrentUser()->data.qwk_archive = a;
      }

      break;
    }

    case 9: {
      struct qwk_junk qj;
      int a;

      memset(&qj, 0, sizeof(struct qwk_junk));
      GetSession()->bout.ClearScreen();

      a = select_qwk_protocol(&qj);
      if (!qj.abort) {
        GetSession()->GetCurrentUser()->data.qwk_protocol = a;
      }

      break;
    }

    case 10: {
      uint16_t max_msgs, max_per_sub;

      if (get_qwk_max_msgs(&max_msgs, &max_per_sub)) {
        GetSession()->GetCurrentUser()->data.qwk_max_msgs = max_msgs;
        GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub = max_per_sub;
      }
      break;
    }

    }
  }
}

const char *qwk_current_text(int pos, char *text) {
  static const char *yesorno[] = { "YES", "NO" };

  switch (pos) {
  case 0:
    if (GetSession()->GetCurrentUser()->data.qwk_dont_scan_mail) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }
  case 1:
    if (GetSession()->GetCurrentUser()->data.qwk_delete_mail) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }
  case 2:
    if (GetSession()->GetCurrentUser()->data.qwk_dontsetnscan) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }
  case 3:
    if (GetSession()->GetCurrentUser()->data.qwk_remove_color) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }
  case 4:
    if (GetSession()->GetCurrentUser()->data.qwk_convert_color) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }

  case 5:
    if (GetSession()->GetCurrentUser()->data.qwk_leave_bulletin) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 6:
    if (GetSession()->GetCurrentUser()->data.qwk_dontscanfiles) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 7:
    if (GetSession()->GetCurrentUser()->data.qwk_keep_routing) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 8:
    qwk_which_zip(text);
    return text;

  case 9:
    qwk_which_protocol(text);
    return text;

  case 10:
    if (!GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub && !GetSession()->GetCurrentUser()->data.qwk_max_msgs) {
      return "Unlimited/Unlimited";
    } else if (!GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub) {
      sprintf(text, "Unlimited/%u", GetSession()->GetCurrentUser()->data.qwk_max_msgs);
      return text;
    } else if (!GetSession()->GetCurrentUser()->data.qwk_max_msgs) {
      sprintf(text, "%u/Unlimited", GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub);
      return text;
    } else {
      sprintf(text, "%u/%u", GetSession()->GetCurrentUser()->data.qwk_max_msgs,
              GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub);
      return text;
    }

  case 11:
    strcpy(text, "DONE");
    return text;
  }

  return nullptr;
}

