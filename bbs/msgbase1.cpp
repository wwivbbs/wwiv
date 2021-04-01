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
#include "bbs/msgbase1.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/inmsg.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/msgscan.h"
#include "bbs/netsup.h"
#include "bbs/shortmsg.h"
#include "bbs/subacc.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/input.h"
#include "common/message_editor_data.h"
#include "core/datetime.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/fido/fido_address.h"
#include "sdk/msgapi/parsed_message.h"
#include "sdk/net/ftn_msgdupe.h"
#include "sdk/net/networks.h"
#include "sdk/net/subscribers.h"
#include <memory>
#include <string>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::system_clock;
using wwiv::sdk::fido::FidoAddress;

using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

void send_net_post(postrec* pPostRecord, const subboard_t& sub) {
  auto o = readfile(&(pPostRecord->msg), sub.filename);
  if (!o) {
    return;
  }

  int netnum;
  const auto orig_netnum = a()->net_num();
  if (pPostRecord->status & status_post_new_net) {
    netnum = pPostRecord->network.network_msg.net_number;
  } else if (!sub.nets.empty()) {
    netnum = sub.nets[0].net_num;
  } else {
    netnum = a()->net_num();
  }

  const auto nn1 = netnum;
  if (pPostRecord->ownersys == 0) {
    netnum = -1;
  }

  const auto& text = o.value();
  net_header_rec nhorig = {};
  nhorig.tosys = 0;
  nhorig.touser = 0;
  nhorig.fromsys = pPostRecord->ownersys;
  nhorig.fromuser = pPostRecord->owneruser;
  nhorig.list_len = 0;
  nhorig.daten = pPostRecord->daten;
  nhorig.length = size_int(text) + 1 + strlen(pPostRecord->title);
  nhorig.method = 0;

  auto message_length = text.size();
  if (nhorig.length > 32755) {
    bout.bprintf("Message truncated by %lu bytes for the network.", nhorig.length - 32755L);
    nhorig.length = 32755;
    message_length = nhorig.length - strlen(pPostRecord->title) - 1;
  }
  std::unique_ptr<char[]> b1(new char[nhorig.length + 100]);
  strcpy(b1.get(), pPostRecord->title);
  memmove(&(b1[strlen(pPostRecord->title) + 1]), text.c_str(), message_length);

  for (const auto& xnp : sub.nets) {
    if (xnp.net_num == netnum && xnp.host) {
      continue;
    }
    set_net_num(xnp.net_num);
    const auto& net = a()->nets()[xnp.net_num];
    auto nh = nhorig;
    std::vector<uint16_t> list;
    nh.minor_type = 0;
    if (!nh.fromsys) {
      nh.fromsys = net.sysnum;
    }
    nh.main_type = main_type_new_post;
    if (xnp.host) {
      nh.tosys = xnp.host;
    } else {
      std::set<uint16_t> subscribers;
      const auto subscribers_read =
          ReadSubcriberFile(FilePath(net.dir, StrCat("n", xnp.stype, ".net")), subscribers);
      if (subscribers_read) {
        for (const auto& s : subscribers) {
          if ((a()->net_num() != netnum || nh.fromsys != s) && s != net.sysnum) {
            if (valid_system(s)) {
              nh.list_len++;
              list.push_back(s);
            }
          }
        }
      }
    }
    if (nn1 == a()->net_num()) {
      const std::string body(b1.get(), nh.length);
      send_net(&nh, list, body, xnp.stype);
    } else {
      gate_msg(&nh, b1.get(), xnp.net_num, xnp.stype, list, netnum);
    }
  }

  set_net_num(orig_netnum);
}

static std::string net_type_to_string(const network_type_t& t) {
  switch (t) {
  case network_type_t::wwivnet:
    return "WWIV";
  case network_type_t::ftn:
    return "FTN";
  case network_type_t::internet:
    return "Internet";
  case network_type_t::news:
    return "Newsgroup";
  }
  return "Unknown";
}

void post(const PostData& post_data) {
  if (!iscan(a()->current_user_sub_num())) {
    bout << "\r\n|#6A file required is in use by another instance. Try again later.\r\n";
    return;
  }
  if (a()->sess().GetCurrentReadMessageArea() < 0) {
    bout << "\r\nNo subs available.\r\n\n";
    return;
  }

  if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
    bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  if (a()->user()->restrict_post() ||
      a()->user()->posts_today() >= a()->config()->sl(a()->sess().effective_sl()).posts) {
    bout << "\r\nToo many messages posted today.\r\n\n";
    return;
  }
  if (!check_acs(a()->current_sub().post_acs)) {
    bout << "\r\nYou can't post here.\r\n\n";
    return;
  }

  MessageEditorData data(a()->user()->name_and_number());
  messagerec m{};
  m.storage_type = static_cast<unsigned char>(a()->current_sub().storage_type);
  data.anonymous_flag = a()->subs().sub(a()->sess().GetCurrentReadMessageArea()).anony & 0x0f;
  if (data.anonymous_flag == 0 && a()->config()->sl(a()->sess().effective_sl()).ability & ability_post_anony) {
    data.anonymous_flag = anony_enable_anony;
  }
  if (data.anonymous_flag == anony_enable_anony && a()->user()->restrict_anony()) {
    data.anonymous_flag = 0;
  }
  if (!a()->current_sub().nets.empty()) {
    data.anonymous_flag &= anony_real_name;
    if (a()->user()->restrict_net()) {
      bout << "\r\nYou can't post on networked sub-boards.\r\n\n";
      return;
    }
    if (a()->current_net().sysnum != 0) {
      bout << "\r\n|#9This post will go out on: ";
      for (auto i = 0; i < ssize(a()->current_sub().nets); i++) {
        if (i) {
          bout << "|#9, ";
        }
        const auto& n = a()->nets()[a()->current_sub().nets[i].net_num];
        bout << "|#2" << n.name << "|#1(" << net_type_to_string(n.type) << ")";
      }
      bout << ".\r\n\n";
    }
  }
  const auto start_time = DateTime::now().to_system_clock();

  write_inst(INST_LOC_POST, a()->sess().GetCurrentReadMessageArea(), INST_FLAGS_NONE);

  data.fsed_flags = FsedFlags::FSED;
  data.msged_flags =
      (a()->current_sub().anony & anony_no_tag) ? MSGED_FLAG_NO_TAGLINE : MSGED_FLAG_NONE;
  data.aux = a()->current_sub().filename;
  data.sub_name = a()->subs().sub(a()->current_user_sub().subnum).name;
  data.to_name = post_data.reply_to.name;
  data.need_title = true;

  if (!inmsg(data)) {
    m.stored_as = 0xffffffff;
    return;
  }

  // Additions for MSGID reply.
  if (a()->current_net().type == network_type_t::ftn && !post_data.reply_to.text.empty()) {
    // We're handling a reply.
    auto msgid = FtnMessageDupe::GetMessageIDFromWWIVText(post_data.reply_to.text);
    if (!msgid.empty()) {
      const auto address = a()->current_net().fido.fido_address;
      try {
        FidoAddress addr(address);
        add_ftn_msgid(*a()->config(), addr, msgid, &data);
      } catch (wwiv::sdk::fido::bad_fidonet_address& e) {
        LOG(ERROR) << "Exception parsing address: " << address << "; reason: " << e.what();
      }
    }
  }
  savefile(data.text, &m, data.aux);

  postrec p{};
  memset(&p, 0, sizeof(postrec));
  to_char_array(p.title, data.title);
  p.anony = static_cast<unsigned char>(data.anonymous_flag);
  p.msg = m;
  p.ownersys = 0;
  p.owneruser = static_cast<uint16_t>(a()->sess().user_num());
  a()->status_manager()->Run([&](Status& s) { p.qscan = s.next_qscanptr(); });
  p.daten = daten_t_now();
  p.status = 0;
  if (a()->user()->restrict_validate()) {
    p.status |= status_unvalidated;
  }

  open_sub(true);

  if (!a()->current_sub().nets.empty() && a()->current_sub().anony & anony_val_net &&
      (!lcs() || !post_data.reply_to.title.empty())) {
    p.status |= status_pending_net;
    auto dm = true;
    for (auto i = a()->GetNumMessagesInCurrentMessageArea();
         i >= 1 && i > a()->GetNumMessagesInCurrentMessageArea() - 28; i--) {
      if (get_post(i)->status & status_pending_net) {
        dm = false;
        break;
      }
    }
    if (dm) {
      // Only SSM if this is the one and only pending networking message.
      ssm(1) << "Unvalidated net posts on " << a()->current_sub().name << ".";
    }
  }
  if (a()->GetNumMessagesInCurrentMessageArea() >= a()->current_sub().maxmsgs) {
    int i = 1;
    int dm = 0;
    while (i <= a()->GetNumMessagesInCurrentMessageArea()) {
      auto* pp = get_post(i);
      if (!pp) {
        break;
      }
      if ((pp->status & status_no_delete) == 0 ||
          pp->msg.storage_type != a()->current_sub().storage_type) {
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

  a()->user()->messages_posted(a()->user()->messages_posted() + 1);
  a()->user()->posts_today(a()->user()->posts_today() + 1);
  a()->status_manager()->Run([](Status& s) {
    s.increment_msgs_today();
    s.IncrementNumLocalPosts();
  });

  if (a()->HasConfigFlag(OP_FLAGS_POSTTIME_COMPENSATE)) {
    const auto end_time = DateTime::now().to_system_clock();
    auto diff_time = end_time - start_time;
    const auto allowed_time_per_login = a()->config()->sl(a()->sess().effective_sl()).time_per_logon;
    if (duration_cast<minutes>(diff_time).count() > allowed_time_per_login) {
      diff_time = minutes(allowed_time_per_login);
    }
    a()->user()->add_extratime(diff_time);
  }
  close_sub();

  a()->UpdateTopScreen();
  sysoplog() << "+ '" << p.title << "' posted on " << a()->current_sub().name;
  bout << "Posted on " << a()->current_sub().name << wwiv::endl;
  if (!a()->current_sub().nets.empty()) {
    a()->user()->posts_net(a()->user()->posts_net() + 1);
    if (!(p.status & status_pending_net)) {
      send_net_post(&p, a()->current_sub());
    }
  }
}

void add_ftn_msgid(const wwiv::sdk::Config& config, const FidoAddress& addr, const std::string& msgid,
                   MessageEditorData* data) {
  FtnMessageDupe dupe(config);
  if (dupe.IsInitialized()) {
    const auto new_msgid = dupe.CreateMessageID(addr);
    WWIVParsedMessageText pmt(data->text);
    // TODO(rushfan): Should we keep removing them while they exist in case
    // there are more than 1??
    // Remove the existing MSGID lines.
    pmt.remove_control_line("MSGID");
    pmt.remove_control_line("REPLY");

    pmt.add_control_line(StrCat("MSGID: ", new_msgid));
    pmt.add_control_line_after("MSGID", StrCat("REPLY: ", msgid));
    data->text = pmt.to_string();
  }
}

std::string grab_user_name(messagerec* m, const std::string& file_name, int network_number) {
  a()->net_email_name.clear();
  auto o = readfile(m, file_name);
  if (!o) {
    return {};
  }
  auto text = o.value();
  const auto cr = text.find_first_of('\r');
  if (cr == std::string::npos) {
    return {};
  }
  text.resize(cr);
  // Don't assume that we have a valid network at the network_number position.
  if (network_number < ssize(a()->nets()) &&
      a()->nets()[network_number].type == network_type_t::ftn) {
    // 1st line of message is from.
    a()->net_email_name = text;
    return text;
  }
  const auto* ss2 = text.c_str();
  if (text[0] == '`' && text[1] == '`') {
    for (const char* ss1 = ss2 + 2; *ss1; ss1++) {
      if (ss1[0] == '`' && ss1[1] == '`') {
        ss2 = ss1 + 2;
      }
    }
    while (*ss2 == ' ') {
      ++ss2;
    }
    a()->net_email_name = ss2;
    return ss2;
  }
  return {};
}

void qscan(uint16_t start_subnum, bool& nextsub) {
  const int sub_number = a()->usub[start_subnum].subnum;

  if (a()->sess().hangup() || sub_number < 0) {
    return;
  }
  bout.nl();
  auto memory_last_read = a()->sess().qsc_p[sub_number];

  iscan1(sub_number);
  auto num_lines = 3;
  if (const auto on_disk_last_post = WWIVReadLastRead(sub_number); !on_disk_last_post || on_disk_last_post > memory_last_read) {
    const auto old_subnum = a()->current_user_sub_num();
    a()->set_current_user_sub_num(start_subnum);

    if (!iscan(a()->current_user_sub_num())) {
      bout << "\r\n\003"
              "6A file required is in use by another instance. Try again later.\r\n";
      return;
    }
    memory_last_read = a()->sess().qsc_p[sub_number];

    bout.bprintf("\r\n\n|#1< Q-scan %s %s - %lu msgs >\r\n", a()->current_sub().name,
                 a()->current_user_sub().keys, a()->GetNumMessagesInCurrentMessageArea());

    int i;
    for (i = a()->GetNumMessagesInCurrentMessageArea();
         (i > 1) && (get_post(i - 1)->qscan > memory_last_read); i--)
      ;

    if (a()->GetNumMessagesInCurrentMessageArea() > 0 &&
        i <= a()->GetNumMessagesInCurrentMessageArea() &&
        get_post(i)->qscan > a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()]) {
      scan(i, MsgScanOption::SCAN_OPTION_READ_MESSAGE, nextsub, false);
    } else {
      const auto status = a()->status_manager()->get_status();
      a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()] = status->qscanptr() - 1;
    }

    a()->set_current_user_sub_num(old_subnum);
    bout << "|#1< " << a()->current_sub().name << " Q-Scan Done >";
    num_lines = 4;
  } else if (!a()->sess().forcescansub()) {
    // No need to display this if we're in a forced nscan for this sub.
    bout << "|#1< Nothing new on " << a()->subs().sub(sub_number).name;
  }
  bout.clreol();
  bout.nl();
  bout.clear_lines_listed();
  bout.clreol();
  bout.move_up_if_newline(num_lines);
  bout.nl();
}

void nscan(uint16_t start_subnum) {
  bool nextsub = true;

  bout << "\r\n|#3-=< Q-Scan All >=-\r\n";
  for (auto i = start_subnum; i < a()->usub.size() && nextsub && !a()->sess().hangup();
       i++) {
    if (a()->sess().qsc_q[a()->usub[i].subnum / 32] & (1L << (a()->usub[i].subnum % 32))) {
      qscan(i, nextsub);
    }
    bool abort = false;
    bin.checka(&abort);
    if (abort) {
      nextsub = false;
    }
  }
  bout.nl();
  bout.clreol();
  bout << "|#3-=< Global Q-Scan Done >=-\r\n\n";
  if (nextsub && a()->user()->newscan_files() && !a()->sess().scanned_files()) {
    bout.clear_lines_listed();
    tmp_disable_conf(true);
    nscanall();
    tmp_disable_conf(false);
  }
}

void ScanMessageTitles() {
  if (!iscan(a()->current_user_sub_num())) {
    bout << "\r\n|#7A file required is in use by another instance. Try again later.\r\n";
    return;
  }
  bout.nl();
  if (a()->sess().GetCurrentReadMessageArea() < 0) {
    bout << "No subs available.\r\n";
    return;
  }
  bout.bprintf("|#2%d |#9messages in area |#2%s\r\n",
                       a()->GetNumMessagesInCurrentMessageArea(), a()->current_sub().name);
  if (a()->GetNumMessagesInCurrentMessageArea() == 0) {
    return;
  }
  bout << "|#9Start listing at (|#21|#9-|#2" << a()->GetNumMessagesInCurrentMessageArea()
       << "|#9): ";
  const auto r =
      bin.input_number_hotkey(1, {'Q', 'S'}, 1, a()->GetNumMessagesInCurrentMessageArea(), false);
  if (auto nextsub = false; r.key == 'S') {
    scan(0, MsgScanOption::SCAN_OPTION_READ_PROMPT, nextsub, true);
  } else if (r.key != 'Q') {
    scan(r.num - 1, MsgScanOption::SCAN_OPTION_LIST_TITLES, nextsub, true);
  }
}

void remove_post() {
  if (!iscan(a()->current_user_sub_num())) {
    bout << "\r\n|#6A file required is in use by another instance. Try again later.\r\n\n";
    return;
  }
  if (a()->sess().GetCurrentReadMessageArea() < 0) {
    bout << "\r\nNo subs available.\r\n\n";
    return;
  }
  bool any = false, abort = false;
  bout << "\r\n\nPosts by you on " << a()->current_sub().name << "\r\n\n";
  for (int j = 1; j <= a()->GetNumMessagesInCurrentMessageArea() && !abort; j++) {
    if (get_post(j)->ownersys == 0 && get_post(j)->owneruser == a()->sess().user_num()) {
      any = true;
      bout.bpla(fmt::sprintf("%u: %60.60s", j, get_post(j)->title), &abort);
    }
  }
  if (!any) {
    bout << "None.\r\n";
    if (!cs()) {
      return;
    }
  }
  bout << "\r\n|#2Remove which? ";
  const auto postnum = bin.input_number(0, 0, a()->GetNumMessagesInCurrentMessageArea(), false);
  OpenSub opened_sub(true);
  if (postnum > 0 && postnum <= a()->GetNumMessagesInCurrentMessageArea()) {
    if (get_post(postnum)->ownersys == 0 && get_post(postnum)->owneruser == a()->sess().user_num() ||
        lcs()) {
      if (get_post(postnum)->owneruser == a()->sess().user_num() && get_post(postnum)->ownersys == 0) {
        User tu;
        a()->users()->readuser(&tu, get_post(postnum)->owneruser);
        if (!tu.deleted()) {
          if (date_to_daten(tu.firston()) < get_post(postnum)->daten) {
            if (tu.messages_posted()) {
              tu.messages_posted(tu.messages_posted() - 1);
              a()->users()->writeuser(&tu, get_post(postnum)->owneruser);
            }
          }
        }
      }
      sysoplog() << "- '" << get_post(postnum)->title << "' removed from "
                 << a()->current_sub().name;
      delete_message(postnum);
      bout << "\r\nMessage removed.\r\n\n";
    }
  }
}
