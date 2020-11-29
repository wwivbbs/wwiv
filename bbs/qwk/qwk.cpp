/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/qwk/qwk.h"

#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "qwk_config.h"
#include "qwk_email.h"
#include "qwk_reply.h"
#include "qwk_ui.h"
#include "qwk_util.h"
#include "bbs/read_message.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/execexternal.h"
#include "bbs/instmsg.h"
#include "bbs/save_qscan.h"
#include "bbs/sr.h"
#include "bbs/stuffin.h"
#include "bbs/subacc.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "common/com.h"
#include "common/input.h"
#include "common/pause.h"
#include "core/clock.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivport.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/qscan.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "sdk/ansi/makeansi.h"
#include <memory>
#include <string>

#define qwk_iscan(x)         (iscan1(a()->usub[x].subnum))

using std::unique_ptr;
using std::string;
using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;

namespace wwiv::bbs::qwk {

// Also used in qwk1.cpp
const char *QWKFrom = "\x04""0QWKFrom:";

static uint16_t max_msgs;

// from xfer.cpp
extern uint32_t this_date;


static bool replacefile(const std::string& src, const std::string& dst) {
  if (dst.empty()) {
    return false;
  }
  return File::Copy(src, dst);
}

bool build_control_dat(const qwk_config& qwk_cfg, Clock* clock, qwk_junk *qwk_info) {
  const auto date_time = clock->Now().to_string("%m-%d-%Y,%H:%M:%S"); // 'mm-dd-yyyy,hh:mm:ss'

  TextFile fp(FilePath(a()->sess().dirs().qwk_directory(), "CONTROL.DAT"), "wd");
  if (!fp) {
    return false;
  }

  const auto system_name = qwk_system_name(qwk_cfg);
  fp.WriteLine(fmt::format("{}.qwk", system_name));
  fp.WriteLine();  // System City and State
  fp.WriteLine(a()->config()->system_phone());
  fp.WriteLine(a()->config()->sysop_name());
  fp.WriteLine(fmt::format("00000,{}", system_name));
  fp.WriteLine(date_time);
  fp.WriteLine(a()->user()->GetName());
  fp.WriteLine("");
  fp.WriteLine("0");
  fp.WriteLine(qwk_info->qwk_rec_num);
  
  const auto max_size = a()->subs().subs().size();
  const qscan_bitset qb(a()->sess().qsc_q, max_size);
  std::vector<std::pair<int, std::string>> subs_list;
  for (auto cur = 0; cur < size_int(a()->usub); cur++) {
    const auto subnum = a()->usub[cur].subnum;
    if (qb.test(subnum)) {
      // QWK support says this should be truncated to 10 or 13 characters however QWKE allows for
      // 255 characters. This works fine in multimail which is the only still maintained QWK
      //  reader that I'm aware of at this time, so we'll allow it to be the full length.
      subs_list.emplace_back(subnum + 1, stripcolors(a()->subs().sub(subnum).name));
    }
  }
  fp.WriteLine(subs_list.size());
  
  fp.WriteLine("0");
  fp.WriteLine("E-Mail");

  for (const auto& s : subs_list) {
  // Write the subs in the format of "Sub Number\r\nSub Name\r\n"
    fp.WriteLine(s.first);
    fp.WriteLine(s.second);
  }

  fp.WriteLine(qwk_cfg.hello);
  fp.WriteLine(qwk_cfg.news);
  fp.WriteLine(qwk_cfg.bye);
  return fp.Close();
}

void build_qwk_packet() {
  auto save_conf = false;
  SaveQScanPointers save_qscan;

  remove_from_temp("*.*", a()->sess().dirs().qwk_directory(), false);

  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
    save_conf = true;
    tmp_disable_conf(true);
  }
  TempDisablePause disable_pause(bout);

  auto qwk_cfg = read_qwk_cfg();
  max_msgs = qwk_cfg.max_msgs;
  if (a()->user()->data.qwk_max_msgs < max_msgs && a()->user()->data.qwk_max_msgs) {
    max_msgs = a()->user()->data.qwk_max_msgs;
  }

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = daten_t_now();
  }

  ++qwk_cfg.timesd;
  write_qwk_cfg(qwk_cfg);

  write_inst(INST_LOC_QWK, a()->current_user_sub().subnum, INST_FLAGS_ONLINE);

  const auto filename = FilePath(a()->sess().dirs().batch_directory(), MESSAGES_DAT).string();
  qwk_junk qwk_info{};
  qwk_info.file = open(filename.c_str(), O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

  if (qwk_info.file < 1) {
    bout.bputs("Open error");
    sysoplog() << "Couldn't open MESSAGES.DAT";
    return;
  }

  // Setup index and other values
  qwk_info.index = -1;
  qwk_info.cursub = -1;

  qwk_info.in_email = 0;
  qwk_info.personal = -1;
  qwk_info.zero = -1;

  // Required header at the start of MESSAGES.DAT
  write(qwk_info.file, "Produced by Qmail...Copyright (c) 1987 by Sparkware.  All Rights Reserved (For Compatibility with Qmail)                        ", 128);

  // Logical record number
  qwk_info.qwk_rec_num = 1;
  qwk_info.qwk_rec_pos = 2;

  qwk_info.abort = 0;

  if (!a()->user()->data.qwk_dont_scan_mail && !qwk_info.abort) {
    qwk_gather_email(&qwk_info);
  }

  bin.checka(&qwk_info.abort);

  bout.cls();
  if (!qwk_info.abort) {
    bout << "|#7\xDA" << string(4, '\xC4') << '\xC2' << string(60, '\xC4') << '\xC2' << string(5, '\xC4') 
      << '\xC2' << string(4, '\xC4') << '\xBF' << wwiv::endl;
  }

  bin.checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    bout << "|#7\xB3|#2Sub |#7\xB3|#3Sub Name" << string(52, ' ') << "|#7\xB3|#8Total|#7\xB3|#5New |#7\xB3" << wwiv::endl;
  }

  bin.checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    bout << "|#7" << "\xC3" << string(4, '\xC4')
         << '\xC5' << string(60, '\xC4') 
         << '\xC5' << string(5, '\xC4') 
         << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  }

  bool msgs_ok = true;
  for (uint16_t i = 0; i < a()->usub.size() && !a()->sess().hangup() && !qwk_info.abort && msgs_ok; i++) {
    msgs_ok = max_msgs ? qwk_info.qwk_rec_num <= max_msgs : true;
    if (a()->sess().qsc_q[a()->usub[i].subnum / 32] & (1L << (a()->usub[i].subnum % 32))) {
      qwk_gather_sub(i, &qwk_info);
    }
  }

  bout << "|#7\xC3" << string(4, '\xC4') << '\xC5' << string(60, '\xC4') << '\xC5' << string(5, '\xC4') 
      << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  bout.nl(2);

  if (qwk_info.abort) {
    bout.Color(1);
    bout << "Abort everything? (NO=Download what I have gathered)";
    if (!bin.yesno()) {
      qwk_info.abort = 0;
    }
  }

  if (qwk_info.file != -1) {
    qwk_info.file = close(qwk_info.file);
  }
  if (qwk_info.index != -1) {
    qwk_info.index = close(qwk_info.index);
  }
  if (qwk_info.personal != -1) {
    qwk_info.personal = close(qwk_info.personal);
  }
  if (qwk_info.zero != -1) {
    qwk_info.zero = close(qwk_info.zero);
  }
  if (!qwk_info.abort) {
    SystemClock clock{};
    build_control_dat(qwk_cfg, &clock, &qwk_info);
  }
  if (!qwk_info.abort) {
    finish_qwk(&qwk_info);
  }

  // Restore on a()->sess().hangup() too, someone might have hungup in the middle of building the list
  if (qwk_info.abort || a()->user()->data.qwk_dontsetnscan || a()->sess().hangup()) {
    save_qscan.restore();
    if (qwk_info.abort) {
      sysoplog() << "Aborted";
    }
  }
  if (a()->user()->data.qwk_delete_mail && !qwk_info.abort) {
    qwk_remove_email();  // Delete email
  }

  if (save_conf) {
    tmp_disable_conf(false);
  }
}

void qwk_gather_sub(uint16_t bn, struct qwk_junk *qwk_info) {

  const auto sn = a()->usub[bn].subnum;

  if (a()->sess().hangup() || (sn < 0)) {
    return;
  }

  auto qscnptrx = a()->sess().qsc_p[sn];
  const auto sd = WWIVReadLastRead(sn);

  if (!sd || sd > qscnptrx) {
    const auto os = a()->current_user_sub_num();
    a()->set_current_user_sub_num(bn);
    int i;

    // Get total amount of messages in base
    if (!qwk_iscan(a()->current_user_sub_num())) {
      return;
    }

    qscnptrx = a()->sess().qsc_p[sn];

    // Find out what message number we are on
    for (i = a()->GetNumMessagesInCurrentMessageArea(); (i > 1) && (get_post(i - 1)->qscan > qscnptrx); i--)
      ;

    char thissub[81];
    to_char_array(thissub, a()->current_sub().name);
    thissub[60] = 0;
    const auto subinfo = fmt::sprintf("|#7\xB3|#9%-4d|#7\xB3|#1%-60s|#7\xB3 |#2%-4d|#7\xB3|#3%-4d|#7\xB3",
                                      bn + 1, thissub, a()->GetNumMessagesInCurrentMessageArea(),
                                      a()->GetNumMessagesInCurrentMessageArea() - i + 1);
    bout.bputs(subinfo);
    bout.nl();

    bin.checka(&qwk_info->abort);

    if ((a()->GetNumMessagesInCurrentMessageArea() > 0)
        && (i <= a()->GetNumMessagesInCurrentMessageArea()) && !qwk_info->abort) {
      if (get_post(i)->qscan > a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()]) {
        qwk_start_read(i, qwk_info);  // read messsage
      }
    }

    const auto status = a()->status_manager()->GetStatus();
    a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()] =
        status->GetQScanPointer() - 1;
    a()->set_current_user_sub_num(os);
  } 
  bout.Color(0);
}

void qwk_start_read(int msgnum, struct qwk_junk *qwk_info) {
  a()->sess().clear_irt();

  if (a()->sess().GetCurrentReadMessageArea() < 0) {
    return;
  }
  // Used to be inside do loop
  if (!a()->current_sub().nets.empty()) {
    set_net_num(a()->current_sub().nets[0].net_num);
  } else {
    set_net_num(0);
  }

  int amount = 1;
  bool done = false;
  do {
    if ((msgnum > 0) && (msgnum <= a()->GetNumMessagesInCurrentMessageArea())) {
      make_pre_qwk(msgnum, qwk_info);
    }
    ++msgnum;
    if (msgnum > a()->GetNumMessagesInCurrentMessageArea()) {
      done = true;
    }
    if (a()->user()->data.qwk_max_msgs_per_sub ? amount >
        a()->user()->data.qwk_max_msgs_per_sub : 0) {
      done = true;
    }
    if (max_msgs ? qwk_info->qwk_rec_num > max_msgs : 0) {
      done = true;
    }
    ++amount;
    bin.checka(&qwk_info->abort);

  } while (!done && !a()->sess().hangup() && !qwk_info->abort);
  bout.bputch('\r');
}

void make_pre_qwk(int msgnum, struct qwk_junk *qwk_info) {
  auto* p = get_post(msgnum);
  if ((p->status & (status_unvalidated | status_delete)) && !lcs()) {
    return;
  }

  const auto nn = a()->net_num();
  if (p->status & status_post_new_net) {
    set_net_num(p->network.network_msg.net_number);
  }

  put_in_qwk(p, a()->current_sub().filename.c_str(), msgnum, qwk_info);
  if (nn != a()->net_num()) {
    set_net_num(nn);
  }

  a()->user()->SetNumMessagesRead(a()->user()->GetNumMessagesRead() + 1);
  a()->SetNumMessagesReadThisLogon(a()->GetNumMessagesReadThisLogon() + 1);

  if (p->qscan >
      a()->sess()
          .qsc_p[a()->sess().GetCurrentReadMessageArea()]) { // Update qscan pointer right here
    a()->sess().qsc_p[a()->sess().GetCurrentReadMessageArea()] = p->qscan; // And here
  }
}


static void insert_after_routing(std::string& text, const std::string& text2insert) {
  const auto text_to_insert_nc = StrCat(stripcolors(text2insert), "\xE3\xE3");

  size_t pos = 0;
  const auto len = text.size();
  while (pos < len && text[pos] != 0) {
    if (text[pos] == 4 && text[pos + 1] == '0') {
      while (pos < len && text[pos] != '\xE3') {
        ++pos;
      }

      if (text[pos] == '\xE3') {
        ++pos;
      }
    } else if (pos < len) {
      text.insert(pos, text_to_insert_nc);
      return;
    }
  }
}

// Give us 3000 extra bytes to play with in the message text
static constexpr int PAD_SPACE = 3000;

// Takes text, deletes all ascii '10' and converts '13' to '227' (ã)
// And does other conversions as specified
// TODO(rushfan): This whole thing needs to be redone.
static std::string make_qwk_ready(const std::string& text, const std::string& address) {
  std::string::size_type pos = 0;

  std::string temp;
  temp.reserve(text.size() + PAD_SPACE + 1);

  while (pos < text.size()) {
    const auto x = static_cast<unsigned char>(text[pos]);
    const auto xo = text[pos];
    if (x == 0) {
      break;
    }
    if (x == 13) {
      temp.push_back('\xE3');
      ++pos;
    } else if (x == 10 || x < 3) {
      // Strip out Newlines, NULLS, 1's and 2's
      ++pos;
    } else if (a()->user()->data.qwk_remove_color && x == 3) {
      pos += 2;
    } else if (a()->user()->data.qwk_convert_color && x == 3) {
      const auto save_curatr = bout.curatr();
      bout.curatr(255);

      const auto ansi_string = wwiv::sdk::ansi::makeansi(text[pos + 1] - '0', bout.curatr());
      temp.append(ansi_string);

      pos += 2;
      bout.SystemColor(save_curatr);
    } else if (a()->user()->data.qwk_keep_routing == false && x == 4 && text[pos + 1] == '0') {
      if (text[pos + 1] == 0) {
        ++pos;
      } else { 
        while (text[pos] != '\xE3' && text[pos] != '\r' && pos < text.size() && text[pos] != 0) {
          ++pos;
        }
      }
      ++pos;
      if (pos < text.size() && text[pos] == '\n') {
        ++pos;
      }
    } else if (x == 4 && text[pos + 1] != '0') {
      pos += 2;
    } else {
      temp.push_back(xo);
      ++pos;
    }
  }

  // Only add address if it does not yet exist
  if (temp.find("QWKFrom:") != std::string::npos) {
    // Don't search for diamond or number, just text after that
    insert_after_routing(temp, address);
  }

  temp.shrink_to_fit();
  return temp;
}

static void qwk_remove_null(char *memory, int size) {
  int pos = 0;

  while (pos < size && !a()->sess().hangup()) {
    if (memory[pos] == 0) {
      (memory)[pos] = ' ';
    }
    ++pos;
  }
}

void put_in_qwk(postrec *m1, const char *fn, int msgnum, struct qwk_junk *qwk_info) {
  if (m1->status & (status_unvalidated | status_delete)) {
    if (!lcs()) {
      return;
    }
  }
  memset(&qwk_info->qwk_rec, ' ', sizeof(qwk_info->qwk_rec));

  auto m = read_type2_message(&m1->msg, static_cast<char>(m1->anony & 0x0f), true,
                              fn, m1->ownersys, m1->owneruser);

  int cur = 0;
  if (m.message_text.empty() && m.title.empty()) {
    // TODO(rushfan): Really read_type2_message should return an std::optional<Type2MessageData>
    bout << "File not found.";
    bout.nl();
    return;
  }

  auto len = m.message_text.length();
  if (len <= 0) {
    std::cout << "we have no text for this message." << std::endl;
    return;
  }
  auto ss = m.message_text;
  auto n = m.from_user_name;
  auto d = m.date;

  auto qwk_address = StrCat(QWKFrom, n);  // Copy wwivnet address to qwk_address
  if (qwk_address.find('@') != std::string::npos) {
    qwk_address.append(fmt::format("@{}", m1->ownersys));
  }

  // Took the annonomouse stuff out right here
  if (!qwk_info->in_email) {
    // Maybe m.to_user_name is valid here?
    if (!m.to_user_name.empty()) {
    strncpy(qwk_info->qwk_rec.to, m.to_user_name.c_str(), 25);
    } else {
      memcpy(qwk_info->qwk_rec.to, "ALL", 3);
    }
  } else {
    auto temp = ToStringUpperCase(a()->user()->GetName());
    strncpy(qwk_info->qwk_rec.to, temp.c_str(), 25);
  }
  auto temp_from = ToStringUpperCase(stripcolors(n));
  strncpy(qwk_info->qwk_rec.from, temp_from.c_str(), 25);

  auto dt = DateTime::from_daten(m1->daten);
  auto date = dt.to_string("%m-%d-%y");
  // to_char_array uses strncpy_safe which expects buffer to be big enough for
  // a trailing 0.  Use memcpy instead.
  memcpy(qwk_info->qwk_rec.date, date.c_str(), sizeof(qwk_info->qwk_rec.date));

  ss = make_qwk_ready(ss, qwk_address);
  // Update length since we don't do it in make_qwk_ready anymore.
  len = ss.length();
  auto amount_blocks = static_cast<int>(len / sizeof(qwk_info->qwk_rec) + 2);

  // Save Qwk Record
  sprintf(qwk_info->qwk_rec.amount_blocks, "%d", amount_blocks);
  sprintf(qwk_info->qwk_rec.msgnum, "%d", msgnum);

  if (!qwk_info->in_email) {
    strncpy(qwk_info->qwk_rec.subject, stripcolors(m1->title), 25);
  } else {
    strncpy(qwk_info->qwk_rec.subject,qwk_info->email_title, 25);
  }

  qwk_remove_null(reinterpret_cast<char*>(&qwk_info->qwk_rec), 123);
  qwk_info->qwk_rec.conf_num = a()->current_user_sub().subnum + 1;
  qwk_info->qwk_rec.logical_num = qwk_info->qwk_rec_num;

  if (write(qwk_info->file, &qwk_info->qwk_rec, sizeof(qwk_info->qwk_rec)) != sizeof(qwk_info->qwk_rec)) {
    qwk_info->abort = true; // Must be out of disk space
    bout.bputs("Write error");
    bout.pausescr();
  }

  // Save Qwk NDX
  qwk_info->qwk_ndx.pos = static_cast<float>(qwk_info->qwk_rec_pos);
  float msbin = 0.0f;
  _fieeetomsbin(&qwk_info->qwk_ndx.pos, &msbin);
  qwk_info->qwk_ndx.pos = msbin;
  qwk_info->qwk_ndx.nouse = 0;

  if (!qwk_info->in_email) { // Only if currently doing messages...
    // Create new index if it hasnt been already
    if (a()->current_user_sub_num() != static_cast<unsigned int>(qwk_info->cursub) || qwk_info->index < 0) {
      qwk_info->cursub = a()->current_user_sub_num();
      const auto filename =
          fmt::sprintf("%s%03d.NDX", a()->sess().dirs().qwk_directory().string(), a()->current_user_sub().subnum + 1);
      if (qwk_info->index > 0) {
        qwk_info->index = close(qwk_info->index);
      }
      qwk_info->index = open(filename.c_str(), O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    }

    write(qwk_info->index, &qwk_info->qwk_ndx, sizeof(qwk_info->qwk_ndx));
  } else { // Write to email indexes
    write(qwk_info->zero, &qwk_info->qwk_ndx, sizeof(qwk_info->qwk_ndx));
    write(qwk_info->personal, &qwk_info->qwk_ndx, sizeof(qwk_info->qwk_ndx));
  }

  // Setup next NDX position
  qwk_info->qwk_rec_pos += static_cast<uint16_t>(amount_blocks);

  int cur_block = 2;
  while (cur_block <= amount_blocks && !a()->sess().hangup()) {
    size_t this_pos;
    memset(&qwk_info->qwk_rec, ' ', sizeof(qwk_info->qwk_rec));
    this_pos = (cur_block - 2) * sizeof(qwk_info->qwk_rec);

    if (this_pos < len) {
      size_t size = (this_pos + sizeof(qwk_info->qwk_rec) > static_cast<size_t>(len)) ? (len - this_pos - 1) : sizeof(qwk_info->qwk_rec);
      memmove(&qwk_info->qwk_rec, ss.data() + cur + this_pos, size);
    }
    // Save this block
    write(qwk_info->file, &qwk_info->qwk_rec, sizeof(qwk_info->qwk_rec));

    this_pos += sizeof(qwk_info->qwk_rec);
    ++cur_block;
  }
  // Global variable on total amount of records saved
  ++qwk_info->qwk_rec_num;
}




void qwk_menu() {
  const auto qwk_cfg = read_qwk_cfg();

  auto done = false;
  while (!done && !a()->sess().hangup()) {
    bout.cls();
    bout.printfile("QWK");

    bout.nl();
    std::string allowed = "QCDU?";
    if (so()) {
      allowed.push_back('*');
    }
    bout.nl();
    if (so()) {
      bout.bputs("|#7(|#1*|#7=|#2Sysop Menu|#7,|#1Q|#7=|#2Quit|#7) |#1C|#7, |#1D|#7, |#1U|#7: ");
    } else {
      bout.bputs("|#7(|#1Q|#7=|#2Quit|#7) |#1C|#7, |#1D|#7, |#1U|#7: ");
    }
    const auto key = onek(allowed, true);

    switch (key) {
    case '?':
      break;

    case 'Q':
    default:
      done = true;
      break;

    case 'U':
      sysoplog() << "Upload REP packet";
      upload_reply_packet();
      break;

    case 'D': {
      sysoplog() << "Download QWK packet";
      auto namepath =
          FilePath(a()->sess().dirs().qwk_directory(), StrCat(qwk_system_name(qwk_cfg), ".REP"));
      File::Remove(namepath);

      build_qwk_packet();

      if (File::Exists(namepath)) {
        sysoplog() << "REP was uploaded";
        upload_reply_packet();
      }
    } break;
    case 'C':
      sysoplog() << "Config Options";
      config_qwk_bw();
      break;

    case '*':
      if (so()) {
        sysoplog() << "Ran Sysop Config";
        qwk_sysop();
      }
      break;
    }
  }
}

static void qwk_send_file(const string& fn, bool *sent, bool *abort) {
  // TODO(rushfan): Should this just call send_file from sr.cpp?
  *sent = false;
  *abort = false;

  int protocol;
  if (a()->user()->data.qwk_protocol <= 1) {
    protocol = get_protocol(xfertype::xf_down_temp);
  } else {
    protocol = a()->user()->data.qwk_protocol;
  }
  switch (protocol) {
  case -1:
    *abort = true;

    break;
  case 0:
  case WWIV_INTERNAL_PROT_ASCII:
    break;

  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM: {
    double percent = 0.0;
    maybe_internal(fn, sent, &percent, true, protocol);
  } break;

  default: {
    const auto exit_code = extern_prot(protocol - WWIV_NUM_INTERNAL_PROTOCOLS, fn, 1);
    *abort = 0;
    if (exit_code == a()->externs[protocol - WWIV_NUM_INTERNAL_PROTOCOLS].ok1) {
      *sent = 1;
    }
  } break;
  }
}




int get_qwk_max_msgs(uint16_t *qwk_max_msgs, uint16_t *max_per_sub) {
  bout.cls();
  bout.nl();
  bout.Color(2);
  bout << "Largest packet you want, in msgs? (0=Unlimited) : ";
  bout.mpl(5);

  char temp[6];
  bin.input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *qwk_max_msgs = to_number<uint16_t>(temp);

  bout << "Most messages you want per sub? ";
  bout.mpl(5);
  bin.input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *max_per_sub = to_number<uint16_t>(temp);
  return 1;
}


void qwk_nscan() {
#ifdef NEVER // Not ported yet
#define SETREC(f,i)  lseek(f,((long) (i))*((long)sizeof(uploadsrec)),SEEK_SET);
  static constexpr int DOTS = 5;
  uploadsrec u;
  bool abort = false;
  int od, newfile, i, i1, i5, f, count, color = 3;
  char s[201];;

  bout.Color(3);
  bout.bputs("Building NEWFILES.DAT");

  sprintf(s, "%s%s", a()->sess().dirs().qwk_directory().c_str(), "NEWFILES.DAT");
  newfile = open(s, O_BINARY | O_RDWR | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE;
  if (newfile < 1) {
    bout.bputs("Open Error");
    return;
  }

  for (i = 0; i < num_dirs && !abort; i++) {
    bin.checka(&abort);
    count++;

    bout.bprintf("%d.", color);
    if (count >= DOTS) {
      bout << "\r";
      bout << "Searching";
      color++;
      count = 0;
      if (color == 4) {
        color++;
      }
      if (color == 10) {
        color = 0;
      }
    }

    i1 = a()->udir[i].subnum;
    if (a()->sess().qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      if ((dir_dates[a()->udir[i].subnum]) &&
          (dir_dates[a()->udir[i].subnum] < a()->sess().nscandate())) {
        continue;
      }

      od = a()->current_user_dir_num();
      a()->set_current_user_dir_num(i);
      dliscan();
      if (this_date >= a()->sess().nscandate()) {
        sprintf(s, "\r\n\r\n%s - #%s, %d %s.\r\n\r\n",
                a()->dirs()[a()->current_user_dir().subnum].name,
                a()->current_user_dir().keys, numf, "files");
        write(newfile,  s, strlen(s));

        f = open(dlfn, O_RDONLY | O_BINARY);
        for (i5 = 1; (i5 <= numf) && (!(abort)) && (!a()->sess().hangup()); i5++) {
          SETREC(f, i5);
          read(f, &u, sizeof(uploadsrec));
          if (u.daten >= a()->sess().nscandate()) {
            sprintf(s, "%s %5ldk  %s\r\n", u.filename, (long) bytes_to_k(u.numbytes), u.description);
            write(newfile,  s, strlen(s));


#ifndef HUGE_TRAN
            if (u.mask & mask_extended) {
              int pos = 0;
              string ext = a()->current_file_area()->ReadExtentedDescription(u.filename).value_or("");

              if (!ext.empty(0)) {
                int spos = 21, x;

                strcpy(s, "                     ");
                while (ext[pos]) {
                  x = ext[pos];

                  if (x != '\r' && x != '\n' && x > 2) {
                    s[spos] = x;
                  }


                  if (x == '\n' || x == 0) {
                    s[spos] = 0;
                    write(newfile, s, strlen(s));
                    write(newfile, "\r\n", 2);
                    strcpy(s, "                     ");
                    spos = 21;
                  }

                  if (x != '\r' && x != '\n' && x > 2) {
                    ++spos;
                  }

                  ++pos;
                }
              }
            }
#endif

          } else if (!empty()) {
            bin.checka(&abort);
          }

        }
        f = close(f);
      }
      a()->set_current_user_dir_num(od);

    }
  }
  newfile = close(newfile);
#endif  // NEVER
}

void finish_qwk(struct qwk_junk *qwk_info) {
  bool sent = false;
  long numbytes;

  bool done = false;
  int archiver;


  if (!a()->user()->data.qwk_dontscanfiles) {
    qwk_nscan();
  }

  auto qwk_cfg = read_qwk_cfg();
  if (!a()->user()->data.qwk_leave_bulletin) {
    bout.bputs("Grabbing hello/news/goodbye text files...");

    if (!qwk_cfg.hello.empty()) {
      auto parem1 = FilePath(a()->config()->gfilesdir(), qwk_cfg.hello);
      auto parem2 = FilePath(a()->sess().dirs().qwk_directory(), qwk_cfg.hello);
      File::Copy(parem1, parem2);
    }

    if (!qwk_cfg.news.empty()) {
      auto parem1 = FilePath(a()->config()->gfilesdir(), qwk_cfg.news);
      auto parem2 = FilePath(a()->sess().dirs().qwk_directory(), qwk_cfg.news);
      File::Copy(parem1, parem2);
    }

    if (!qwk_cfg.bye.empty()) {
      auto parem1 = FilePath(a()->config()->gfilesdir(), qwk_cfg.bye);
      auto parem2 = FilePath(a()->sess().dirs().qwk_directory(), qwk_cfg.bye);
    }

    for (const auto& b : qwk_cfg.bulletins) {
      if (!File::Exists(b.path)) {
        // Don't have file_daten anymore
        continue;
      }

      // If we want to only copy if bulletin is newer than the users laston date:
      // if(file_daten(qwk_cfg.blt[x]) > date_to_daten(a()->user()->GetLastOnDateNumber()))
      auto parem2 = FilePath(a()->sess().dirs().qwk_directory(), b.name);
      File::Copy(b.path, parem2);
    }
  }

  auto qwkname = StrCat(qwk_system_name(qwk_cfg), ".qwk");

  if (!a()->user()->data.qwk_archive ||
      !a()->arcs[a()->user()->data.qwk_archive - 1].extension[0]) {
    archiver = select_qwk_archiver(qwk_info, 0) - 1;
  } else {
    archiver = a()->user()->data.qwk_archive - 1;
  }

  string qwk_file_to_send;
  if (!qwk_info->abort) {
    auto parem1 = FilePath(a()->sess().dirs().qwk_directory(), qwkname);
    auto parem2 = FilePath(a()->sess().dirs().qwk_directory(), "*.*");

    auto command = stuff_in(a()->arcs[archiver].arca, parem1.string(), parem2.string(), "", "", "");
    ExecuteExternalProgram(command, a()->spawn_option(SPAWNOPT_ARCH_A));

    qwk_file_to_send = FilePath(a()->sess().dirs().qwk_directory(), qwkname).string();

    File qwk_file_to_send_file(qwk_file_to_send);
    if (!File::Exists(qwk_file_to_send)){
      bout.bputs("No such file.");
      bout.nl();
      qwk_info->abort = true;
      return;
    }
    numbytes = qwk_file_to_send_file.length();

    if (numbytes == 0L) {
      bout.bputs("File has nothing in it.");
      qwk_info->abort = true;
      return;
    }
  }

  if (a()->sess().incom()) {
    while (!done && !qwk_info->abort && !a()->sess().hangup()) {
      bool abort = false;
      qwk_send_file(qwk_file_to_send, &sent, &abort);
      if (sent) {
        done = true;
      } else {
        bout.nl();
        bout.Color(2);
        bout.bputs("Packet was not successful...");
        bout.Color(1);
        bout << "Try transfer again?";

        if (!bin.noyes()) {
          done = true;
          abort = true;
          qwk_info->abort = true;
        } else {
          abort = false;
        }

      }
      if (abort) {
        qwk_info->abort = true;
      }
    }
  } else while (!done && !a()->sess().hangup() && !qwk_info->abort) {
    bout.Color(2);
    bout << "Move to what dir? ";
    bout.mpl(60);
    auto new_dir = StringTrim(bin.input_path(60));
    if (new_dir.empty()) {
      continue;
    }

    const auto nfile = FilePath(new_dir, qwkname);

    if (File::Exists(nfile)) {
      bout << "|#5File Exists. Would you like to overrite it?";
      if (bin.yesno()) {
        File::Remove(nfile);
      }
    }

    auto ofile = FilePath(a()->sess().dirs().qwk_directory(), qwkname);
    if (!replacefile(ofile.string(), nfile.string())) {
      bout << "|#6Unable to copy file\r\n|#5Would you like to try again?";
      if (!bin.noyes()) {
        qwk_info->abort = true;
        done = true;
      }
    } else {
      done = true;
    }
  }
}


static string qwk_current_text(int pos) {
  static const char* yesorno[] = {"Yes", "No"};

  switch (pos) {
  case 0:
    return a()->user()->data.qwk_dont_scan_mail ? yesorno[1] : yesorno[0];
  case 1:
    return a()->user()->data.qwk_delete_mail ? yesorno[0] : yesorno[1];
  case 2:
    return a()->user()->data.qwk_dontsetnscan ? yesorno[1] : yesorno[0];
  case 3:
    return a()->user()->data.qwk_remove_color ? yesorno[0] : yesorno[1];
  case 4:
    return a()->user()->data.qwk_convert_color ? yesorno[0] : yesorno[1];
  case 5:
    return a()->user()->data.qwk_leave_bulletin ? yesorno[1] : yesorno[0];
  case 6:
    return a()->user()->data.qwk_dontscanfiles ? yesorno[1] : yesorno[0];
  case 7:
    return a()->user()->data.qwk_keep_routing ? yesorno[1] : yesorno[0];
  case 8:
    return qwk_which_zip();
  case 9:
    return qwk_which_protocol();
  case 10: {
    if (!a()->user()->data.qwk_max_msgs_per_sub && !a()->user()->data.qwk_max_msgs) {
      return "Unlimited/Unlimited";
    }
    if (!a()->user()->data.qwk_max_msgs_per_sub) {
      return fmt::format("Unlimited/{}", a()->user()->data.qwk_max_msgs);
    }
    if (!a()->user()->data.qwk_max_msgs) {
      return fmt::format("{}/Unlimited", a()->user()->data.qwk_max_msgs_per_sub);
    }
    return fmt::format("{}/{}", a()->user()->data.qwk_max_msgs,
                       a()->user()->data.qwk_max_msgs_per_sub);
  }
  case 11:
  default:
    return "DONE";
  }
}

void config_qwk_bw() {
  bool done = false;

  while (!done && !a()->sess().hangup()) {
    bout.cls();
    bout.litebar("QWK Preferences");
    bout << "|#1A|#9) Include E-Mail:             |#2" << qwk_current_text(0) << wwiv::endl;
    bout << "|#1B|#9) Delete Included E-Mail:     |#2" << qwk_current_text(1) << wwiv::endl;
    bout << "|#1C|#9) Update Last Read Pointer:   |#2" << qwk_current_text(2) << wwiv::endl;
    bout << "|#1D|#9) Remove WWIV color codes:    |#2" << qwk_current_text(3) << wwiv::endl;
    bout << "|#1E|#9) Convert WWIV color to ANSI: |#2" << qwk_current_text(4) << wwiv::endl;
    bout << "|#1F|#9) Include Bulletins:          |#2" << qwk_current_text(5) << wwiv::endl;
    //bout << "|#1G|#9) Scan New Files:             |#2" << qwk_current_text(6) << wwiv::endl;
    bout << "|#1H|#9) Remove Routing Information: |#2" << qwk_current_text(7) << wwiv::endl;
    bout << "|#1I|#9) Default Compression Type :  |#2" << qwk_current_text(8) << wwiv::endl;
    bout << "|#1J|#9) Default Transfer Protocol:  |#2" << qwk_current_text(9) << wwiv::endl;
    bout << "|#1K|#9) Max Messages To Include:    |#2" << qwk_current_text(10) << wwiv::endl;
    bout << "|#1Q|#9) Done" << wwiv::endl;
    bout.nl();
    bout.nl();
    bout << "|#9Select: ";
    int key = onek("QABCDEFGHIJK", true);

    if (key == 'Q') {
      done = true;
    }
    key = key - 'A';

    switch (key) {
    case 0:
      a()->user()->data.qwk_dont_scan_mail = !a()->user()->data.qwk_dont_scan_mail;
      break;
    case 1:
      a()->user()->data.qwk_delete_mail = !a()->user()->data.qwk_delete_mail;
      break;
    case 2:
      a()->user()->data.qwk_dontsetnscan = !a()->user()->data.qwk_dontsetnscan;
      break;
    case 3:
      a()->user()->data.qwk_remove_color = !a()->user()->data.qwk_remove_color;
      break;
    case 4:
      a()->user()->data.qwk_convert_color = !a()->user()->data.qwk_convert_color;
      break;
    case 5:
      a()->user()->data.qwk_leave_bulletin = !a()->user()->data.qwk_leave_bulletin;
      break;
    case 6:
      a()->user()->data.qwk_dontscanfiles = !a()->user()->data.qwk_dontscanfiles;
      break;
    case 7:
      a()->user()->data.qwk_keep_routing = !a()->user()->data.qwk_keep_routing;
      break;
    case 8: {
      qwk_junk qj{};
      memset(&qj, 0, sizeof(struct qwk_junk));
      bout.cls();

      const auto arcno = static_cast<unsigned short>(select_qwk_archiver(&qj, 1));
      if (!qj.abort) {
        a()->user()->data.qwk_archive = arcno;
      }
      break;
    }
    case 9: {
      qwk_junk qj{};
      memset(&qj, 0, sizeof(struct qwk_junk));
      bout.cls();

      const auto arcno = select_qwk_protocol(&qj);
      if (!qj.abort) {
        a()->user()->data.qwk_protocol = arcno;
      }
    } break;
    case 10: {
      uint16_t max_msgs, max_per_sub;

      if (get_qwk_max_msgs(&max_msgs, &max_per_sub)) {
        a()->user()->data.qwk_max_msgs = max_msgs;
        a()->user()->data.qwk_max_msgs_per_sub = max_per_sub;
      }
    } break;
    }
  }
}

void qwk_sysop() {
  if (!so()) {
    return;
  }

  auto c = read_qwk_cfg();

  auto done = false;
  while (!done && !a()->sess().hangup()) {
    bout.cls();
    bout.format("|#21|#9) Hello file:      |#5{}\r\n", c.hello.empty() ? "|#3<None>" : c.hello);
    bout.format("|#22|#9) News file:       |#5{}\r\n", c.news.empty() ? "|#3<None>" : c.news);
    bout.format("|#23|#9) Goodbye file:    |#5{}\r\n", c.bye.empty() ? "|#3<None>" : c.bye);
    auto sn = qwk_system_name(c);
    bout.format("|#24|#9) Packet name:     |#5{}\r\n", sn);
    const auto max_msgs = c.max_msgs == 0 ? "(Unlimited)" : std::to_string(c.max_msgs);
    bout.format("|#25|#9) Max Msgs/Packet: |#5{}\r\n", max_msgs);
    bout.format("|#26|#9) Modify Bulletins ({})\r\n", c.amount_blts);
    bout.format("|#2Q|#9) Quit\r\n");
    bout.nl();
    bout << "|#9Selection? ";

    const int x = onek("Q123456\r\n");
    if (x == '1' || x == '2' || x == '3') {
      bout.nl();
      bout << "|#9Enter New Filename:";
      bout.mpl(12);
    }

    switch (x) {
    case '1':
      c.hello = bin.input(12);
      break;
    case '2':
      c.news = bin.input(12);
      break;
    case '3':
      c.bye = bin.input(12);
      break;
    case '4': {
      sn = qwk_system_name(c);
      write_qwk_cfg(c);
      bout.nl();
      bout.format("|#9 Current Packet Name:  |#1{}\r\n", sn);
      bout <<     "|#9Enter new packet name: ";
      bout.mpl(8);
      sn = bin.input_upper(sn, 8);
      if (!sn.empty()) {
        c.packet_name = sn;
      }
      write_qwk_cfg(c);
    } break;

    case '5': {
      bout << "|#9(|#10=Unlimited|#9) Enter max messages per packet: ";
      bout.mpl(5);
      c.max_msgs = bin.input_number(c.max_msgs, 0, 65535, true);
    } break;
    case '6':
      modify_bulletins(c);
      break;
    default:
      done = true;
    }
  }

  write_qwk_cfg(c);
}

}
