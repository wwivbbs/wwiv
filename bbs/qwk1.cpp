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

// ReSharper disable once CppUnusedIncludeDirective
#include <fcntl.h>
#ifdef _WIN32
// ReSharper disable once CppUnusedIncludeDirective
#include <io.h> // needed for lseek, etc
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "bbs/application.h"
#include "bbs/archivers.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "common/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "common/input.h"
#include "bbs/message_file.h"
#include "bbs/msgbase1.h"
#include "common/pause.h"
#include "common/quote.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/stuffin.h"
#include "bbs/subacc.h"
#include "bbs/sublist.h"
#include "bbs/sysoplog.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/msgapi/message_utils_wwiv.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include <chrono>
#include <memory>
#include <optional>
#include <sstream>

using std::string;
using std::unique_ptr;
using std::chrono::milliseconds;

using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;

#define qwk_iscan_literal(x) (iscan1(x))

void qwk_email_text(const char *text, char *title, char *to);
void qwk_inmsg(const char *text,messagerec *m1, const char *aux, const char *name, const wwiv::core::DateTime& dt);
void qwk_post_text(const char *text, char *title, int16_t sub);


extern const char* QWKFrom;
extern int qwk_percent;

// from readmail.cpp
bool read_same_email(std::vector<tmpmailrec>& mloc, int mw, int rec, mailrec& m, int del,
                     unsigned short stat);

std::optional<std::string> get_qwk_from_message(const std::string& text) {
  auto qwk_from_start = strstr(text.c_str(), QWKFrom + 2);

  if (qwk_from_start == nullptr) {
    return std::nullopt;
  }

  qwk_from_start += strlen(QWKFrom + 2); // Get past 'QWKFrom:'
  const auto qwk_from_end = strchr(qwk_from_start, '\r');

  if (qwk_from_end && qwk_from_end > qwk_from_start) {
    std::string temp(qwk_from_start, qwk_from_end - qwk_from_start);

    StringTrim(&temp);
    StringUpperCase(&temp);
    return temp;
  }
  return std::nullopt;
}

void qwk_remove_email() {
  a()->emchg_ = false;

  auto mloc = (tmpmailrec*)malloc(MAXMAIL * sizeof(tmpmailrec));
  if (!mloc) {
    bout.bputs("Not enough memory.");
    return;
  }

  auto f(OpenEmailFile(true));

  if (!f->IsOpen()) {
    free(mloc);
    return;
  }

  const auto mfl = f->length() / sizeof(mailrec);
  uint8_t mw = 0;

  mailrec m{};
  for (unsigned long i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
    f->Seek(i * sizeof(mailrec), File::Whence::begin);
    f->Read(&m, sizeof(mailrec));
    if ((m.tosys == 0) && (m.touser == a()->usernum)) {
      mloc[mw].index = static_cast<int16_t>(i);
      mloc[mw].fromsys = m.fromsys;
      mloc[mw].fromuser = m.fromuser;
      mloc[mw].daten = m.daten;
      mloc[mw].msg = m.msg;
      mw++;
    }
  }
  a()->user()->data.waiting = mw;

  if (mw == 0) {
    free(mloc);
    return;
  }

  int curmail = 0;
  bool done = false;
  do {
    delmail(*f, mloc[curmail].index);

    ++curmail;
    if (curmail >= mw) {
      done = true;
    }

  } while (!a()->sess().hangup() && !done);
}

void qwk_gather_email(qwk_junk* qwk_info) {
  mailrec m{};
  postrec junk{};

  a()->emchg_ = false;
  std::vector<tmpmailrec> mloc;

  auto f(OpenEmailFile(false));
  if (!f->IsOpen()) {
    bout.nl(2);
    bout.bputs("No mail file exists!");
    bout.nl();
    return;
  }
  const auto mfl = static_cast<int>(f->length() / sizeof(mailrec));
  uint8_t mw = 0;
  for (int i = 0; i < mfl && mw < MAXMAIL; i++) {
    f->Seek(static_cast<File::size_type>(i) * sizeof(mailrec), File::Whence::begin);
    f->Read(&m, sizeof(mailrec));
    if ((m.tosys == 0) && (m.touser == a()->usernum)) {
      tmpmailrec r{};
      r.index = static_cast<int16_t>(i);
      r.fromsys = m.fromsys;
      r.fromuser = m.fromuser;
      r.daten = m.daten;
      r.msg = m.msg;
      mloc.emplace_back(r);
      mw++;
    }
  }
  f->Close();
  a()->user()->data.waiting = mw;

  if (mw == 0) {
    bout.nl();
    bout.bputs("You have no mail.");
    bout.nl();
    return;
  }

  bout.Color(7);
  bout.bputs("Gathering Email");

  int curmail = 0;
  bool done = false;

  qwk_info->in_email = 1;

  auto filename = FilePath(a()->sess().dirs().qwk_directory(), "PERSONAL.NDX");
  qwk_info->personal = open(filename.string().c_str(), O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  filename = FilePath(a()->sess().dirs().qwk_directory(), "000.NDX");
  qwk_info->zero = open(filename.string().c_str(), O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

  do {
    read_same_email(mloc, mw, curmail, m, 0, 0);

    strupr(m.title);
    strncpy(qwk_info->email_title, stripcolors(m.title), 25);
    // had crash in stripcolors since this won't null terminate.
    // qwk_info->email_title[25] = 0;

    //i = (ability_read_email_anony & ss.ability) != 0;

    if (m.fromsys && !m.fromuser) {
      grab_user_name(&(m.msg), "email", network_number_from(&m));
    } else {
      a()->net_email_name.clear();
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
      done = true;
    }

  } while (!a()->sess().hangup() && !done);

  qwk_info->in_email = 0;
}

int select_qwk_archiver(struct qwk_junk* qwk_info, int ask) {
  std::string allowed = "Q\r";

  bout.nl();
  bout.bputs("Select an archiver");
  bout.nl();
  if (ask) {
    bout.bputs("0) Ask me later");
  }
  auto num = 0;
  for (const auto& arc : a()->arcs) {
    std::string ext = arc.extension;
    ++num;
    if (!StringTrim(ext).empty() && ext != "EXT") {
      allowed.append(std::to_string(num));
      bout.format("1{}) 3{}", num, ext);
      bout.nl();
    }
  }
  bout.nl();
  bout << "Enter #  Q to Quit <CR>=1 :";

  if (ask) {
    allowed.push_back('0');
  }

  const auto archiver = onek(allowed);

  if (archiver == '\r') {
    return 1;
  }

  if (archiver == 'Q') {
    qwk_info->abort = true;
    return 0;
  }
  return archiver - '0';
}

string qwk_which_zip() {
  if (a()->user()->data.qwk_archive == 0) {
    return "ASK";
  }

  if (a()->user()->data.qwk_archive > 4) {
    a()->user()->data.qwk_archive = 0;
    return "ASK";
  }

  if (a()->arcs[a()->user()->data.qwk_archive - 1].extension[0] == '\0') {
    a()->user()->data.qwk_archive = 0;
    return "ASK";
  }

  return a()->arcs[a()->user()->data.qwk_archive - 1].extension;
}

string qwk_which_protocol() {
  if (a()->user()->data.qwk_protocol == 1) {
    a()->user()->data.qwk_protocol = 0;
  }

  if (a()->user()->data.qwk_protocol == 0) {
    return string("ASK");
  }
  string prot(prot_name(a()->user()->data.qwk_protocol));
  if (prot.size() > 22) {
    return prot.substr(0, 21);
  }
  return prot;
}

static void qwk_receive_file(const std::string& fn, bool* received, int prot_num) {
  if (prot_num <= 1 || prot_num == 5) {
    prot_num = get_protocol(xfertype::xf_up_temp);
  }

  switch (prot_num) {
  case -1:
  case 0:
  case WWIV_INTERNAL_PROT_ASCII:
  case WWIV_INTERNAL_PROT_BATCH:
    *received = false;
    break;
  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM:
    maybe_internal(fn, received, nullptr, false, prot_num);
    break;
  default:
    if (a()->sess().incom()) {
      extern_prot(prot_num - WWIV_NUM_INTERNAL_PROTOCOLS, fn, 0);
      *received = File::Exists(fn);
    }
    break;
  }
}

static std::filesystem::path ready_reply_packet(const std::string& packet_name, const std::string& msg_name) {
  const auto archiver = match_archiver(a()->arcs, packet_name).value_or(a()->arcs[0]);
  const auto command = stuff_in(archiver.arce, packet_name, msg_name, "", "", "");

  ExecuteExternalProgram(command, EFLAG_QWK_DIR);
  return FilePath(a()->sess().dirs().qwk_directory(), msg_name);
}

// Takes reply packet and converts '227' (ã) to '13' and removes QWK style
// space padding at the end.
static std::string make_text_ready(const std::string& text, long len) {
  string temp;
  for (auto pos = 0; pos < len; pos++) {
    if (text[pos] == '\xE3') {
      temp.push_back(13);
      temp.push_back(10);
    } else {
      temp.push_back(text[pos]);
    }
  }

  // Remove trailing space character only. wwiv::strings::StringTrimEnd also removes other
  // whitespace characters like \t and \r\n
  const auto pos = temp.find_last_not_of(" ");
  temp.erase(pos + 1);
  return temp;
}

static std::string make_text_file(int filenumber, int curpos, int blocks) {
  std::string text;
  text.resize(sizeof(qwk_record) * blocks);

  lseek(filenumber, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
  read(filenumber, &text[0], sizeof(qwk_record) * blocks);

  return make_text_ready(text, static_cast<int>(sizeof(qwk_record) * blocks));
}

static void process_reply_dat(const std::string& name) {
  qwk_record qwk{};
  int curpos = 0;
  int done = 0;

  int repfile = open(name.c_str(), O_RDONLY | O_BINARY);

  if (repfile < 0) {
    bout.nl();
    bout.Color(3);
    bout.bputs("Can't open packet.");
    bout.pausescr();
    return;
  }

  lseek(repfile, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
  read(repfile, &qwk, sizeof(qwk_record));

  // Should check to make sure first block contains our bbs id
  ++curpos;

  bout.cls();

  while (!done && !a()->sess().hangup()) {
    bool to_email = false;

    lseek(repfile, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
    ++curpos;

    if (read(repfile, &qwk, sizeof(qwk_record)) < 1) {
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
      if (to_number<int>(tosub) == 0) {
        to_email = true;
      } else if (qwk.status != ' ' && qwk.status != '-') { // if not public
        bout.cls();
        bout.Color(1);
        bout.bprintf("Message '2%s1' is marked 3PRIVATE", title);
        bout.nl();
        bout.Color(1);
        bout.bprintf("It is addressed to 2%s", to);
        bout.nl(2);
        bout.Color(7);
        bout << "Route into E-Mail?";
        if (bin.noyes()) {
          to_email = true;
        }
      }

      auto text = make_text_file(repfile, curpos, to_number<int>(blocks) - 1);
      if (text.empty()) {
        curpos += to_number<int>(blocks) - 1;
        continue;
      }

      if (to_email) {
        auto to_from_msg_opt = get_qwk_from_message(text);
        if (to_from_msg_opt.has_value()) {
          bout.nl();
          bout.Color(3);
          bout.bprintf("1) %s", to);
          bout.nl();
          bout.Color(3);
          bout.bprintf("2) %s", to_from_msg_opt.value());
          bout.nl(2);

          bout << "Which address is correct?";
          bout.mpl(1);

          const int x = onek("12");

          if (x == '2') {
            to_char_array(to, to_from_msg_opt.value());
          }
        }
      }

      if (to_email) {
        qwk_email_text(text.c_str(), title, to);
      } else if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
        // Not enough disk space
        bout.nl();
        bout.bputs("Sorry, not enough disk space left.");
        bout.pausescr();
      } else {
        qwk_post_text(text.c_str(), title, to_number<int16_t>(tosub) - 1);
      }
      curpos += to_number<int>(blocks) - 1;
    }
  }
  close(repfile);
}


void upload_reply_packet() {
  bool rec = true;
  int save_conf = 0;
  auto qwk_cfg = read_qwk_cfg();

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = daten_t_now();
  }

  ++qwk_cfg.timesu;
  write_qwk_cfg(qwk_cfg);

  const auto save_sub = a()->current_user_sub_num();
  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
    save_conf = 1;
    tmp_disable_conf(true);
  }

  const auto rep_name = StrCat(qwk_system_name(qwk_cfg), ".REP");

  bout.format("Hit 'Y' to upload reply packet {} :", rep_name);
  const auto rep_path = FilePath(a()->sess().dirs().qwk_directory(), rep_name);

  const auto do_it = bin.yesno();

  if (do_it) {
    if (a()->sess().incom()) {
      qwk_receive_file(rep_path.string(), &rec, a()->user()->data.qwk_protocol);
      sleep_for(milliseconds(500));
    } else {
      bout << "Please copy the REP file to the following directory: " << wwiv::endl;
      bout << "'" << a()->sess().dirs().qwk_directory() << "'" << wwiv::endl;
      bout.pausescr();
    }

    if (rec) {
      const auto msg_name = StrCat(qwk_system_name(qwk_cfg), ".MSG");
      auto msg_path = ready_reply_packet(rep_path.string(), msg_name);
      process_reply_dat(msg_path.string());
    } else {
      sysoplog() << "Aborted";
      bout.nl();
      bout.format("{} not found", rep_name);
      bout.nl();
    }
  }
  if (save_conf) {
    tmp_disable_conf(false);
  }

  a()->set_current_user_sub_num(save_sub);
}

void qwk_email_text(const char* text, char* title, char* to) {
  strupr(to);

  // Remove text name from address, if it doesn't contain " AT " in it
  auto* st = strstr(to, " AT ");
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

  a()->sess().clear_irt();
  auto [un, sy] = parse_email_info(to);
  clear_quotes(a()->sess());

  if (un || sy) {
    net_system_list_rec* csne = nullptr;

    if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
      bout.nl();
      bout.bputs("Sorry, not enough disk space left.");
      bout.nl();
      bout.pausescr();
      return;
    }

    if (ForwardMessage(&un, &sy)) {
      bout.nl();
      bout.bputs("Mail Forwarded.");
      bout.nl();
      if ((un == 0) && (sy == 0)) {
        bout.bputs("Forwarded to unknown user.");
        bout.pausescr();
        return;
      }
    }

    if (!un && !sy) {
      return;
    }

    if (sy) {
      csne = next_system(sy);
    }

    std::string send_to_name;
    if (sy == 0) {
      set_net_num(0);
      send_to_name = a()->names()->UserName(un);
    } else {
      const std::string netname = (wwiv::stl::ssize(a()->nets()) > 1) ? a()->network_name() : "";
      send_to_name = username_system_net_as_string(un, a()->net_email_name, sy, netname);
    }

    if (sy != 0) {
      bout.nl();
      bout.format("Name of system: {}\r\n", csne->name);
      bout.format("Number of hops: {}\r\n", csne->numhops);
      bout.nl();
    }

    bout.cls();
    bout.Color(2);
    bout.format("Sending to: {}", send_to_name);
    bout.nl();
    bout.Color(2);
    bout.format("Titled    : {}", title);
    bout.nl(2);
    bout.Color(5);
    bout << "Correct? ";

    if (!bin.yesno()) {
      return;
    }

    messagerec msg{};
    msg.storage_type = EMAIL_STORAGE;

    const auto name = a()->names()->UserName(a()->usernum, a()->current_net().sysnum);
    qwk_inmsg(text, &msg, "email", name.c_str(), DateTime::now());

    if (msg.stored_as == 0xffffffff) {
      return;
    }

    bout.Color(8);

    ::EmailData email;
    email.title = title;
    email.msg = &msg;
    email.anony = 0;
    email.user_number = un;
    email.system_number = sy;
    email.an = true;
    email.from_user = a()->usernum;
    email.from_system = a()->current_net().sysnum;
    email.forwarded_code = 0;
    email.from_network_number = a()->net_num();
    sendout_email(email);
  }
}

void qwk_inmsg(const char* text, messagerec* m1, const char* aux, const char* name,
               const wwiv::core::DateTime& dt) {
  ScopeExit at_exit([=]() {
    // Might not need to do this anymore since quoting
    // isn't so convoluted.
    bin.charbufferpointer_ = 0;
    bin.charbuffer[0] = 0;
  });

  auto m = *m1;
  std::ostringstream ss;
  ss << name << "\r\n";
  ss << dt.to_string() << "\r\n";
  ss << text << "\r\n";

  auto message_text = ss.str();
  if (message_text.back() != CZ) {
    message_text.push_back(CZ);
  }
  savefile(message_text, &m, aux);
  *m1 = m;
}

void qwk_post_text(const char* text, char* title, int16_t sub) {
  messagerec m{};
  postrec p{};

  int dm, done = 0, pass = 0;
  slrec ss{};
  char user_name[101];

  while (!done && !a()->sess().hangup()) {
    if (pass > 0) {
      int done5 = 0;
      char substr[5];

      while (!done5 && !a()->sess().hangup()) {
        bout.nl();
        bout << "Then which sub?  ?=List  Q=Don't Post :";
        bin.input(substr, 3);

        StringTrim(substr);
        sub = a()->usub[to_number<int>(substr) - 1].subnum;

        if (substr[0] == 'Q') {
          return;
        } else if (substr[0] == '?') {
          SubList();
        } else {
          done5 = 1;
        }
      }
    }

    if (sub >= ssize(a()->subs().subs()) || sub < 0) {
      bout.Color(5);
      bout.bputs("Sub out of range");

      ++pass;
      continue;
    }
    a()->set_current_user_sub_num(static_cast<uint16_t>(sub));

    // Busy files... allow to retry
    while (!a()->sess().hangup()) {
      if (!qwk_iscan_literal(a()->current_user_sub_num())) {
        bout.nl();
        bout << "MSG file is busy on another instance, try again?";
        if (!bin.noyes()) {
          ++pass;
          continue;
        }
      } else {
        break;
      }
    }

    if (a()->sess().GetCurrentReadMessageArea() < 0) {
      bout.Color(5);
      bout.bputs("Sub out of range");

      ++pass;
      continue;
    }

    ss = a()->effective_slrec();

    int xa = 0;
    // User is restricked from posting
    if ((restrict_post & a()->user()->data.restrict) || (a()->user()->data.posttoday >= ss.posts)) {
      bout.nl();
      bout.bputs("Too many messages posted today.");
      bout.nl();

      ++pass;
      continue;
    }

    // User doesn't have enough sl to post on sub
    if (a()->sess().effective_sl() < a()->current_sub().postsl) {
      bout.nl();
      bout.bputs("You can't post here.");
      bout.nl();
      ++pass;
      continue;
    }

    m.storage_type = static_cast<uint8_t>(a()->current_sub().storage_type);

    if (!a()->current_sub().nets.empty()) {
      xa &= (anony_real_name);

      if (a()->user()->data.restrict & restrict_net) {
        bout.nl();
        bout.bputs("You can't post on networked sub-boards.");
        bout.nl();
        ++pass;
        continue;
      }
    }

    bout.cls();
    bout.Color(2);
    bout << "Posting    : ";
    bout.Color(3);
    bout.bputs(title);
    bout.nl();

    bout.Color(2);
    bout << "Posting on : ";
    bout.Color(3);
    bout.bputs(stripcolors(a()->current_sub().name));
    bout.nl();

    if (!a()->current_sub().nets.empty()) {
      bout.Color(2);
      bout << "Going on   : ";
      bout.Color(3);
      for (const auto& xnp : a()->current_sub().nets) {
        bout << a()->nets()[xnp.net_num].name << " ";
      }
      bout.nl();
    }

    bout.nl();
    bout.Color(5);
    bout << "Correct? ";

    if (bin.noyes()) {
      done = 1;
    } else {
      ++pass;
    }
  }
  bout.nl();

  if (a()->current_sub().anony & anony_real_name) {
    strcpy(user_name, a()->user()->GetRealName());
    properize(user_name);
  } else {
    const string name = a()->names()->UserName(a()->usernum, a()->current_net().sysnum);
    strcpy(user_name, name.c_str());
  }

  qwk_inmsg(text, &m, a()->current_sub().filename.c_str(), user_name, DateTime::now());

  if (m.stored_as != 0xffffffff) {
    while (!a()->sess().hangup()) {
      int f = qwk_iscan_literal(a()->sess().GetCurrentReadMessageArea());

      if (f == -1) {
        bout.nl();
        bout << "MSG file is busy on another instance, try again?";
        if (!bin.noyes()) {
          return;
        }
      } else {
        break;
      }
    }

    // Anonymous
    uint8_t an = 0;
    if (an) {
      bout.Color(1);
      bout << "Anonymous?";
      an = bin.yesno() ? 1 : 0;
    }
    bout.nl();

    strcpy(p.title, title);
    p.anony = an;
    p.msg = m;
    p.ownersys = 0;
    p.owneruser = static_cast<uint16_t>(a()->usernum);
    {
      a()->status_manager()->Run([&](WStatus& s) { p.qscan = s.IncrementQScanPointer(); });
    }
    p.daten = daten_t_now();
    if (a()->user()->data.restrict & restrict_validate) {
      p.status = status_unvalidated;
    } else {
      p.status = 0;
    }

    open_sub(true);

    if (!a()->current_sub().nets.empty() && a()->current_sub().anony & anony_val_net &&
        (!lcs() || !a()->sess().irt().empty())) {
      p.status |= status_pending_net;
      dm = 1;

      for (auto i = a()->GetNumMessagesInCurrentMessageArea();
           (i >= 1) && (i > (a()->GetNumMessagesInCurrentMessageArea() - 28)); i--) {
        if (get_post(i)->status & status_pending_net) {
          dm = 0;
          break;
        }
      }
      if (dm) {
        ssm(1) << "Unvalidated net posts on " << a()->current_sub().name << ".";
      }
    }

    if (a()->GetNumMessagesInCurrentMessageArea() >= a()->current_sub().maxmsgs) {
      int i = 1;
      dm = 0;
      while (dm == 0 && i <= a()->GetNumMessagesInCurrentMessageArea() && !a()->sess().hangup()) {
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

    ++a()->user()->data.msgpost;
    ++a()->user()->data.posttoday;

    a()->status_manager()->Run([](WStatus& s) {
      s.IncrementNumLocalPosts();
      s.IncrementNumMessagesPostedToday();
    });

    close_sub();

    sysoplog() << "+ '" << p.title << "' posted on '" << a()->current_sub().name;

    if (!a()->current_sub().nets.empty()) {
      ++a()->user()->data.postnet;
      if (!(p.status & status_pending_net)) {
        send_net_post(&p, a()->current_sub());
      }
    }
  }
}

static void modify_bulletins(qwk_config& qwk_cfg) {
  char s[101], t[101];

  while (!a()->sess().hangup()) {
    bout.nl();
    bout << "Add - Delete - ? List - Quit";
    bout.mpl(1);

    const int key = onek("Q\rAD?");

    switch (key) {
    case 'Q':
    case '\r':
      return;
    case 'D': {
      bout.nl();
      bout << "Which one?";
      bout.mpl(2);

      bin.input(s, 2);
      const int x = to_number<int>(s);
      // Delete the one at the right position.
      if (x >= 0 && x < ssize(qwk_cfg.bulletins)) {
        erase_at(qwk_cfg.bulletins, x);
        --qwk_cfg.amount_blts;
      }
    } break;
    case 'A': {
      bout.nl();
      bout.bputs("Enter complete path to Bulletin");
      bin.input(s, 80);

      if (!File::Exists(s)) {
        bout << "File doesn't exist, continue?";
        if (!bin.yesno()) {
          break;
        }
      }

      bout.bputs("Now enter its bulletin name, in the format BLT-????.???");
      bin.input(t, BNAME_SIZE);

      if (strncasecmp(t, "BLT-", 4) != 0) {
        bout.bputs("Improper format");
        break;
      }

      qwk_bulletin b{t, s};
      qwk_cfg.bulletins.emplace_back(b);
      ++qwk_cfg.amount_blts;
    } break;
    case '?': {
      int x = 1;
      for (const auto& b : qwk_cfg.bulletins) {
        if (bin.checka()) {
          break;
        }
        bout.bprintf("[%d] %s Is copied over from", x++, b.name);
        bout.nl();
        bout.Color(7);
        bout << string(78, '\xCD');
        bout.nl();
        bout << b.path;
        bout.nl();
      }
    } break;
    }
  }
}

void qwk_sysop() {
  if (!so()) {
    return;
  }

  auto qwk_cfg = read_qwk_cfg();

  bool done = false;
  while (!done && !a()->sess().hangup()) {
    auto sn = qwk_system_name(qwk_cfg);
    bout.cls();
    bout.bprintf("[1] Hello   file : %s\r\n", qwk_cfg.hello);
    bout.bprintf("[2] News    file : %s\r\n", qwk_cfg.news);
    bout.bprintf("[3] Goodbye file : %s\r\n", qwk_cfg.bye);
    bout.bprintf("[4] Packet name  : %s\r\n", sn);
    bout.bprintf("[5] Max messages per packet (0=No max): %d\r\n", qwk_cfg.max_msgs);
    bout.bprintf("[6] Modify Bulletins - Current amount= %d\r\n\n", qwk_cfg.amount_blts);
    bout << "Hit <Enter> or Q to save and exit: [12345<CR>] ";

    const int x = onek("Q123456\r\n");
    if (x == '1' || x == '2' || x == '3') {
      bout.nl();
      bout.Color(1);
      bout << "Enter new filename:";
      bout.mpl(12);
    }

    switch (x) {
    case '1':
      qwk_cfg.hello = bin.input(12);
      break;
    case '2':
      qwk_cfg.news = bin.input(12);
      break;
    case '3':
      qwk_cfg.bye = bin.input(12);
      break;
    case '4': {
      sn = qwk_system_name(qwk_cfg);
      write_qwk_cfg(qwk_cfg);
      bout.nl();
      bout.Color(1);
      bout.bprintf("Current name : %s", sn);
      bout.nl();
      bout << "Enter new packet name: ";
      sn = bin.input(8);
      if (!sn.empty()) {
        qwk_cfg.packet_name = sn;
      }
      write_qwk_cfg(qwk_cfg);
    } break;

    case '5': {
      bout.Color(1);
      bout << "Enter max messages per packet, 0=No Max: ";
      bout.mpl(5);
      auto tmp = bin.input(5);
      qwk_cfg.max_msgs = to_number<uint16_t>(tmp);
    } break;
    case '6':
      modify_bulletins(qwk_cfg);
      break;
    default:
      done = true;
    }
  }

  write_qwk_cfg(qwk_cfg);
}


static string qwk_current_text(int pos) {
  static const char* yesorno[] = {"YES", "NO"};

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
    bout << "|#1A|#9) Scan E-Mail                |#2" << qwk_current_text(0) << wwiv::endl;
    bout << "|#1B|#9) Delete Scanned E-Mail      |#2" << qwk_current_text(1) << wwiv::endl;
    bout << "|#1C|#9) Set N-Scan of messages     |#2" << qwk_current_text(2) << wwiv::endl;
    bout << "|#1D|#9) Remove WWIV color codes    |#2" << qwk_current_text(3) << wwiv::endl;
    bout << "|#1E|#9) Convert WWIV color to ANSI |#2" << qwk_current_text(4) << wwiv::endl;
    bout << "|#1F|#9) Pack Bulletins             |#2" << qwk_current_text(5) << wwiv::endl;
    bout << "|#1G|#9) Scan New Files             |#2" << qwk_current_text(6) << wwiv::endl;
    bout << "|#1H|#9) Remove routing information |#2" << qwk_current_text(7) << wwiv::endl;
    bout << "|#1I|#9) Archive to pack QWK with   |#2" << qwk_current_text(8) << wwiv::endl;
    bout << "|#1J|#9) Default transfer protocol  |#2" << qwk_current_text(9) << wwiv::endl;
    bout << "|#1K|#9) Max messages per pack      |#2" << qwk_current_text(10) << wwiv::endl;
    bout << "|#1Q|#9) Done" << wwiv::endl;
    bout.nl();
    bout << "|#9Select? ";
    int key = onek("QABCDEFGHIJK");

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

