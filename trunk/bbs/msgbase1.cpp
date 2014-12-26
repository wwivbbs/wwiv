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
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include <memory>
#include <string>

#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/subxtr.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/textfile.h"

using std::string;
using std::unique_ptr;
using std::unique_ptr;
using wwiv::strings::StringPrintf;

void send_net_post(postrec* pPostRecord, const char* extra, int nSubNumber) {
  long lMessageLength;
  unique_ptr<char[]> b(readfile(&(pPostRecord->msg), extra, &lMessageLength));
  if (!b) {
    return;
  }

  int nNetNumber;
  int nOrigNetNumber = session()->GetNetworkNumber();
  if (pPostRecord->status & status_post_new_net) {
    nNetNumber = pPostRecord->title[80];
  } else if (xsubs[nSubNumber].num_nets) {
    nNetNumber = xsubs[nSubNumber].nets[0].net_num;
  } else {
    nNetNumber = session()->GetNetworkNumber();
  }

  int nn1 = nNetNumber;
  if (pPostRecord->ownersys == 0) {
    nNetNumber = -1;
  }

  net_header_rec netHeaderOrig;
  netHeaderOrig.tosys   = 0;
  netHeaderOrig.touser  = 0;
  netHeaderOrig.fromsys = pPostRecord->ownersys;
  netHeaderOrig.fromuser  = pPostRecord->owneruser;
  netHeaderOrig.list_len  = 0;
  netHeaderOrig.daten   = pPostRecord->daten;
  netHeaderOrig.length  = lMessageLength + 1 + strlen(pPostRecord->title);
  netHeaderOrig.method  = 0;

  if (netHeaderOrig.length > 32755) {
    bout.bprintf("Message truncated by %lu bytes for the network.", netHeaderOrig.length - 32755L);
    netHeaderOrig.length = 32755;
    lMessageLength = netHeaderOrig.length - strlen(pPostRecord->title) - 1;
  }
  unique_ptr<char[]> b1(new char[netHeaderOrig.length + 100]);
  if (!b1) {
    set_net_num(nOrigNetNumber);
    return;
  }
  strcpy(b1.get(), pPostRecord->title);
  memmove(&(b1[strlen(pPostRecord->title) + 1]), b.get(), lMessageLength);

  for (int n = 0; n < xsubs[nSubNumber].num_nets; n++) {
    xtrasubsnetrec* xnp = &(xsubs[nSubNumber].nets[n]);
    if (xnp->net_num == nNetNumber && xnp->host) {
      continue;
    }
    set_net_num(xnp->net_num);
    net_header_rec nh = netHeaderOrig;
    unsigned short int *pList = nullptr;
    nh.minor_type = xnp->type;
    if (!nh.fromsys) {
      nh.fromsys = net_sysnum;
    }
    if (xnp->host) {
      nh.main_type = main_type_pre_post;
      nh.tosys = xnp->host;
    } else {
      nh.main_type = main_type_post;
      const string filename = StringPrintf("%sn%s.net", session()->GetNetworkDataDirectory().c_str(), xnp->stype);
      File file(filename);
      if (file.Open(File::modeBinary | File::modeReadOnly)) {
        int len1 = file.GetLength();
        pList = static_cast<unsigned short int *>(BbsAllocA(len1 * 2 + 1));
        if (!pList) {
          continue;
        }
        // looks like this leaks
        b.reset(new char[len1 + 100]);
        if (!b) {
          free(pList);
          continue;
        }
        file.Read(b.get(), len1);
        file.Close();
        b[len1] = '\0';
        int len2 = 0;
        while (len2 < len1) {
          while (len2 < len1 && (b[len2] < '0' || b[len2] > '9')) {
            ++len2;
          }
          if ((b[len2] >= '0') && (b[len2] <= '9') && (len2 < len1)) {
            int i = atoi(&(b[len2]));
            if (((session()->GetNetworkNumber() != nNetNumber) || (nh.fromsys != i)) && (i != net_sysnum)) {
              if (valid_system(i)) {
                pList[(nh.list_len)++] = static_cast< unsigned short >(i);
              }
            }
            while ((len2 < len1) && (b[len2] >= '0') && (b[len2] <= '9')) {
              ++len2;
            }
          }
        }
        // delete[] p
      }
      if (!nh.list_len) {
        if (pList) {
          free(pList);
        }
        continue;
      }
    }
    if (!xnp->type) {
      nh.main_type = main_type_new_post;
    }
    if (nn1 == session()->GetNetworkNumber()) {
      send_net(&nh, pList, b1.get(), xnp->type ? nullptr : xnp->stype);
    } else {
      gate_msg(&nh, b1.get(), xnp->net_num, xnp->stype, pList, nNetNumber);
    }
    if (pList) {
      free(pList);
    }
  }

  set_net_num(nOrigNetNumber);
}

void post() {
  if (!iscan(session()->GetCurrentMessageArea())) {
    bout << "\r\n|#6A file required is in use by another instance. Try again later.\r\n";
    return;
  }
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout << "\r\nNo subs available.\r\n\n";
    return;
  }

  if (freek1(syscfg.msgsdir) < 10) {
    bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  if (session()->user()->IsRestrictionPost()
      || session()->user()->GetNumPostsToday() >= getslrec(session()->GetEffectiveSl()).posts) {
    bout << "\r\nToo many messages posted today.\r\n\n";
    return;
  }
  if (session()->GetEffectiveSl() < subboards[session()->GetCurrentReadMessageArea()].postsl) {
    bout << "\r\nYou can't post here.\r\n\n";
    return;
  }

  messagerec m;
  m.storage_type = static_cast<unsigned char>(subboards[session()->GetCurrentReadMessageArea()].storage_type);
  int a = subboards[ session()->GetCurrentReadMessageArea() ].anony & 0x0f;
  if (a == 0 && getslrec(session()->GetEffectiveSl()).ability & ability_post_anony) {
    a = anony_enable_anony;
  }
  if (a == anony_enable_anony && session()->user()->IsRestrictionAnonymous()) {
    a = 0;
  }
  if (xsubs[ session()->GetCurrentReadMessageArea() ].num_nets) {
    a &= (anony_real_name);
    if (session()->user()->IsRestrictionNet()) {
      bout << "\r\nYou can't post on networked sub-boards.\r\n\n";
      return;
    }
    if (net_sysnum) {
      bout << "\r\nThis post will go out on ";
      for (int i = 0; i < xsubs[ session()->GetCurrentReadMessageArea() ].num_nets; i++) {
        if (i) {
          bout << ", ";
        }
        bout << net_networks[xsubs[session()->GetCurrentReadMessageArea()].nets[i].net_num].name;
      }
      bout << ".\r\n\n";
    }
  }
  time_t lStartTime = time(nullptr);

  write_inst(INST_LOC_POST, session()->GetCurrentReadMessageArea(), INST_FLAGS_NONE);

  postrec p;
  string title;
  inmsg(&m, &title, &a, true, (subboards[session()->GetCurrentReadMessageArea()].filename), INMSG_FSED,
        subboards[session()->GetCurrentReadMessageArea()].name,
        (subboards[session()->GetCurrentReadMessageArea()].anony & anony_no_tag) ? MSGED_FLAG_NO_TAGLINE : MSGED_FLAG_NONE);
  strcpy(p.title, title.c_str());
  if (m.stored_as != 0xffffffff) {
    p.anony   = static_cast< unsigned char >(a);
    p.msg   = m;
    p.ownersys  = 0;
    p.owneruser = static_cast<unsigned short>(session()->usernum);
    WStatus* pStatus = application()->GetStatusManager()->BeginTransaction();
    p.qscan = pStatus->IncrementQScanPointer();
    application()->GetStatusManager()->CommitTransaction(pStatus);
    p.daten = static_cast<unsigned long>(time(nullptr));
    if (session()->user()->IsRestrictionValidate()) {
      p.status = status_unvalidated;
    } else {
      p.status = 0;
    }

    open_sub(true);

    if ((xsubs[session()->GetCurrentReadMessageArea()].num_nets) &&
        (subboards[session()->GetCurrentReadMessageArea()].anony & anony_val_net) && (!lcs() || irt[0])) {
      p.status |= status_pending_net;
      int dm = 1;
      for (int i = session()->GetNumMessagesInCurrentMessageArea(); (i >= 1)
           && (i > (session()->GetNumMessagesInCurrentMessageArea() - 28)); i--) {
        if (get_post(i)->status & status_pending_net) {
          dm = 0;
          break;
        }
      }
      if (dm) {
        ssm(1, 0, "Unvalidated net posts on %s.", subboards[session()->GetCurrentReadMessageArea()].name);
      }
    }
    if (session()->GetNumMessagesInCurrentMessageArea() >=
        subboards[session()->GetCurrentReadMessageArea()].maxmsgs) {
      int i = 1;
      int dm = 0;
      while (i <= session()->GetNumMessagesInCurrentMessageArea()) {
        postrec* pp = get_post(i);
        if (!pp) {
          break;
        } else if (((pp->status & status_no_delete) == 0) ||
                   (pp->msg.storage_type != subboards[session()->GetCurrentReadMessageArea()].storage_type)) {
          dm = i;
          break;
        }
        ++i;
      }
      if (dm == 0) {
        dm = 1;
      }
      delete_message(dm);
    }
    add_post(&p);

    session()->user()->SetNumMessagesPosted(session()->user()->GetNumMessagesPosted() + 1);
    session()->user()->SetNumPostsToday(session()->user()->GetNumPostsToday() + 1);
    pStatus = application()->GetStatusManager()->BeginTransaction();
    pStatus->IncrementNumMessagesPostedToday();
    pStatus->IncrementNumLocalPosts();

    if (application()->HasConfigFlag(OP_FLAGS_POSTTIME_COMPENSATE)) {
      time_t lEndTime = time(nullptr);
      if (lStartTime > lEndTime) {
        lEndTime += HOURS_PER_DAY * SECONDS_PER_DAY;
      }
      lStartTime = static_cast<long>(lEndTime - lStartTime);
      if ((lStartTime / MINUTES_PER_HOUR_FLOAT) > getslrec(session()->GetEffectiveSl()).time_per_logon) {
        lStartTime = static_cast<long>(static_cast<float>(getslrec(session()->GetEffectiveSl()).time_per_logon *
                                       MINUTES_PER_HOUR_FLOAT));
      }
      session()->user()->SetExtraTime(session()->user()->GetExtraTime() + static_cast<float>
          (lStartTime));
    }
    application()->GetStatusManager()->CommitTransaction(pStatus);
    close_sub();

    application()->UpdateTopScreen();
    sysoplogf("+ \"%s\" posted on %s", p.title, subboards[session()->GetCurrentReadMessageArea()].name);
    bout << "Posted on " << subboards[session()->GetCurrentReadMessageArea()].name << wwiv::endl;
    if (xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
      session()->user()->SetNumNetPosts(session()->user()->GetNumNetPosts() + 1);
      if (!(p.status & status_pending_net)) {
        send_net_post(&p, subboards[session()->GetCurrentReadMessageArea()].filename,
                      session()->GetCurrentReadMessageArea());
      }
    }
  }
}

void grab_user_name(messagerec* pMessageRecord, const char* pszFileName) {
  long lMessageLen;
  unique_ptr<char[]> ss(readfile(pMessageRecord, pszFileName, &lMessageLen));
  if (ss.get()) {
    char* ss1 = strchr(ss.get(), '\r');
    if (ss1) {
      *ss1 = '\0';
      char* ss2 = ss.get();
      if (ss[0] == '`' && ss[1] == '`') {
        for (ss1 = ss2 + 2; *ss1; ss1++) {
          if (ss1[0] == '`' && ss1[1] == '`') {
            ss2 = ss1 + 2;
          }
        }
        while (*ss2 == ' ') {
          ++ss2;
        }
      }
      strcpy(net_email_name, ss2);
    } else {
      net_email_name[0] = '\0';
    }
  } else {
    net_email_name[0] = '\0';
  }
}

void qscan(int nBeginSubNumber, int *pnNextSubNumber) {
  int nSubNumber = usub[nBeginSubNumber].subnum;
  g_flags &= ~g_flag_made_find_str;

  if (hangup || nSubNumber < 0) {
    return;
  }
  bout.nl();
  uint32_t lQuickScanPointer = qsc_p[nSubNumber];

  if (!session()->m_SubDateCache[nSubNumber]) {
    iscan1(nSubNumber, true);
  }

  uint32_t lSubDate = session()->m_SubDateCache[nSubNumber];
  if (!lSubDate || lSubDate > lQuickScanPointer) {
    int nNextSubNumber = *pnNextSubNumber;
    int nOldSubNumber = session()->GetCurrentMessageArea();
    session()->SetCurrentMessageArea(nBeginSubNumber);

    if (!iscan(session()->GetCurrentMessageArea())) {
      bout << "\r\n\003""6A file required is in use by another instance. Try again later.\r\n";
      return;
    }
    lQuickScanPointer = qsc_p[nSubNumber];

    bout.bprintf("\r\n\n|#1< Q-scan %s %s - %lu msgs >\r\n",
                                      subboards[session()->GetCurrentReadMessageArea()].name,
                                      usub[session()->GetCurrentMessageArea()].keys, session()->GetNumMessagesInCurrentMessageArea());

    int i;
    for (i = session()->GetNumMessagesInCurrentMessageArea(); (i > 1)
         && (get_post(i - 1)->qscan > lQuickScanPointer); i--)
      ;

    if (session()->GetNumMessagesInCurrentMessageArea() > 0 && i <= session()->GetNumMessagesInCurrentMessageArea()
        && get_post(i)->qscan > qsc_p[session()->GetCurrentReadMessageArea()]) {
      scan(i, SCAN_OPTION_READ_MESSAGE, &nNextSubNumber, false);
    } else {
      unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
      qsc_p[session()->GetCurrentReadMessageArea()] = pStatus->GetQScanPointer() - 1;
    }

    session()->SetCurrentMessageArea(nOldSubNumber);
    *pnNextSubNumber = nNextSubNumber;
    bout.bprintf("|#1< %s Q-Scan Done >", subboards[session()->GetCurrentReadMessageArea()].name);
    bout.clreol();
    bout.nl();
    lines_listed = 0;
    bout.clreol();
    if (okansi() && !newline) {
      bout << "\r\x1b[4A";
    }
  } else {
    bout.bprintf("|#1< Nothing new on %s %s >", subboards[nSubNumber].name,
                                      usub[nBeginSubNumber].keys);
    bout.clreol();
    bout.nl();
    lines_listed = 0;
    bout.clreol();
    if (okansi() && !newline) {
      bout << "\r\x1b[3A";
    }
  }
  bout.nl();
}

void nscan(int nStartingSubNum) {
  int nNextSubNumber = 1;

  bout << "\r\n|#3-=< Q-Scan All >=-\r\n";
  for (int i = nStartingSubNum; usub[i].subnum != -1 && i < session()->num_subs && nNextSubNumber && !hangup; i++) {
    if (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32))) {
      qscan(i, &nNextSubNumber);
    }
    bool abort = false;
    checka(&abort);
    if (abort) {
      nNextSubNumber = 0;
    }
  }
  bout.nl();
  bout.clreol();
  bout << "|#3-=< Global Q-Scan Done >=-\r\n\n";
  if (nNextSubNumber && session()->user()->IsNewScanFiles() &&
      (syscfg.sysconfig & sysconfig_no_xfer) == 0 &&
      (!(g_flags & g_flag_scanned_files))) {
    lines_listed = 0;
    session()->tagging = 1;
    tmp_disable_conf(true);
    nscanall();
    tmp_disable_conf(false);
    session()->tagging = 0;
  }
}

void ScanMessageTitles() {
  if (!iscan(session()->GetCurrentMessageArea())) {
    bout << "\r\n|#7A file required is in use by another instance. Try again later.\r\n";
    return;
  }
  bout.nl();
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout << "No subs available.\r\n";
    return;
  }
  bout.bprintf("|#2%d |#9messages in area |#2%s\r\n",
                                    session()->GetNumMessagesInCurrentMessageArea(), subboards[session()->GetCurrentReadMessageArea()].name);
  if (session()->GetNumMessagesInCurrentMessageArea() == 0) {
    return;
  }
  bout.bprintf("|#9Start listing at (|#21|#9-|#2%d|#9): ",
                                    session()->GetNumMessagesInCurrentMessageArea());
  string messageNumber;
  input(&messageNumber, 5, true);
  int nMessageNumber = atoi(messageNumber.c_str());
  if (nMessageNumber < 1) {
    nMessageNumber = 0;
  } else if (nMessageNumber > session()->GetNumMessagesInCurrentMessageArea()) {
    nMessageNumber = session()->GetNumMessagesInCurrentMessageArea();
  } else {
    nMessageNumber--;
  }
  int nNextSubNumber = 0;
  // 'S' means start reading at the 1st message.
  if (messageNumber == "S") {
    scan(0, SCAN_OPTION_READ_PROMPT, &nNextSubNumber, true);
  } else if (nMessageNumber >= 0) {
    scan(nMessageNumber, SCAN_OPTION_LIST_TITLES, &nNextSubNumber, true);
  }
}

void delmail(File *pFile, int loc) {
  mailrec m, m1;
  WUser user;

  pFile->Seek(static_cast<long>(loc * sizeof(mailrec)), File::seekBegin);
  pFile->Read(&m, sizeof(mailrec));

  if (m.touser == 0 && m.tosys == 0) {
    return;
  }

  bool rm = true;
  if (m.status & status_multimail) {
    int t = pFile->GetLength() / sizeof(mailrec);
    int otf = false;
    for (int i = 0; i < t; i++) {
      if (i != loc) {
        pFile->Seek(static_cast<long>(i * sizeof(mailrec)), File::seekBegin);
        pFile->Read(&m1, sizeof(mailrec));
        if ((m.msg.stored_as == m1.msg.stored_as) && (m.msg.storage_type == m1.msg.storage_type) && (m1.daten != 0xffffffff)) {
          otf = true;
        }
      }
    }
    if (otf) {
      rm = false;
    }
  }
  if (rm) {
    remove_link(&m.msg, "email");
  }

  if (m.tosys == 0) {
    application()->users()->ReadUser(&user, m.touser);
    if (user.GetNumMailWaiting()) {
      user.SetNumMailWaiting(user.GetNumMailWaiting() - 1);
      application()->users()->WriteUser(&user, m.touser);
    }
    if (m.touser == 1) {
      --fwaiting;
    }
  }
  pFile->Seek(static_cast<long>(loc * sizeof(mailrec)), File::seekBegin);
  m.touser = 0;
  m.tosys = 0;
  m.daten = 0xffffffff;
  m.msg.storage_type = 0;
  m.msg.stored_as = 0xffffffff;
  pFile->Write(&m, sizeof(mailrec));
  mailcheck = true;
}

void remove_post() {
  if (!iscan(session()->GetCurrentMessageArea())) {
    bout << "\r\n|#6A file required is in use by another instance. Try again later.\r\n\n";
    return;
  }
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout << "\r\nNo subs available.\r\n\n";
    return;
  }
  bool any = false, abort = false;
  bout.bprintf("\r\n\nPosts by you on %s\r\n\n",
                                    subboards[session()->GetCurrentReadMessageArea()].name);
  for (int j = 1; j <= session()->GetNumMessagesInCurrentMessageArea() && !abort; j++) {
    if (get_post(j)->ownersys == 0 && get_post(j)->owneruser == session()->usernum) {
      any = true;
      string buffer = StringPrintf("%u: %60.60s", j, get_post(j)->title);
      pla(buffer.c_str(), &abort);
    }
  }
  if (!any) {
    bout << "None.\r\n";
    if (!cs()) {
      return;
    }
  }
  bout << "\r\n|#2Remove which? ";
  string postNumberToRemove;
  input(&postNumberToRemove, 5);
  int nPostNumber = atoi(postNumberToRemove.c_str());
  open_sub(true);
  if (nPostNumber > 0 && nPostNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
    if (((get_post(nPostNumber)->ownersys == 0) && (get_post(nPostNumber)->owneruser == session()->usernum)) || lcs()) {
      if ((get_post(nPostNumber)->owneruser == session()->usernum) && (get_post(nPostNumber)->ownersys == 0)) {
        WUser tu;
        application()->users()->ReadUser(&tu, get_post(nPostNumber)->owneruser);
        if (!tu.IsUserDeleted()) {
          if (date_to_daten(tu.GetFirstOn()) < static_cast<time_t>(get_post(nPostNumber)->daten)) {
            if (tu.GetNumMessagesPosted()) {
              tu.SetNumMessagesPosted(tu.GetNumMessagesPosted() - 1);
              application()->users()->WriteUser(&tu, get_post(nPostNumber)->owneruser);
            }
          }
        }
      }
      sysoplogf("- \"%s\" removed from %s", get_post(nPostNumber)->title,
                subboards[session()->GetCurrentReadMessageArea()].name);
      delete_message(nPostNumber);
      bout << "\r\nMessage removed.\r\n\n";
    }
  }
  close_sub();
}



