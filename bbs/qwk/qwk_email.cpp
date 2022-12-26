/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "qwk_mail_packet.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/message_file.h"
#include "bbs/msgbase1.h"
#include "bbs/readmail.h"
#include "bbs/qwk/qwk_reply.h"
#include "bbs/qwk/qwk_struct.h"
#include "common/input.h"
#include "common/output.h"
#include "common/quote.h"
#include "core/file.h"
#include "local_io/wconstants.h"
#include "sdk/names.h"
#include "sdk/vardec.h"
#include "sdk/msgapi/message_utils_wwiv.h"
#include "sdk/net/networks.h"

namespace wwiv::bbs::qwk {

using namespace core;
using namespace sdk::msgapi;

void qwk_remove_email() {
  a()->emchg_ = false;

  std::vector<tmpmailrec> mloc;
  auto f(OpenEmailFile(true));

  if (!f->IsOpen()) {
    return;
  }

  const auto mfl = f->length() / sizeof(mailrec);
  uint8_t mw = 0;

  mailrec m{};
  for (unsigned long i = 0; (i < mfl) && (mw < MAXMAIL); i++) {
    f->Seek(i * sizeof(mailrec), File::Whence::begin);
    f->Read(&m, sizeof(mailrec));
    if (m.tosys == 0 && m.touser == a()->sess().user_num()) {
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
  a()->user()->data.waiting = mw;

  if (mloc.empty()) {
    return;
  }

  auto curmail = 0;
  auto done = false;
  do {
    delmail(*f, stl::at(mloc, curmail).index);

    ++curmail;
    if (curmail >= mw) {
      done = true;
    }

  } while (!a()->sess().hangup() && !done);
}

void qwk_gather_email(qwk_state* qwk_info) {
  mailrec m{};
  postrec junk{};

  a()->emchg_ = false;
  std::vector<tmpmailrec> mloc;

  auto f(OpenEmailFile(false));
  if (!f->IsOpen()) {
    bout.nl(2);
    bout.outstr("No mail file exists!");
    bout.nl();
    return;
  }
  const auto mfl = static_cast<int>(f->length() / sizeof(mailrec));
  uint8_t mw = 0;
  for (auto i = 0; i < mfl && mw < MAXMAIL; i++) {
    f->Seek(static_cast<File::size_type>(i) * sizeof(mailrec), File::Whence::begin);
    f->Read(&m, sizeof(mailrec));
    if (m.tosys == 0 && m.touser == a()->sess().user_num()) {
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
    bout.outstr("You have no mail.");
    bout.nl();
    return;
  }

  bout.ansic(7);
  bout.outstr("Gathering Email");

  auto curmail = 0;
  auto done = false;
  qwk_info->in_email = true;
  const auto index_filemode = File::modeReadWrite | File::modeAppend | File::modeBinary | File::modeCreateFile;

  const auto personal_filename = FilePath(a()->sess().dirs().qwk_directory(), "PERSONAL.NDX");
  qwk_info->personal =
      std::make_unique<DataFile<qwk_index>>(personal_filename, index_filemode);


  const auto zero_filename = FilePath(a()->sess().dirs().qwk_directory(), "000.NDX");
  qwk_info->zero = 
      std::make_unique<DataFile<qwk_index>>(zero_filename, index_filemode);

  do {
    read_same_email(mloc, mw, curmail, m, false, 0);

    strupr(m.title);
    strncpy(qwk_info->email_title, stripcolors(m.title), 25);
    // had crash in stripcolors since this won't null terminate.
    // qwk_info->email_title[25] = 0;

    //i = (ability_read_email_anony & ss.ability) != 0;

    if (m.fromsys && !m.fromuser) {
      grab_user_name(&m.msg, "email", network_number_from(&m));
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

  qwk_info->in_email = false;
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
    std::optional<net_system_list_rec> csne;

    if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
      bout.nl();
      bout.pl("Sorry, not enough disk space left.");
      bout.pausescr();
      return;
    }

    if (ForwardMessage(&un, &sy)) {
      bout.nl();
      bout.pl("Mail Forwarded.");
      if (un == 0 && sy == 0) {
        bout.pl("Forwarded to unknown user.");
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
      const auto netname = wwiv::stl::ssize(a()->nets()) > 1 ? a()->network_name() : "";
      send_to_name = username_system_net_as_string(un, a()->net_email_name, sy, netname);
    }

    if (sy != 0 && csne.has_value()) {
      bout.nl();
      bout.print("Name of system: {}\r\n", csne->name);
      bout.print("Number of hops: {}\r\n\r\n", csne->numhops);
    }

    bout.cls();
    bout.print("|#2Sending to: {}\r\n", send_to_name);
    bout.print("|#2Titled    : {}\r\n\r\n", title);
    bout.outstr("|#5Correct? ");

    if (!bin.yesno()) {
      return;
    }

    messagerec msg{};
    msg.storage_type = EMAIL_STORAGE;

    const auto name = a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum);
    qwk_inmsg(text, &msg, "email", name, DateTime::now());

    if (msg.stored_as == 0xffffffff) {
      return;
    }

    bout.ansic(8);

    EmailData email;
    email.title = title;
    email.msg = &msg;
    email.anony = 0;
    email.user_number = un;
    email.system_number = sy;
    email.an = true;
    email.set_from_user(a()->sess().user_num());
    email.set_from_system(a()->current_net().sysnum);
    email.forwarded_code = 0;
    email.from_network_number = a()->net_num();
    sendout_email(email);
  }
}

}
