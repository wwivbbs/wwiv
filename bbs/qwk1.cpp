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
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/message_file.h"
#include "bbs/msgbase1.h"
#include "bbs/pause.h"
#include "bbs/quote.h"
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

  } while (!a()->hangup_ && !done);
}

void qwk_gather_email(qwk_junk* qwk_info) {
  int i;
  mailrec m{};
  postrec junk{};

  a()->emchg_ = false;
  std::vector<tmpmailrec> mloc;

  const auto ss = a()->effective_slrec();
  auto f(OpenEmailFile(false));
  if (!f->IsOpen()) {
    bout.nl(2);
    bout.bputs("No mail file exists!");
    bout.nl();
    return;
  }
  const int mfl = f->length() / sizeof(mailrec);
  uint8_t mw = 0;
  for (i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
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

  auto filename = PathFilePath(a()->qwk_directory(), "PERSONAL.NDX");
  qwk_info->personal = open(filename.string().c_str(), O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  filename = PathFilePath(a()->qwk_directory(), "000.NDX");
  qwk_info->zero = open(filename.string().c_str(), O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

  do {
    read_same_email(mloc, mw, curmail, m, 0, 0);

    strupr(m.title);
    strncpy(qwk_info->email_title, stripcolors(m.title), 25);
    // had crash in stripcolors since this won't null terminate.
    // qwk_info->email_title[25] = 0;

    i = (ability_read_email_anony & ss.ability) != 0;

    if ((m.fromsys) && (!m.fromuser)) {
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

  } while ((!a()->hangup_) && (!done));

  qwk_info->in_email = 0;
}

int select_qwk_archiver(struct qwk_junk* qwk_info, int ask) {
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
    strcpy(temp, a()->arcs[x].extension);
    StringTrim(temp);

    if (temp[0]) {
      sprintf(temp, "%d", x + 1);
      strcat(allowed, temp);
      bout << fmt::sprintf("1%d) 3%s", x + 1, a()->arcs[x].extension);
      bout.nl();
    }
  }
  bout.nl();
  bout << "Enter #  Q to Quit <CR>=1 :";

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
  if (a()->user()->data.qwk_archive > 4) {
    a()->user()->data.qwk_archive = 0;
  }

  if (a()->arcs[a()->user()->data.qwk_archive - 1].extension[0] == 0) {
    a()->user()->data.qwk_archive = 0;
  }

  if (a()->user()->data.qwk_archive == 0) {
    return string("ASK");
  } else {
    return string((a()->arcs[a()->user()->data.qwk_archive - 1].extension));
  }
}

string qwk_which_protocol() {
  if (a()->user()->data.qwk_protocol == 1) {
    a()->user()->data.qwk_protocol = 0;
  }

  if (a()->user()->data.qwk_protocol == 0) {
    return string("ASK");
  } else {
    string thisprotocol(prot_name(a()->user()->data.qwk_protocol));
    if (thisprotocol.size() > 22) {
      return thisprotocol.substr(0, 21);
    }
    return thisprotocol;
  }
}

static void qwk_receive_file(const std::string& fn, bool* received, int i) {
  if (i <= 1 || i == 5) {
    i = get_protocol(xfertype::xf_up_temp);
  }

  switch (i) {
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
    maybe_internal(fn, received, nullptr, false, i);
    break;
  default:
    if (a()->context().incom()) {
      extern_prot(i - WWIV_NUM_INTERNAL_PROTOCOLS, fn, 0);
      *received = File::Exists(fn);
    }
    break;
  }
}

static void ready_reply_packet(const std::string& packet_name, const std::string& msg_name) {
  const auto archiver = match_archiver(packet_name);
  const auto command = stuff_in(a()->arcs[archiver].arce, packet_name, msg_name, "", "", "");

  File::set_current_directory(a()->qwk_directory());
  ExecuteExternalProgram(command, EFLAG_NONE);
  a()->CdHome();
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

  return make_text_ready(text, sizeof(qwk_record) * blocks);
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
    pausescr();
    return;
  }

  lseek(repfile, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
  read(repfile, &qwk, sizeof(qwk_record));

  // Should check to makesure first block contains our bbs id
  ++curpos;

  bout.cls();

  while (!done && !a()->hangup_) {
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
        bout << fmt::sprintf("Message '2%s1' is marked 3PRIVATE", title);
        bout.nl();
        bout.Color(1);
        bout << fmt::sprintf("It is addressed to 2%s", to);
        bout.nl(2);
        bout.Color(7);
        bout << "Route into E-Mail?";
        if (noyes()) {
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
          bout << fmt::sprintf("1) %s", to);
          bout.nl();
          bout.Color(3);
          bout << fmt::sprintf("2) %s", to_from_msg_opt.value());
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
        pausescr();
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
  if ((a()->uconfsub[1].confnum != -1) && (okconf(a()->user()))) {
    save_conf = 1;
    tmp_disable_conf(true);
  }

  auto name = StrCat(qwk_system_name(qwk_cfg), ".REP");

  bout << fmt::format("Hit 'Y' to upload reply packet {} :", name);
  const auto namepath = PathFilePath(a()->qwk_directory(), name);

  const bool do_it = yesno();

  if (do_it) {
    if (a()->context().incom()) {
      qwk_receive_file(namepath.string(), &rec, a()->user()->data.qwk_protocol);
      sleep_for(milliseconds(500));
    }

    if (rec) {
      name = StrCat(qwk_system_name(qwk_cfg), ".MSG");
      ready_reply_packet(namepath.string(), name);
      process_reply_dat(namepath.string());
    } else {
      sysoplog() << "Aborted";
      bout.nl();
      bout << fmt::sprintf("%s not found", name);
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
  auto st = strstr(to, " AT ");
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

  a()->context().clear_irt();
  uint16_t sy, un;
  parse_email_info(to, &un, &sy);
  clear_quotes();

  if (un || sy) {
    messagerec msg{};
    net_system_list_rec* csne = nullptr;

    if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
      bout.nl();
      bout.bputs("Sorry, not enough disk space left.");
      bout.nl();
      pausescr();
      return;
    }

    if (ForwardMessage(&un, &sy)) {
      bout.nl();
      bout.bputs("Mail Forwarded.");
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

    std::string send_to_name;
    if (sy == 0) {
      set_net_num(0);
      send_to_name = a()->names()->UserName(un);
    } else {
      const std::string netname = (wwiv::stl::ssize(a()->net_networks) > 1) ? a()->network_name() : "";
      send_to_name = username_system_net_as_string(un, a()->net_email_name, sy, netname);
    }

    if (sy != 0) {
      bout.nl();
      bout << fmt::format("Name of system: {}", csne->name) << wwiv::endl;
      bout << fmt::format("Number of hops: {}", csne->numhops);
      bout.nl(2);
    }

    bout.cls();
    bout.Color(2);
    bout << fmt::format("Sending to: {}", send_to_name);
    bout.nl();
    bout.Color(2);
    bout << fmt::format("Titled    : {}", title);
    bout.nl(2);
    bout.Color(5);
    bout << "Correct? ";

    if (!yesno()) {
      return;
    }

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
  wwiv::core::ScopeExit at_exit([=]() {
    // Might not need to do this anymore since quoting
    // isn't so convoluted.
    bout.charbufferpointer_ = 0;
    bout.charbuffer[0] = 0;
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

  int dm, f, done = 0, pass = 0;
  slrec ss{};
  char user_name[101];

  while (!done && !a()->hangup_) {
    if (pass > 0) {
      int done5 = 0;
      char substr[5];

      while (!done5 && !a()->hangup_) {
        bout.nl();
        bout << "Then which sub?  ?=List  Q=Don't Post :";
        input(substr, 3);

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
    while (!a()->hangup_) {
      if (!qwk_iscan_literal(a()->current_user_sub_num())) {
        bout.nl();
        bout << "MSG file is busy on another instance, try again?";
        if (!noyes()) {
          ++pass;
          continue;
        }
      } else {
        break;
      }
    }

    if (a()->GetCurrentReadMessageArea() < 0) {
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
    if (a()->effective_sl() < a()->current_sub().postsl) {
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
        bout << a()->net_networks[xnp.net_num].name << " ";
      }
      bout.nl();
    }

    bout.nl();
    bout.Color(5);
    bout << "Correct? ";

    if (noyes()) {
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
    while (!a()->hangup_) {
      f = qwk_iscan_literal(a()->GetCurrentReadMessageArea());

      if (f == -1) {
        bout.nl();
        bout << "MSG file is busy on another instance, try again?";
        if (!noyes()) {
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
      an = yesno() ? 1 : 0;
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

    if ((!a()->current_sub().nets.empty()) && (a()->current_sub().anony & anony_val_net) &&
        (!lcs() || !a()->context().irt().empty())) {
      p.status |= status_pending_net;
      dm = 1;

      for (int i = a()->GetNumMessagesInCurrentMessageArea();
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
      while ((dm == 0) && (i <= a()->GetNumMessagesInCurrentMessageArea()) && !a()->hangup_) {
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

    {
      a()->status_manager()->Run([](WStatus& s) {
        s.IncrementNumLocalPosts();
        s.IncrementNumMessagesPostedToday();
      });
    }

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

  auto done = false;
  while (!done && !a()->hangup_) {
    bout.nl();
    bout << "Add - Delete - ? List - Quit";
    bout.mpl(1);

    int key = onek("Q\rAD?");

    switch (key) {
    case 'Q':
    case '\r':
      return;
    case 'D': {
      bout.nl();
      bout << "Which one?";
      bout.mpl(2);

      input(s, 2);
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
      input(s, 80);

      if (!File::Exists(s)) {
        bout << "File doesn't exist, continue?";
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

      qwk_bulletin b{t, s};
      qwk_cfg.bulletins.emplace_back(b);
      ++qwk_cfg.amount_blts;
    } break;
    case '?': {
      int x = 1;
      for (const auto& b : qwk_cfg.bulletins) {
        if (checka()) { break; }
        bout << fmt::sprintf("[%d] %s Is copied over from", x++, b.name);
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
  while (!done && !a()->hangup_) {
    auto sn = qwk_system_name(qwk_cfg);
    bout.cls();
    bout << fmt::sprintf("[1] Hello   file : %s\r\n", qwk_cfg.hello);
    bout << fmt::sprintf("[2] News    file : %s\r\n", qwk_cfg.news);
    bout << fmt::sprintf("[3] Goodbye file : %s\r\n", qwk_cfg.bye);
    bout << fmt::sprintf("[4] Packet name  : %s\r\n", sn);
    bout << fmt::sprintf("[5] Max messages per packet (0=No max): %d\r\n", qwk_cfg.max_msgs);
    bout << fmt::sprintf("[6] Modify Bulletins - Current amount= %d\r\n\n", qwk_cfg.amount_blts);
    bout << "Hit <Enter> or Q to save and exit: [12345<CR>] ";

    int x = onek("Q123456\r\n");
    if (x == '1' || x == '2' || x == '3') {
      bout.nl();
      bout.Color(1);
      bout << "Enter new filename:";
      bout.mpl(12);
    }

    switch (x) {
    case '1':
      qwk_cfg.hello = input(12);
      break;
    case '2':
      qwk_cfg.news = input(12);
      break;
    case '3':
      qwk_cfg.bye = input(12);
      break;
    case '4': {
      sn = qwk_system_name(qwk_cfg);
      write_qwk_cfg(qwk_cfg);
      bout.nl();
      bout.Color(1);
      bout << fmt::sprintf("Current name : %s", sn);
      bout.nl();
      bout << "Enter new packet name: ";
      sn = input(8);
      if (!sn.empty()) {
        qwk_cfg.packet_name = sn;
      }
      write_qwk_cfg(qwk_cfg);
    } break;

    case '5': {
      bout.Color(1);
      bout << "Enter max messages per packet, 0=No Max: ";
      bout.mpl(5);
      auto tmp = input(5);
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
    if (a()->user()->data.qwk_dont_scan_mail) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }
  case 1:
    if (a()->user()->data.qwk_delete_mail) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }
  case 2:
    if (a()->user()->data.qwk_dontsetnscan) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }
  case 3:
    if (a()->user()->data.qwk_remove_color) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }
  case 4:
    if (a()->user()->data.qwk_convert_color) {
      return yesorno[0];
    } else {
      return yesorno[1];
    }

  case 5:
    if (a()->user()->data.qwk_leave_bulletin) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 6:
    if (a()->user()->data.qwk_dontscanfiles) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 7:
    if (a()->user()->data.qwk_keep_routing) {
      return yesorno[1];
    } else {
      return yesorno[0];
    }

  case 8:
    return qwk_which_zip();

  case 9:
    return qwk_which_protocol();

  case 10:
    if (!a()->user()->data.qwk_max_msgs_per_sub && !a()->user()->data.qwk_max_msgs) {
      return "Unlimited/Unlimited";
    } else if (!a()->user()->data.qwk_max_msgs_per_sub) {
      return fmt::format("Unlimited/{}", a()->user()->data.qwk_max_msgs);
    } else if (!a()->user()->data.qwk_max_msgs) {
      return fmt::format("{}/Unlimited", a()->user()->data.qwk_max_msgs_per_sub);
    } else {
      return fmt::format("{}/{}", a()->user()->data.qwk_max_msgs,
                         a()->user()->data.qwk_max_msgs_per_sub);
    }

  case 11:
    return string("DONE");
  }

  return nullptr;
}

void config_qwk_bw() {
  bool done = false;

  while (!done && !a()->hangup_) {
    bout << "A) Scan E-Mail " << qwk_current_text(0);
    bout.nl();
    bout << "B) Delete Scanned E-Mail " << qwk_current_text(1);
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

