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
#include "bbs/qwk.h"

#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/defaults.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/make_abs_cmd.h"
#include "bbs/pause.h"
#include "bbs/save_qscan.h"
#include "bbs/sr.h"
#include "bbs/stuffin.h"
#include "bbs/subacc.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/ansi/makeansi.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include <memory>
#include <string>


#include "core/clock.h"
#include "read_message.h"
#include "sdk/qscan.h"

#define qwk_iscan(x)         (iscan1(a()->usub[x].subnum))

using std::unique_ptr;
using std::string;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;

// Also used in qwk1.cpp
const char *QWKFrom = "\x04""0QWKFrom:";

static int qwk_percent;
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

  TextFile fp(FilePath(a()->qwk_directory(), "CONTROL.DAT"), "wd");
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
  const qscan_bitset qb(a()->context().qsc_q, max_size);
  std::vector<std::pair<int, std::string>> subs_list;
  for (size_t cur = 0; a()->usub[cur].subnum != -1 && cur < max_size; cur++) {
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

  remove_from_temp("*.*", a()->qwk_directory(), false);

  if (a()->uconfsub[1].confnum != -1 && okconf(a()->user())) {
    save_conf = true;
    tmp_disable_conf(true);
  }
  TempDisablePause disable_pause;

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

  const auto filename = FilePath(a()->batch_directory(), MESSAGES_DAT).string();
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

  checka(&qwk_info.abort);

  bout.cls();
  if (!qwk_info.abort) {
    bout << "|#7\xDA" << string(4, '\xC4') << '\xC2' << string(60, '\xC4') << '\xC2' << string(5, '\xC4') 
      << '\xC2' << string(4, '\xC4') << '\xBF' << wwiv::endl;
  }

  checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    bout << "|#7\xB3|#2Sub |#7\xB3|#3Sub Name" << string(52, ' ') << "|#7\xB3|#8Total|#7\xB3|#5New |#7\xB3" << wwiv::endl;
  }

  checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    bout << "|#7" << "\xC3" << string(4, '\xC4')
         << '\xC5' << string(60, '\xC4') 
         << '\xC5' << string(5, '\xC4') 
         << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  }

  bool msgs_ok = true;
  for (uint16_t i = 0; (a()->usub[i].subnum != -1) && (i < a()->subs().subs().size()) && (!a()->hangup_) && !qwk_info.abort && msgs_ok; i++) {
    msgs_ok = (max_msgs ? qwk_info.qwk_rec_num <= max_msgs : true);
    if (a()->context().qsc_q[a()->usub[i].subnum / 32] & (1L << (a()->usub[i].subnum % 32))) {
      qwk_gather_sub(i, &qwk_info);
    }
  }

  bout << "|#7\xC3" << string(4, '\xC4') << '\xC5' << string(60, '\xC4') << '\xC5' << string(5, '\xC4') 
      << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  bout.nl(2);

  if (qwk_info.abort) {
    bout.Color(1);
    bout << "Abort everything? (NO=Download what I have gathered)";
    if (!yesno()) {
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

  // Restore on a()->hangup_ too, someone might have hungup in the middle of building the list
  if (qwk_info.abort || a()->user()->data.qwk_dontsetnscan || a()->hangup_) {
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

  if (a()->hangup_ || (sn < 0)) {
    return;
  }

  auto qscnptrx = a()->context().qsc_p[sn];
  const auto sd = WWIVReadLastRead(sn);

  if (qwk_percent || (!sd || sd > qscnptrx)) {
    const auto os = a()->current_user_sub_num();
    a()->set_current_user_sub_num(bn);
    int i;

    // Get total amount of messages in base
    if (!qwk_iscan(a()->current_user_sub_num())) {
      return;
    }

    qscnptrx = a()->context().qsc_p[sn];

    if (!qwk_percent) {
      // Find out what message number we are on
      for (i = a()->GetNumMessagesInCurrentMessageArea(); (i > 1) && (get_post(i - 1)->qscan > qscnptrx); i--)
        ;
    } else { // Get last qwk_percent of messages in sub
      auto temp_percent = static_cast<float>(qwk_percent) / 100;
      if (temp_percent > 1.0) {
        temp_percent = 1.0;
      }
      i = a()->GetNumMessagesInCurrentMessageArea() - 
          static_cast<int>(std::floor(temp_percent * a()->GetNumMessagesInCurrentMessageArea()));
    }

    char thissub[81];
    to_char_array(thissub, a()->current_sub().name);
    thissub[60] = 0;
    const auto subinfo = fmt::sprintf("|#7\xB3|#9%-4d|#7\xB3|#1%-60s|#7\xB3 |#2%-4d|#7\xB3|#3%-4d|#7\xB3",
                                      bn + 1, thissub, a()->GetNumMessagesInCurrentMessageArea(),
                                      a()->GetNumMessagesInCurrentMessageArea() - i + 1 - (qwk_percent ? 1 : 0));
    bout.bputs(subinfo);
    bout.nl();

    checka(&qwk_info->abort);

    if ((a()->GetNumMessagesInCurrentMessageArea() > 0)
        && (i <= a()->GetNumMessagesInCurrentMessageArea()) && !qwk_info->abort) {
      if ((get_post(i)->qscan > a()->context().qsc_p[a()->GetCurrentReadMessageArea()]) ||
          qwk_percent) {
        qwk_start_read(i, qwk_info);  // read messsage
      }
    }

    auto status = a()->status_manager()->GetStatus();
    a()->context().qsc_p[a()->GetCurrentReadMessageArea()] = status->GetQScanPointer() - 1;
    a()->set_current_user_sub_num(os);
  } 
  bout.Color(0);
}

void qwk_start_read(int msgnum, struct qwk_junk *qwk_info) {
  a()->context().clear_irt();

  if (a()->GetCurrentReadMessageArea() < 0) {
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
    checka(&qwk_info->abort);

  } while (!done && !a()->hangup_ && !qwk_info->abort);
  bout.bputch('\r');
}

void make_pre_qwk(int msgnum, struct qwk_junk *qwk_info) {
  postrec* p = get_post(msgnum);
  if ((p->status & (status_unvalidated | status_delete)) && !lcs()) {
    return;
  }

  int nn = a()->net_num();
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
      a()->context().qsc_p[a()->GetCurrentReadMessageArea()]) { // Update qscan pointer right here
    a()->context().qsc_p[a()->GetCurrentReadMessageArea()] = p->qscan; // And here
  }
}

static int _fieeetomsbin(float *src4, float *dest4) {
  const auto ieee = reinterpret_cast<unsigned char*>(src4);
  const auto msbin = reinterpret_cast<unsigned char*>(dest4);
  unsigned char msbin_exp = 0x00;

  /* See _fmsbintoieee() for details of formats   */
  const unsigned char sign = ieee[3] & 0x80;
  msbin_exp |= ieee[3] << 1;
  msbin_exp |= ieee[2] >> 7;

  /* An ieee exponent of 0xfe overflows in MBF    */
  if (msbin_exp == 0xfe) {
    return 1;
  }

  msbin_exp += 2;     /* actually, -127 + 128 + 1 */

  for (auto i = 0; i < 4; i++) {
    msbin[i] = 0;
  }

  msbin[3] = msbin_exp;
  msbin[2] |= sign;
  msbin[2] |= ieee[2] & 0x7f;
  msbin[1] = ieee[1];
  msbin[0] = ieee[0];

  return 0;
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
      bout.curatr(save_curatr);
    } else if (a()->user()->data.qwk_keep_routing == false && x == 4 && text[pos + 1] == '0') {
      if (text[pos + 1] == 0) {
        ++pos;
      } else { 
        while (text[pos] != '\xE3' && text[pos] != '\r' && pos < text.size() && text[pos] != 0) {
          ++pos;
        }
      }
      ++pos;
      if (text[pos] == '\n') {
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

  while (pos < size && !a()->hangup_) {
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
    memcpy(qwk_info->qwk_rec.to, "ALL", 3);
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
    pausescr();
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
          fmt::sprintf("%s%03d.NDX", a()->qwk_directory(), a()->current_user_sub().subnum + 1);
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
  while (cur_block <= amount_blocks && !a()->hangup_) {
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



std::string qwk_system_name(const qwk_config& c) {
  auto qwkname = !c.packet_name.empty() ? c.packet_name : a()->config()->system_name();

  if (qwkname.length() > 8) {
    qwkname.resize(8);
  }
  std::replace( qwkname.begin(), qwkname.end(), ' ', '-' );
  std::replace( qwkname.begin(), qwkname.end(), '.', '-' );
  return ToStringUpperCase(qwkname);
}

void qwk_menu() {
  qwk_percent = 0;
  const auto qwk_cfg = read_qwk_cfg();

  auto done = false;
  while (!done && !a()->hangup_) {
    bout.cls();
    printfile("QWK");
    if (so()) {
      bout.bputs("1) Sysop QWK config");
    }

    if (qwk_percent) {
      bout.Color(3);
      bout.nl();
      bout << fmt::sprintf("Of all messages, you will be downloading %d%%\r\n", qwk_percent);
    }
    bout.nl();
    std::string allowed = "7[3Q1DCUBS%";
    if (so()) {
      allowed.push_back('1');
    }
    allowed.append("7] ");
    bout << allowed;
    bout.mpl(1);

    allowed = "Q\r?CDUBS%";
    if (so()) {
      allowed.push_back('1');
    }
    const auto key = onek(allowed);

    switch (key) {
    case '?':
      break;

    case 'Q':
    case '\r':
    default:
      done = true;
      break;

    case 'U':
      sysoplog() << "Upload REP packet";
      upload_reply_packet();
      break;

    case 'D': {
      sysoplog() << "Download QWK packet";
      auto namepath = FilePath(a()->qwk_directory(), StrCat(qwk_system_name(qwk_cfg), ".REP"));
      File::Remove(namepath);

      build_qwk_packet();

      if (File::Exists(namepath)) {
        sysoplog() << "REP was uploaded";
        upload_reply_packet();
      }
    } break;
    case 'B': {
      sysoplog() << "Down/Up QWK/REP packet";

      auto namepath = FilePath(a()->qwk_directory(), StrCat(qwk_system_name(qwk_cfg), ".REP"));
      File::Remove(namepath);

      build_qwk_packet();

      if (File::Exists(namepath)) {
        upload_reply_packet();
      }
     } break;

    case 'S':
      sysoplog() << "Select Subs";
      config_qscan();
      break;

    case 'C':
      sysoplog() << "Config Options";
      config_qwk_bw();
      break;

    case '%': {
      sysoplog() << "Set %";
      bout.Color(2);
      bout << "Enter percent of all messages in all QSCAN subs to pack:";
      bout.mpl(3);
      char temp[101];
      input(temp, 3);
      qwk_percent = to_number<int>(temp);
      if (qwk_percent > 100) {
        qwk_percent = 100;
      }
    } break;

    case '1':
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

  auto protocol = -1;
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

unsigned short select_qwk_protocol(qwk_junk *qwk_info) {
  const auto protocol = get_protocol(xfertype::xf_down_temp);
  if (protocol == -1) {
    qwk_info->abort = true;
  }
  return static_cast<unsigned short>(protocol);
}

qwk_config read_qwk_cfg() {
  qwk_config_430 o{};

  const auto filename = FilePath(a()->config()->datadir(), QWK_CFG).string();
  int f = open(filename.c_str(), O_BINARY | O_RDONLY);
  if (f < 0) {
    return {};
  }
  read(f,  &o, sizeof(qwk_config_430));
  int x = 0;

  qwk_config c{};
  while (x < o.amount_blts) {
    auto pos = sizeof(qwk_config_430) + (x * BULL_SIZE);
    lseek(f, pos, SEEK_SET);
    char b[BULL_SIZE];
    memset(b, 0, sizeof(b));
    read(f, &b, BULL_SIZE);
    qwk_bulletin bltn;
    bltn.path = b;
    c.bulletins.emplace_back(bltn);
    ++x;
  }

  x = 0;
  for (auto& blt : c.bulletins) {
    const auto pos = sizeof(qwk_config_430) + (ssize(c.bulletins) * BULL_SIZE) + (x * BNAME_SIZE);
    lseek(f, pos, SEEK_SET);
    char b[BULL_SIZE];
    memset(b, 0, sizeof(b));
    read(f, &b, BULL_SIZE);
    blt.name = b;
    ++x;
  }
  close(f);

  c.fu = o.fu;
  c.timesd = o.timesd;
  c.timesu = o.timesu;
  c.max_msgs = o.max_msgs;
  c.hello = o.hello;
  c.news = o.news;
  c.bye = o.bye;
  c.packet_name = o.packet_name;
  c.amount_blts = o.amount_blts;
  return c;
}

void write_qwk_cfg(const qwk_config& c) {
  const auto filename = FilePath(a()->config()->datadir(), QWK_CFG).string();
  const auto f = open(filename.c_str(), O_BINARY | O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  if (f < 0) {
    return;
  }

  qwk_config_430 o{};
  o.fu = c.fu;
  o.timesd = c.timesd;
  o.timesu = c.timesu;
  o.max_msgs = c.max_msgs;
  to_char_array(o.hello, c.hello);
  to_char_array(o.news, c.news);
  to_char_array(o.bye, c.bye);
  to_char_array(o.packet_name, c.packet_name);
  o.amount_blts = c.bulletins.size();

  write(f, &o, sizeof(qwk_config_430));

  int x = 0;
  for (const auto& b : c.bulletins) {
    const auto pos = sizeof(qwk_config_430) + (x * BULL_SIZE);
    lseek(f, pos, SEEK_SET);

    if (!b.path.empty()) {
      char p[BULL_SIZE+1];
      memset(&p, 0, BULL_SIZE);
      to_char_array(p, b.path);
      write(f, p, BULL_SIZE);
      ++x;
    }
  }

  x = 0;
  for (const auto& b : c.bulletins) {
    const auto pos = sizeof(qwk_config_430) + (c.bulletins.size() * BULL_SIZE) + (x++ * BNAME_SIZE);
    lseek(f, pos, SEEK_SET);

    if (!b.name.empty()) {
      char p[BNAME_SIZE+1];
      memset(&p, 0, BNAME_SIZE);
      to_char_array(p, b.name);
      write(f, p, BNAME_SIZE);
    }
  }

  close(f);
}

int get_qwk_max_msgs(uint16_t *qwk_max_msgs, uint16_t *max_per_sub) {
  bout.cls();
  bout.nl();
  bout.Color(2);
  bout << "Largest packet you want, in msgs? (0=Unlimited) : ";
  bout.mpl(5);

  char temp[6];
  input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *qwk_max_msgs = to_number<uint16_t>(temp);

  bout << "Most messages you want per sub? ";
  bout.mpl(5);
  input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *max_per_sub = to_number<uint16_t>(temp);
  return 1;
}

void qwk_nscan() {
#ifdef NEVER // Not ported yet
  static constexpr int DOTS = 5;
  uploadsrec u;
  bool abort = false;
  int od, newfile, i, i1, i5, f, count, color = 3;
  char s[201];;

  bout.Color(3);
  bout.bputs("Building NEWFILES.DAT");

  sprintf(s, "%s%s", a()->qwk_directory().c_str(), "NEWFILES.DAT");
  newfile = open(s, O_BINARY | O_RDWR | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE;
  if (newfile < 1) {
    bout.bputs("Open Error");
    return;
  }

  for (i = 0; (i < num_dirs) && (!abort) && (a()->udir[i].subnum != -1); i++) {
    checka(&abort);
    count++;

    bout << fmt::sprintf("%d.", color);
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
    if (a()->context().qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      if ((dir_dates[a()->udir[i].subnum]) &&
          (dir_dates[a()->udir[i].subnum] < a()->context().nscandate())) {
        continue;
      }

      od = a()->current_user_dir_num();
      a()->set_current_user_dir_num(i);
      dliscan();
      if (this_date >= a()->context().nscandate()) {
        sprintf(s, "\r\n\r\n%s - #%s, %d %s.\r\n\r\n",
                a()->directories[a()->current_user_dir().subnum].name,
                a()->current_user_dir().keys, numf, "files");
        write(newfile,  s, strlen(s));

        f = open(dlfn, O_RDONLY | O_BINARY);
        for (i5 = 1; (i5 <= numf) && (!(abort)) && (!a()->hangup_); i5++) {
          SETREC(f, i5);
          read(f, &u, sizeof(uploadsrec));
          if (u.daten >= a()->context().nscandate()) {
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
            checka(&abort);
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
      auto parem2 = FilePath(a()->qwk_directory(), qwk_cfg.hello);
      File::Copy(parem1, parem2);
    }

    if (!qwk_cfg.news.empty()) {
      auto parem1 = FilePath(a()->config()->gfilesdir(), qwk_cfg.news);
      auto parem2 = FilePath(a()->qwk_directory(), qwk_cfg.news);
      File::Copy(parem1, parem2);
    }

    if (!qwk_cfg.bye.empty()) {
      auto parem1 = FilePath(a()->config()->gfilesdir(), qwk_cfg.bye);
      auto parem2 = FilePath(a()->qwk_directory(), qwk_cfg.bye);
    }

    for (const auto& b : qwk_cfg.bulletins) {
      if (!File::Exists(b.path)) {
        // Don't have file_daten anymore
        continue;
      }

      // If we want to only copy if bulletin is newer than the users laston date:
      // if(file_daten(qwk_cfg.blt[x]) > date_to_daten(a()->user()->GetLastOnDateNumber()))
      auto parem2 = FilePath(a()->qwk_directory(), b.name);
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
    auto parem1 = FilePath(a()->qwk_directory(), qwkname);
    auto parem2 = FilePath(a()->qwk_directory(), "*.*");

    auto command = stuff_in(a()->arcs[archiver].arca, parem1.string(), parem2.string(), "", "", "");
    ExecuteExternalProgram(command, a()->spawn_option(SPAWNOPT_ARCH_A));

    qwk_file_to_send = StrCat(a()->qwk_directory(), qwkname);
    // TODO(rushfan): Should we just have a make abs path?
    make_abs_cmd(a()->bbsdir().string(), &qwk_file_to_send);

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

  if (a()->context().incom()) {
    while (!done && !qwk_info->abort && !a()->hangup_) {
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

        if (!noyes()) {
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
  } else while (!done && !a()->hangup_ && !qwk_info->abort) {
    bout.Color(2);
    bout << "Move to what dir? ";
    bout.mpl(60);
    auto new_dir = input(60);
    StringTrim(new_dir);
    if (new_dir.empty()) {
      continue;
    }

    const auto nfile = FilePath(new_dir, qwkname);

    if (File::Exists(nfile)) {
      bout << "|#5File Exists. Would you like to overrite it?";
      if (yesno()) {
        File::Remove(nfile);
      }
    }

    auto ofile = FilePath(a()->qwk_directory(), qwkname);
    if (!replacefile(ofile.string(), nfile.string())) {
      bout << "|#6Unable to copy file\r\n|#5Would you like to try again?";
      if (!noyes()) {
        qwk_info->abort = true;
        done = true;
      }
    } else {
      done = true;
    }
  }
}
