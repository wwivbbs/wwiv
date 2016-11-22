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
#include "bbs/msgbase1.h"

#include <memory>
#include <string>

#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/netsup.h"
#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/sysoplog.h"
#include "bbs/wconstants.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/datetime.h"
#include "sdk/status.h"
#include "sdk/subscribers.h"
#include "sdk/subxtr.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void send_net_post(postrec* pPostRecord, const subboard_t& sub) {
  string text;
  if (!readfile(&(pPostRecord->msg), sub.filename, &text)){
    return;
  }

  int nNetNumber;
  int nOrigNetNumber = session()->net_num();
  if (pPostRecord->status & status_post_new_net) {
    nNetNumber = pPostRecord->network.network_msg.net_number;
  } else if (!sub.nets.empty()) {
    nNetNumber = sub.nets[0].net_num;
  } else {
    nNetNumber = session()->net_num();
  }

  int nn1 = nNetNumber;
  if (pPostRecord->ownersys == 0) {
    nNetNumber = -1;
  }

  net_header_rec nhorig = {};
  nhorig.tosys   = 0;
  nhorig.touser  = 0;
  nhorig.fromsys = pPostRecord->ownersys;
  nhorig.fromuser  = pPostRecord->owneruser;
  nhorig.list_len  = 0;
  nhorig.daten   = pPostRecord->daten;
  nhorig.length  = text.size() + 1 + strlen(pPostRecord->title);
  nhorig.method  = 0;

  uint32_t message_length = text.size();
  if (nhorig.length > 32755) {
    bout.bprintf("Message truncated by %lu bytes for the network.", nhorig.length - 32755L);
    nhorig.length = 32755;
    message_length = nhorig.length - strlen(pPostRecord->title) - 1;
  }
  unique_ptr<char[]> b1(new char[nhorig.length + 100]);
  if (!b1) {
    set_net_num(nOrigNetNumber);
    return;
  }
  strcpy(b1.get(), pPostRecord->title);
  memmove(&(b1[strlen(pPostRecord->title) + 1]), text.c_str(), message_length);

  for (size_t n = 0; n < sub.nets.size(); n++) {
    const auto& xnp = sub.nets[n];
    if (xnp.net_num == nNetNumber && xnp.host) {
      continue;
    }
    set_net_num(xnp.net_num);
    net_header_rec nh = nhorig;
    std::vector<uint16_t> list;
    nh.minor_type = 0;
    if (!nh.fromsys) {
      nh.fromsys = session()->current_net().sysnum;
    }
    nh.main_type = main_type_new_post;
    if (xnp.host) {
      nh.tosys = xnp.host;
    } else {
      std::set<uint16_t> subscribers;
      bool subscribers_read = ReadSubcriberFile(session()->network_directory(), StrCat("n", xnp.stype, ".net"), subscribers);
      if (subscribers_read) {
        for (const auto& s : subscribers) {
          if (((session()->net_num() != nNetNumber) || (nh.fromsys != s)) && (s != session()->current_net().sysnum)) {
            if (valid_system(s)) {
              nh.list_len++;
              list.push_back(s);
            }
          }
        }
      }
    }
    if (nn1 == session()->net_num()) {
      const string body(b1.get(), nh.length);
      send_net(&nh, list, body, xnp.stype);
    } else {
      gate_msg(&nh, b1.get(), xnp.net_num, xnp.stype, list, nNetNumber);
    }
  }

  set_net_num(nOrigNetNumber);
}

void post() {
  if (!iscan(session()->current_user_sub_num())) {
    bout << "\r\n|#6A file required is in use by another instance. Try again later.\r\n";
    return;
  }
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout << "\r\nNo subs available.\r\n\n";
    return;
  }

  if (File::GetFreeSpaceForPath(session()->config()->msgsdir()) < 10) {
    bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  if (session()->user()->IsRestrictionPost()
      || session()->user()->GetNumPostsToday() >= getslrec(session()->GetEffectiveSl()).posts) {
    bout << "\r\nToo many messages posted today.\r\n\n";
    return;
  }
  if (session()->GetEffectiveSl() < session()->current_sub().postsl) {
    bout << "\r\nYou can't post here.\r\n\n";
    return;
  }

  MessageEditorData data;
  messagerec m;
  m.storage_type = static_cast<unsigned char>(session()->current_sub().storage_type);
  data.anonymous_flag = session()->subs().sub(session()->GetCurrentReadMessageArea()).anony & 0x0f;
  if (data.anonymous_flag == 0 && getslrec(session()->GetEffectiveSl()).ability & ability_post_anony) {
    data.anonymous_flag = anony_enable_anony;
  }
  if (data.anonymous_flag == anony_enable_anony && session()->user()->IsRestrictionAnonymous()) {
    data.anonymous_flag = 0;
  }
  if (!session()->current_sub().nets.empty()) {
    data.anonymous_flag &= (anony_real_name);
    if (session()->user()->IsRestrictionNet()) {
      bout << "\r\nYou can't post on networked sub-boards.\r\n\n";
      return;
    }
    if (session()->current_net().sysnum != 0) {
      bout << "\r\n|#9This post will go out on: ";
      for (size_t i = 0; i < session()->current_sub().nets.size(); i++) {
        if (i) {
          bout << "|#9, ";
        }
        const auto& n = session()->net_networks[session()->current_sub().nets[i].net_num];
        bout << "|#2" << n.name << "|#1";
        if (n.type == network_type_t::wwivnet) {
          bout << "|#1 (WWIV)";
        } else if (n.type == network_type_t::ftn) {
          bout << "|#1 (FTN)";
        } else if (n.type == network_type_t::internet) {
          bout << "|#1 (Internet)";
        }
      }
      bout << ".\r\n\n";
    }
  }
  time_t start_time = time(nullptr);

  write_inst(INST_LOC_POST, session()->GetCurrentReadMessageArea(), INST_FLAGS_NONE);

  data.fsed_flags = FsedFlags::FSED;
  data.msged_flags = (session()->current_sub().anony & anony_no_tag) ? MSGED_FLAG_NO_TAGLINE : MSGED_FLAG_NONE;
  data.aux = session()->current_sub().filename;
  data.to_name = session()->current_sub().name;
  data.need_title = true;

  if (!inmsg(data)) {
    m.stored_as = 0xffffffff;
    return;
  }
  savefile(data.text, &m, data.aux);

  postrec p{};
  memset(&p, 0, sizeof(postrec));
  strcpy(p.title, data.title.c_str());
  p.anony = static_cast<unsigned char>(data.anonymous_flag);
  p.msg = m;
  p.ownersys  = 0;
  p.owneruser = static_cast<uint16_t>(session()->usernum);
  WStatus* pStatus = session()->status_manager()->BeginTransaction();
  p.qscan = pStatus->IncrementQScanPointer();
  session()->status_manager()->CommitTransaction(pStatus);
  p.daten = wwiv::sdk::time_t_to_daten(time(nullptr));
  p.status = 0;
  if (session()->user()->IsRestrictionValidate()) {
    p.status |= status_unvalidated;
  }

  open_sub(true);

  if ((!session()->current_sub().nets.empty()) &&
      (session()->current_sub().anony & anony_val_net) && (!lcs() || irt[0])) {
    p.status |= status_pending_net;
    bool dm = true;
    for (int i = session()->GetNumMessagesInCurrentMessageArea(); (i >= 1)
          && (i > (session()->GetNumMessagesInCurrentMessageArea() - 28)); i--) {
      if (get_post(i)->status & status_pending_net) {
        dm = false;
        break;
      }
    }
    if (dm) {
      ssm(1, 0) << "Unvalidated net posts on " << session()->current_sub().name << ".";
    }
  }
  if (session()->GetNumMessagesInCurrentMessageArea() >=
    session()->current_sub().maxmsgs) {
    int i = 1;
    int dm = 0;
    while (i <= session()->GetNumMessagesInCurrentMessageArea()) {
      postrec* pp = get_post(i);
      if (!pp) {
        break;
      } else if (((pp->status & status_no_delete) == 0) ||
                  (pp->msg.storage_type != session()->current_sub().storage_type)) {
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
  pStatus = session()->status_manager()->BeginTransaction();
  pStatus->IncrementNumMessagesPostedToday();
  pStatus->IncrementNumLocalPosts();

  if (session()->HasConfigFlag(OP_FLAGS_POSTTIME_COMPENSATE)) {
    time_t lEndTime = time(nullptr);
    if (start_time > lEndTime) {
      lEndTime += SECONDS_PER_DAY;
    }
    start_time = static_cast<long>(lEndTime - start_time);
    if ((start_time / MINUTES_PER_HOUR) > getslrec(session()->GetEffectiveSl()).time_per_logon) {
      start_time = static_cast<long>(static_cast<float>(getslrec(session()->GetEffectiveSl()).time_per_logon *
                                      MINUTES_PER_HOUR));
    }
    session()->user()->SetExtraTime(session()->user()->GetExtraTime() + static_cast<float>
        (start_time));
  }
  session()->status_manager()->CommitTransaction(pStatus);
  close_sub();

  session()->UpdateTopScreen();
  sysoplog() << "+ '" << p.title << "' posted on " << session()->current_sub().name;
  bout << "Posted on " << session()->current_sub().name << wwiv::endl;
  if (!session()->current_sub().nets.empty()) {
    session()->user()->SetNumNetPosts(session()->user()->GetNumNetPosts() + 1);
    if (!(p.status & status_pending_net)) {
      send_net_post(&p, session()->current_sub());
    }
  }
}

void grab_user_name(messagerec* pMessageRecord, const std::string& file_name) {
  string text;
  session()->net_email_name.clear();
  if (!readfile(pMessageRecord, file_name, &text)) {
    return;
  }
  string::size_type cr = text.find_first_of('\r');
  if (cr == string::npos) {
    return;
  }
  text.resize(cr);
  const char* ss2 = text.c_str();
  if (text[0] == '`' && text[1] == '`') {
    for (const char* ss1 = ss2 + 2; *ss1; ss1++) {
      if (ss1[0] == '`' && ss1[1] == '`') {
        ss2 = ss1 + 2;
      }
    }
    while (*ss2 == ' ') {
      ++ss2;
    }
    session()->net_email_name = ss2;
  }
}

void qscan(int nBeginSubNumber, int *pnNextSubNumber) {
  int sub_number = session()->usub[nBeginSubNumber].subnum;
  g_flags &= ~g_flag_made_find_str;

  if (hangup || sub_number < 0) {
    return;
  }
  bout.nl();
  uint32_t memory_last_read = qsc_p[sub_number];

  iscan1(sub_number);

  uint32_t on_disk_last_post = WWIVReadLastRead(sub_number);
  if (!on_disk_last_post || on_disk_last_post > memory_last_read) {
    int nNextSubNumber = *pnNextSubNumber;
    int nOldSubNumber = session()->current_user_sub_num();
    session()->set_current_user_sub_num(nBeginSubNumber);

    if (!iscan(session()->current_user_sub_num())) {
      bout << "\r\n\003""6A file required is in use by another instance. Try again later.\r\n";
      return;
    }
    memory_last_read = qsc_p[sub_number];

    bout.bprintf("\r\n\n|#1< Q-scan %s %s - %lu msgs >\r\n",
                 session()->current_sub().name.c_str(),
                 session()->current_user_sub().keys,
                 session()->GetNumMessagesInCurrentMessageArea());

    int i;
    for (i = session()->GetNumMessagesInCurrentMessageArea(); (i > 1)
         && (get_post(i - 1)->qscan > memory_last_read); i--)
      ;

    if (session()->GetNumMessagesInCurrentMessageArea() > 0
        && i <= session()->GetNumMessagesInCurrentMessageArea()
        && get_post(i)->qscan > qsc_p[session()->GetCurrentReadMessageArea()]) {
      scan(i, SCAN_OPTION_READ_MESSAGE, &nNextSubNumber, false);
    } else {
      unique_ptr<WStatus> pStatus(session()->status_manager()->GetStatus());
      qsc_p[session()->GetCurrentReadMessageArea()] = pStatus->GetQScanPointer() - 1;
    }

    session()->set_current_user_sub_num(nOldSubNumber);
    *pnNextSubNumber = nNextSubNumber;
    bout << "|#1< " << session()->current_sub().name << " Q-Scan Done >";
    bout.clreol();
    bout.nl();
    bout.clear_lines_listed();
    bout.clreol();
    if (okansi() && !newline) {
      bout << "\r\x1b[4A";
    }
  } else {
    bout << "|#1< Nothing new on " << session()->subs().sub(sub_number).name
      << " " << session()->usub[nBeginSubNumber].keys;
    bout.clreol();
    bout.nl();
    bout.clear_lines_listed();
    bout.clreol();
    if (okansi() && !newline) {
      bout << "\r\x1b[3A";
    }
  }
  bout.nl();
}

void nscan(int nStartingSubNum) {
  int nNextSubNumber = 1;

  if (nStartingSubNum < 0) {
    // TODO(rushfan): Log error?
    return;
  }
  bout << "\r\n|#3-=< Q-Scan All >=-\r\n";
  for (size_t i = nStartingSubNum; 
       session()->usub[i].subnum != -1 && i < session()->subs().subs().size() && nNextSubNumber && !hangup;
       i++) {
    if (qsc_q[session()->usub[i].subnum / 32] & (1L << (session()->usub[i].subnum % 32))) {
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
    bout.clear_lines_listed();
    tmp_disable_conf(true);
    nscanall();
    tmp_disable_conf(false);
  }
}

void ScanMessageTitles() {
  if (!iscan(session()->current_user_sub_num())) {
    bout << "\r\n|#7A file required is in use by another instance. Try again later.\r\n";
    return;
  }
  bout.nl();
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout << "No subs available.\r\n";
    return;
  }
  bout.bprintf("|#2%d |#9messages in area |#2%s\r\n",
               session()->GetNumMessagesInCurrentMessageArea(),
               session()->current_sub().name.c_str());
  if (session()->GetNumMessagesInCurrentMessageArea() == 0) {
    return;
  }
  bout << "|#9Start listing at (|#21|#9-|#2"
       << session()->GetNumMessagesInCurrentMessageArea() << "|#9): ";
  string messageNumber = input(5, true);
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

void remove_post() {
  if (!iscan(session()->current_user_sub_num())) {
    bout << "\r\n|#6A file required is in use by another instance. Try again later.\r\n\n";
    return;
  }
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout << "\r\nNo subs available.\r\n\n";
    return;
  }
  bool any = false, abort = false;
  bout.bprintf("\r\n\nPosts by you on %s\r\n\n",
    session()->current_sub().name.c_str());
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
  string postNumberToRemove = input(5);
  int nPostNumber = atoi(postNumberToRemove.c_str());
  wwiv::bbs::OpenSub opened_sub(true);
  if (nPostNumber > 0 && nPostNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
    if (((get_post(nPostNumber)->ownersys == 0) && (get_post(nPostNumber)->owneruser == session()->usernum)) || lcs()) {
      if ((get_post(nPostNumber)->owneruser == session()->usernum) && (get_post(nPostNumber)->ownersys == 0)) {
        User tu;
        session()->users()->ReadUser(&tu, get_post(nPostNumber)->owneruser);
        if (!tu.IsUserDeleted()) {
          if (date_to_daten(tu.GetFirstOn()) < static_cast<time_t>(get_post(nPostNumber)->daten)) {
            if (tu.GetNumMessagesPosted()) {
              tu.SetNumMessagesPosted(tu.GetNumMessagesPosted() - 1);
              session()->users()->WriteUser(&tu, get_post(nPostNumber)->owneruser);
            }
          }
        }
      }
      sysoplog() << "- '" << get_post(nPostNumber)->title << "' removed from " << session()->current_sub().name;
      delete_message(nPostNumber);
      bout << "\r\nMessage removed.\r\n\n";
    }
  }
}



