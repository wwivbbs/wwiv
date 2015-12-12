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
#include "bbs/qwk.h"

#include <chrono>
#include <memory>
#include <sstream>
#include <ctype.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif
#include <sys/stat.h>

#include "bbs/archivers.h"
#include "bbs/bbs.h"
#include "bbs/conf.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/message_file.h"
#include "bbs/subxtr.h"
#include "sdk/vardec.h"
#include "bbs/vars.h"
#include "bbs/wstatus.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/stuffin.h"
#include "bbs/wconstants.h"
#include "bbs/wwivcolors.h"
#include "core/file.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/scope_exit.h"
#include "core/wwivport.h"
#include "sdk/message_utils_wwiv.h"

using std::chrono::milliseconds;
using std::string;
using std::unique_ptr;

using namespace wwiv::os;
using namespace wwiv::strings;
using namespace wwiv::sdk::msgapi;

#define SET_BLOCK(file, pos, size) lseek(file, (long)pos * (long)size, SEEK_SET)
#define qwk_iscan_literal(x) (iscan1(x))

extern const char *QWKFrom;
extern int qwk_percent;

// from readmail.cpp
bool read_same_email(tmpmailrec * mloc, int mw, int rec, mailrec * m, int del, unsigned short stat);

void qwk_remove_email() {
  emchg = false;

  tmpmailrec* mloc = (tmpmailrec *)malloc(MAXMAIL * sizeof(tmpmailrec));
  if (!mloc) {
    bout.bputs("Not enough memory.");
    return;
  }

  std::unique_ptr<File> f(OpenEmailFile(true));
  
  if (!f->IsOpen()) {
    free(mloc);
    return;
  }

  int mfl = f->GetLength() / sizeof(mailrec);
  uint8_t mw = 0;

  mailrec m;
  for (long i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
    f->Seek(i * sizeof(mailrec), File::seekBegin);
    f->Read(&m, sizeof(mailrec));
    if ((m.tosys == 0) && (m.touser == session()->usernum)) {
      mloc[mw].index = static_cast<short>(i);
      mloc[mw].fromsys = m.fromsys;
      mloc[mw].fromuser = m.fromuser;
      mloc[mw].daten = m.daten;
      mloc[mw].msg = m.msg;
      mw++;
    }
  }
  session()->user()->data.waiting = mw;

  if (session()->usernum == 1) {
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
  int i, mfl, curmail, done;
  char filename[201];
  mailrec m;
  postrec junk;

  emchg = false;
  tmpmailrec *mloc = (tmpmailrec *)malloc(MAXMAIL * sizeof(tmpmailrec));
  if (!mloc) {
    bout.bputs("Not enough memory");
    return;
  }

  slrec ss = getslrec(session()->GetEffectiveSl());
  std::unique_ptr<File> f(OpenEmailFile(false));
  if (!f->IsOpen()) {
    bout.nl(2);
    bout.bputs("No mail file exists!");
    bout.nl();
    free(mloc);
    return;
  }
  mfl = f->GetLength() / sizeof(mailrec);
  uint8_t mw = 0;
  for (i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
    f->Seek(((long)(i)) * (sizeof(mailrec)), File::seekBegin);
    f->Read(&m, sizeof(mailrec));
    if ((m.tosys == 0) && (m.touser == session()->usernum)) {
      mloc[mw].index = static_cast<short>(i);
      mloc[mw].fromsys = m.fromsys;
      mloc[mw].fromuser = m.fromuser;
      mloc[mw].daten = m.daten;
      mloc[mw].msg = m.msg;
      mw++;
    }
  }
  f->Close();
  session()->user()->data.waiting = mw;

  if (session()->usernum == 1) {
    fwaiting = mw;
  }
  
  if (mw == 0) {
    bout.nl();
    bout.bputs("You have no mail.");
    bout.nl();
    free(mloc);
    return;
  }

  bout.Color(7);
  bout.bputs("Gathering Email");

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
    set_net_num(network_number_from(&m));

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

  bout.nl();
  bout.bputs("Select an archiver");
  bout.nl();
  if (ask) {
    bout.bputs("0) Ask me later");
  }
  for (x = 0; x < 4; ++x) {
    strcpy(temp, arcs[x].extension);
    StringTrim(temp);

    if (temp[0]) {
      sprintf(temp, "%d", x + 1);
      strcat(allowed, temp);
      bout.bprintf("1%d) 3%s", x + 1, arcs[x].extension);
      bout.nl();
    }
  }
  bout.nl();
  bout.bprintf("Enter #  Q to Quit <CR>=1 :");

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

string qwk_which_zip() {
  if (session()->user()->data.qwk_archive > 4) {
    session()->user()->data.qwk_archive = 0;
  }

  if (arcs[session()->user()->data.qwk_archive - 1].extension[0] == 0) {
    session()->user()->data.qwk_archive = 0;
  }

  if (session()->user()->data.qwk_archive == 0) {
    return string("ASK");
  } else {
    return string((arcs[session()->user()->data.qwk_archive - 1].extension));
  }
}

string qwk_which_protocol() {
  if (session()->user()->data.qwk_protocol == 1) {
    session()->user()->data.qwk_protocol = 0;
  }

  if (session()->user()->data.qwk_protocol == 0) {
    return string("ASK");
  } else {
    string thisprotocol(prot_name(session()->user()->data.qwk_protocol));
    if (thisprotocol.size() > 22) {
      return thisprotocol.substr(0, 21);
    }
    return thisprotocol;
  }
}

void upload_reply_packet() {
  char name[21], namepath[101];
  bool rec = true;
  int save_conf = 0, save_sub;
  struct qwk_config qwk_cfg;


  read_qwk_cfg(&qwk_cfg);

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = static_cast<int32_t>(time(nullptr));
  }

  ++qwk_cfg.timesu;
  write_qwk_cfg(&qwk_cfg);
  close_qwk_cfg(&qwk_cfg);

  save_sub = session()->GetCurrentMessageArea();
  if ((uconfsub[1].confnum != -1) && (okconf(session()->user()))) {
    save_conf = 1;
    tmp_disable_conf(true);
  }

  qwk_system_name(name);
  strcat(name, ".REP");

  bout.bprintf("Hit 'Y' to upload reply packet %s :", name);

  sprintf(namepath, "%s%s", QWK_DIRECTORY, name);
  
  bool do_it = yesno();

  if (do_it) {
    if (incom) {
      qwk_receive_file(namepath, &rec, session()->user()->data.qwk_protocol);
      sleep_for(milliseconds(500));
    }

    if (rec) {
      qwk_system_name(name);
      strcat(name, ".MSG");

      ready_reply_packet(namepath, name);

      sprintf(namepath, "%s%s", QWK_DIRECTORY, name);
      process_reply_dat(namepath);
    } else {
      sysoplog("Aborted");
      bout.nl();
      bout.bprintf("%s not found", name);
      bout.nl();
    }
  }
  if (save_conf) {
    tmp_disable_conf(false);
  }

  session()->SetCurrentMessageArea(save_sub);
}

void ready_reply_packet(const char *packet_name, const char *msg_name) {
  int archiver = match_archiver(packet_name);
  string command = stuff_in(arcs[archiver].arce, packet_name, msg_name, "", "", "");

  chdir(QWK_DIRECTORY);
  ExecuteExternalProgram(command, EFLAG_NONE);
  session()->CdHome();
}

// Takes reply packet and converts '227' (�) to '13'
void make_text_ready(char *text, long len) {
  int pos = 0;

  while (pos < len && !hangup) {
    if (text[pos] == '\xE3') {
      text[pos] = 13;
    }
    ++pos;
  }
}

char* make_text_file(int filenumber, int curpos, int blocks) {
  // This memory has to be freed later, after text is 'emailed' or 'posted'
  // Enough memory is allocated for all blocks, plus 2k extra for other
  // 'addline' stuff
  qwk_junk *qwk = (qwk_junk *) malloc((blocks * sizeof(qwk_junk)) + 2048);
  if (!qwk) {
    return nullptr;
  }
  SET_BLOCK(filenumber, curpos, sizeof(qwk_record));
  read(filenumber, qwk, sizeof(qwk_record) * blocks);
  make_text_ready((char *)qwk, sizeof(qwk_record)*blocks);

  return reinterpret_cast<char*>(qwk);
}

void qwk_email_text(char *text, char *title, char *to) {
  int sy, un;

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
      bout.nl();
      bout.bputs("Sorry, not enough disk space left.");
      bout.nl();
      pausescr();
      return;
    }

    if (ForwardMessage(&un, &sy)) {
      bout.nl();
      bout.bputs("Mail Forwarded.]");
      bout.nl();
      if ((un == 0) && (sy == 0)) {
        bout.bputs("Forwarded to unknown user.");
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
      session()->users()->ReadUser(&u, un);
      strcpy(s2, u.GetUserNameAndNumber(un));
    } else {
      if (session()->GetMaxNetworkNumber() > 1) {
        if (un == 0) {
          sprintf(s2, "%s @%u.%s", net_email_name, sy, session()->GetNetworkName());
        } else {
          sprintf(s2, "%u @%u.%s", un, sy, session()->GetNetworkName());
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
      bout.nl();
      bout.bprintf("Name of system: ");
      bout.bputs(csne->name);
      bout.bprintf("Number of hops:");
      bout.bprintf("%d", csne->numhops);
      bout.nl(2);
    }

    bout.cls();
    bout.Color(2);
    bout.bprintf("Sending to: %s", s2);
    bout.nl();
    bout.Color(2);
    bout.bprintf("Titled    : %s", title);
    bout.nl(2);
    bout.Color(5);
    bout.bprintf("Correct? ");

    if (!yesno()) {
      return;
    }

    msg.storage_type = EMAIL_STORAGE;
    time_t thetime = time(nullptr);

    const char* nam1 = session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum);
    qwk_inmsg(text, &msg, "email", nam1, thetime);

    if (msg.stored_as == 0xffffffff) {
      return;
    }

    bout.Color(8);
    sendout_email(title, &msg, 0, un, sy, 1, session()->usernum, net_sysnum, false, session()->GetNetworkNumber());
  }
}

void qwk_inmsg(const char *text, messagerec *m1, const char *aux, const char *name, time_t thetime) {
  char s[181];
  int oiia = iia;
  setiia(0);
  wwiv::core::ScopeExit  at_exit([=]() {
    charbufferpointer = 0;
    charbuffer[0] = 0;
    setiia(oiia);
  });

  messagerec m = *m1;
  std::ostringstream ss;
  ss << name << "\r\n";

  strcpy(s, ctime(&thetime));
  s[strlen(s) - 1] = 0;
  ss << s << "\r\n";
  ss << text << "\r\n";

  std::string message_text = ss.str();
  if (message_text.back() != CZ) {
    message_text.push_back(CZ);
  }
  savefile(message_text, &m, aux);
  *m1 = m;
}

void process_reply_dat(char *name) {
  struct qwk_record qwk;
  int curpos = 0;
  int done = 0;
  int to_email = 0;

  int repfile = open(name, O_RDONLY | O_BINARY);

  if (repfile < 0) {
    bout.nl();
    bout.Color(3);
    bout.bputs("Can't open packet.");
    pausescr();
    return;
  }

  SET_BLOCK(repfile, curpos, sizeof(struct qwk_record));
  read(repfile, &qwk, sizeof(struct qwk_record));

  // Should check to makesure first block contains our bbs id
  ++curpos;

  bout.cls();

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
        bout.cls();
        bout.Color(1);
        bout.bprintf("Message '2%s1' is marked 3PRIVATE", title);
        bout.nl();
        bout.Color(1);
        bout.bprintf("It is addressed to 2%s", to);
        bout.nl(2);
        bout.Color(7);
        bout.bprintf("Route into E-Mail?");
        if (noyes()) {
          to_email = 1;
        }
      }

      char* text = make_text_file(repfile, curpos, atoi(blocks) - 1);
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
              bout.nl();
              bout.Color(3);
              bout.bprintf("1) %s", to);
              bout.nl();
              bout.Color(3);
              bout.bprintf("2) %s", temp);
              bout.nl(2);

              bout.bprintf("Which address is correct?");
              bout.mpl(1);

              x = onek("12");

              if (x == '2') {
                strcpy(to, temp);
              }
            }
          }
        }
      }
            
      if (to_email) {
        qwk_email_text(text, title, to);
      } else if (freek1(syscfg.msgsdir) < 10) {
        // Not enough disk space
        bout.nl();
        bout.bputs("Sorry, not enough disk space left.");
        pausescr();
      } else {
        qwk_post_text(text, title, atoi(tosub) - 1);
      }

      free(text);

      curpos += atoi(blocks) - 1;
    }
  }
  repfile = close(repfile);
}

void qwk_post_text(char *text, char *title, int sub) {
  messagerec m;
  postrec p;

  int dm, f, done = 0, pass = 0;
  slrec ss;
  char user_name[101];
  uint8_t a;

  while (!done && !hangup) {
    if (pass > 0) {
      int done5 = 0;
      char substr[5];

      while (!done5 && !hangup) {
        bout.nl();
        bout.bprintf("Then which sub?  ?=List  Q=Don't Post :");
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


    if (sub >= session()->num_subs || sub < 0) {
      bout.Color(5);
      bout.bputs("Sub out of range");

      ++pass;
      continue;
    }
    session()->SetCurrentMessageArea(sub);

    // Busy files... allow to retry
    while (!hangup) {
      if (!qwk_iscan_literal(session()->GetCurrentMessageArea())) {
        bout.nl();
        bout.bprintf("MSG file is busy on another instance, try again?");
        if (!noyes()) {
          ++pass;
          continue;
        }
      } else {
        break;
      }
    }

    if (session()->GetCurrentReadMessageArea() < 0) {
      bout.Color(5);
      bout.bputs("Sub out of range");

      ++pass;
      continue;
    }

    ss = getslrec(session()->GetEffectiveSl());

    // User is restricked from posting
    if ((restrict_post & session()->user()->data.restrict)
        || (session()->user()->data.posttoday >= ss.posts)) {
      bout.nl();
      bout.bputs("Too many messages posted today.");
      bout.nl();

      ++pass;
      continue;
    }

    // User doesn't have enough sl to post on sub
    if (session()->GetEffectiveSl() < subboards[session()->GetCurrentReadMessageArea()].postsl) {
      bout.nl();
      bout.bputs("You can't post here.");
      bout.nl();
      ++pass;
      continue;
    }

    m.storage_type = static_cast<uint8_t>(subboards[session()->GetCurrentReadMessageArea()].storage_type);

    a = 0;

    if (xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
      a &= (anony_real_name);

      if (session()->user()->data.restrict & restrict_net) {
        bout.nl();
        bout.bputs("You can't post on networked sub-boards.");
        bout.nl();
        ++pass;
        continue;
      }
    }

    bout.cls();
    bout.Color(2);
    bout.bprintf("Posting    : ");
    bout.Color(3);
    bout.bputs(title);
    bout.nl();

    bout.Color(2);
    bout.bprintf("Posting on : ");
    bout.Color(3);
    bout.bputs(stripcolors(subboards[session()->GetCurrentReadMessageArea()].name));
    bout.nl();

    if (xsubs[session()->GetCurrentReadMessageArea()].nets) {
      bout.Color(2);
      bout.bprintf("Going on   : ");
      bout.Color(3);
      for (int i = 0; i < xsubs[session()->GetCurrentReadMessageArea()].num_nets; i++) {
        xtrasubsnetrec *xnp = &xsubs[session()->GetCurrentReadMessageArea()].nets[i];
        string network_name = net_networks[xnp->net_num].name;
        bout << network_name << " ";
      }
      bout.nl();
    }

    bout.nl();
    bout.Color(5);
    bout.bprintf("Correct? ");

    if (noyes()) {
      done = 1;
    } else {
      ++pass;
    }
  }
  bout.nl();

  if (subboards[session()->GetCurrentReadMessageArea()].anony & anony_real_name) {
    strcpy(user_name, session()->user()->GetRealName());
    properize(user_name);
  } else {
    const char* nam1 = session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum);
    strcpy(user_name, nam1);
  }

  time_t thetime = time(nullptr);
  qwk_inmsg(text, &m, subboards[session()->GetCurrentReadMessageArea()].filename, user_name, thetime);

  if (m.stored_as != 0xffffffff) {
    char s[201];

    while (!hangup) {
      f = qwk_iscan_literal(session()->GetCurrentReadMessageArea());

      if (f == -1) {
        bout.nl();
        bout.bprintf("MSG file is busy on another instance, try again?");
        if (!noyes()) {
          return;
        }
      } else {
        break;
      }
    }

    // Anonymous
    if (a) {
      bout.Color(1);
      bout.bprintf("Anonymous?");
      a = yesno() ? 1 : 0;
    }
    bout.nl();

    strcpy(p.title, title);
    p.anony = a;
    p.msg = m;
    p.ownersys = 0;
    p.owneruser = static_cast<uint16_t>(session()->usernum);
    {
      WStatus* pStatus = session()->GetStatusManager()->BeginTransaction();
      p.qscan = pStatus->IncrementQScanPointer();
      session()->GetStatusManager()->CommitTransaction(pStatus);
    }
    time_t now = time(nullptr);
    p.daten = static_cast<uint32_t>(now);
    if (session()->user()->data.restrict & restrict_validate) {
      p.status = status_unvalidated;
    } else {
      p.status = 0;
    }

    open_sub(1);

    if ((xsubs[session()->GetCurrentReadMessageArea()].num_nets) &&
        (subboards[session()->GetCurrentReadMessageArea()].anony & anony_val_net) && (!lcs() || irt[0])) {
      p.status |= status_pending_net;
      dm = 1;

      for (int i = session()->GetNumMessagesInCurrentMessageArea(); (i >= 1)
           && (i > (session()->GetNumMessagesInCurrentMessageArea() - 28)); i--) {
        if (get_post(i)->status & status_pending_net) {
          dm = 0;
          break;
        }
      }
      if (dm) {
        sprintf(s, "Unvalidated net posts on %s.", subboards[session()->GetCurrentReadMessageArea()].name);
        ssm(1, 0, s);
      }
    }

    if (session()->GetNumMessagesInCurrentMessageArea() >=
        subboards[session()->GetCurrentReadMessageArea()].maxmsgs) {
      int i = 1;
      dm = 0;
      while ((dm == 0) && (i <= session()->GetNumMessagesInCurrentMessageArea()) && !hangup) {
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

    ++session()->user()->data.msgpost;
    ++session()->user()->data.posttoday;

    {
      WStatus* pStatus = session()->GetStatusManager()->BeginTransaction();
      pStatus->IncrementNumLocalPosts();
      pStatus->IncrementNumMessagesPostedToday();
      session()->GetStatusManager()->CommitTransaction(pStatus);
    }

    close_sub();

    sprintf(s, "+ \"%s\" posted on %s", p.title, subboards[session()->GetCurrentReadMessageArea()].name);
    sysoplog(s);

    if (xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
      ++session()->user()->data.postnet;
      if (!(p.status & status_pending_net)) {
        send_net_post(&p, subboards[session()->GetCurrentReadMessageArea()].filename,
                      session()->GetCurrentReadMessageArea());
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
      extern_prot(i - WWIV_NUM_INTERNAL_PROTOCOLS, fn, 0);
      *received = File::Exists(fn);
    }
    break;
  }
}
/* End DAW */

void qwk_sysop() {
  struct qwk_config qwk_cfg;
  char sn[10];

  if (!so()) {
    return;
  }

  read_qwk_cfg(&qwk_cfg);

  bool done = false;
  while (!done && !hangup) {
    qwk_system_name(sn);
    bout.cls();
    bout.bprintf("[1] Hello   file : %s\r\n", qwk_cfg.hello);
    bout.bprintf("[2] News    file : %s\r\n", qwk_cfg.news);
    bout.bprintf("[3] Goodbye file : %s\r\n", qwk_cfg.bye);
    bout.bprintf("[4] Packet name  : %s\r\n", sn);
    bout.bprintf("[5] Max messages per packet (0=No max): %d\r\n", qwk_cfg.max_msgs);
    bout.bprintf("[6] Modify Bulletins - Current amount= %d\r\n\n", qwk_cfg.amount_blts);
    bout.bprintf("Hit <Enter> or Q to save and exit: [12345<CR>] ");

    int x = onek("Q123456\r\n");
    if (x == '1' || x == '2' || x == '3') {
      bout.nl();
      bout.Color(1);
      bout.bprintf("Enter new filename:");
      bout.mpl(12);
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
      bout.nl();
      bout.Color(1);
      bout.bprintf("Current name : %s", sn);
      bout.nl();
      bout.bprintf("Enter new packet name: ");
      input(sn, 8);
      if (sn[0]) {
        strcpy(qwk_cfg.packet_name, sn);
      }

      write_qwk_cfg(&qwk_cfg);
      break;

    case '5': {
      bout.Color(1);
      bout.bprintf("Enter max messages per packet, 0=No Max: ");
      bout.mpl(5);
      string tmp;
      input(&tmp, 5);
      qwk_cfg.max_msgs = static_cast<uint16_t>(atoi(tmp.c_str()));
    } break;
    case '6':
      modify_bulletins(&qwk_cfg);
      break;
    default:
      done = true;
    }
  }

  write_qwk_cfg(&qwk_cfg);
  close_qwk_cfg(&qwk_cfg);
}

void modify_bulletins(struct qwk_config *qwk_cfg) {
  char s[101], t[101];

  bool done = false;
  while (!done && !hangup) {
    bout.nl();
    bout.bprintf("Add - Delete - ? List - Quit");
    bout.mpl(1);

    int key = onek("Q\rAD?");

    switch (key) {
    case 'Q':
    case '\r':
      return;
    case 'D': {
      bout.nl();
      bout.bprintf("Which one?");
      bout.mpl(2);

      input(s, 2);
      int x = atoi(s);

      if (x <= qwk_cfg->amount_blts) {
        strcpy(qwk_cfg->blt[x], qwk_cfg->blt[qwk_cfg->amount_blts - 1]);
        strcpy(qwk_cfg->bltname[x], qwk_cfg->bltname[qwk_cfg->amount_blts - 1]);

        free(qwk_cfg->blt[qwk_cfg->amount_blts - 1]);
        free(qwk_cfg->bltname[qwk_cfg->amount_blts - 1]);

        --qwk_cfg->amount_blts;
      }
    } break;
    case 'A':
      bout.nl();
      bout.bputs("Enter complete path to Bulletin");
      input(s, 80);

      if (!File::Exists(s)) {
        bout.bprintf("File doesn't exist, continue?");
        if (!yesno()) {
          break;
        }
      }

      bout.bputs("Now enter its bulletin name, in the format BLT-????.???");
      input(t, BNAME_SIZE);

      if (strncasecmp(t, "BLT-", 4) != 0) {
        bout.bputs("Improper format");
        break;
      }

      qwk_cfg->blt[qwk_cfg->amount_blts] = (char *)calloc(BULL_SIZE, sizeof(char));
      qwk_cfg->bltname[qwk_cfg->amount_blts] = (char *)calloc(BNAME_SIZE, sizeof(char));

      strcpy(qwk_cfg->blt[qwk_cfg->amount_blts], s);
      strcpy(qwk_cfg->bltname[qwk_cfg->amount_blts], t);
      ++qwk_cfg->amount_blts;
      break;
    case '?': {
      bool abort = false;
      int x = 0;
      while (x < qwk_cfg->amount_blts && !abort && !hangup) {
        bout.bprintf("[%d] %s Is copied over from", x + 1, qwk_cfg->bltname[x]);
        bout.nl();
        bout.Color(7);
        bout << string(78, '\xCD');
        bout.nl();
        bout.bprintf(qwk_cfg->blt[x]);
        bout.nl();
        abort = checka();
        ++x;
      }
    } break;
    }
  }
}

void config_qwk_bw() {
  bool done = false;

  while (!done && !hangup) {
    bout << "A) Scan E-Mail " << qwk_current_text(0);
    bout.nl();
    bout<< "B) Delete Scanned E-Mail " << qwk_current_text(1);
    bout.nl();
    bout << "C) Set N-Scan of messages " << qwk_current_text(2);
    bout.nl();
    bout << "D) Remove WWIV color codes " << qwk_current_text(3);
    bout.nl();
    bout << "E) Convert WWIV color to ANSI " << qwk_current_text(4);
    bout.nl();
    bout << "F) Pack Bulletins " << qwk_current_text(5);
    bout.nl();
    bout << "G) Scan New Files " << qwk_current_text(6);
    bout.nl();
    bout << "H) Remove routing information " << qwk_current_text(7);
    bout.nl();
    bout << "I) Archive to pack QWK with " << qwk_current_text(8);
    bout.nl();
    bout << "J) Default transfer protocol " << qwk_current_text(9);
    bout.nl();
    bout << "K) Max messages per pack " << qwk_current_text(10);
    bout.nl();
    bout << "Q) Done";

    int key = onek("QABCDEFGHIJK");

    if (key == 'Q') {
      done = true;
    }
    key = key - 'A';

    switch (key) {
    case 0:
      session()->user()->data.qwk_dont_scan_mail = !session()->user()->data.qwk_dont_scan_mail;
      break;
    case 1:
      session()->user()->data.qwk_delete_mail = !session()->user()->data.qwk_delete_mail;
      break;
    case 2:
      session()->user()->data.qwk_dontsetnscan = !session()->user()->data.qwk_dontsetnscan;
      break;
    case 3:
      session()->user()->data.qwk_remove_color = !session()->user()->data.qwk_remove_color;
      break;
    case 4:
      session()->user()->data.qwk_convert_color = !session()->user()->data.qwk_convert_color;
      break;
    case 5:
      session()->user()->data.qwk_leave_bulletin = !session()->user()->data.qwk_leave_bulletin;
      break;
    case 6:
      session()->user()->data.qwk_dontscanfiles = !session()->user()->data.qwk_dontscanfiles;
      break;
    case 7:
      session()->user()->data.qwk_keep_routing = !session()->user()->data.qwk_keep_routing;
      break;
    case 8: {
      struct qwk_junk qj;
      memset(&qj, 0, sizeof(struct qwk_junk));
      bout.cls();

      int a = select_qwk_archiver(&qj, 1);
      if (!qj.abort) {
        session()->user()->data.qwk_archive = a;
      }
      break;
    }
    case 9: {
      struct qwk_junk qj;
      memset(&qj, 0, sizeof(struct qwk_junk));
      bout.cls();

      int a = select_qwk_protocol(&qj);
      if (!qj.abort) {
        session()->user()->data.qwk_protocol = a;
      }
    } break;
    case 10: {
      uint16_t max_msgs, max_per_sub;

      if (get_qwk_max_msgs(&max_msgs, &max_per_sub)) {
        session()->user()->data.qwk_max_msgs = max_msgs;
        session()->user()->data.qwk_max_msgs_per_sub = max_per_sub;
      }
    } break;
    }
  }
}

string qwk_current_text(int pos) {
  static const char *yesorno[] = { "YES", "NO" };

  switch (pos) {
  case 0:
    if (session()->user()->data.qwk_dont_scan_mail) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }
  case 1:
    if (session()->user()->data.qwk_delete_mail) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }
  case 2:
    if (session()->user()->data.qwk_dontsetnscan) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }
  case 3:
    if (session()->user()->data.qwk_remove_color) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }
  case 4:
    if (session()->user()->data.qwk_convert_color) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }

  case 5:
    if (session()->user()->data.qwk_leave_bulletin) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 6:
    if (session()->user()->data.qwk_dontscanfiles) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 7:
    if (session()->user()->data.qwk_keep_routing) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 8:
    return qwk_which_zip();

  case 9:
    return qwk_which_protocol();

  case 10:
    if (!session()->user()->data.qwk_max_msgs_per_sub && !session()->user()->data.qwk_max_msgs) {
      return "Unlimited/Unlimited";
    } else if (!session()->user()->data.qwk_max_msgs_per_sub) {
      return StringPrintf("Unlimited/%u", session()->user()->data.qwk_max_msgs);
    } else if (!session()->user()->data.qwk_max_msgs) {
      return StringPrintf("%u/Unlimited", session()->user()->data.qwk_max_msgs_per_sub);
    } else {
      return StringPrintf("%u/%u", session()->user()->data.qwk_max_msgs,
              session()->user()->data.qwk_max_msgs_per_sub);
    }

  case 11:
    return string("DONE");
  }

  return nullptr;
}

