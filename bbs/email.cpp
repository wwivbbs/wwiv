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
#include "bbs/email.h"

#include "bbs/attach.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/connect1.h"
#include "bbs/inetmsg.h"
#include "bbs/inmsg.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/sysoplog.h"
#include "common/input.h"
#include "common/output.h"
#include "common/quote.h"
#include "core/datetime.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/fido/fido_util.h"
#include "sdk/fido/nodelist.h"
#include "sdk/net/net.h"
#include "sdk/net/networks.h"

#include <chrono>
#include <memory>
#include <string>

static constexpr int NUM_ATTEMPTS_TO_OPEN_EMAIL = 5;
static constexpr int DELAY_BETWEEN_EMAIL_ATTEMPTS = 9;

using std::chrono::seconds;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

// returns true on success (i.e. the message gets forwarded)
bool ForwardMessage(uint16_t *pUserNumber, uint16_t *pSystemNumber) {
  if (*pSystemNumber) {
    return false;
  }

  User userRecord;
  a()->users()->readuser(&userRecord, *pUserNumber);
  if (userRecord.deleted()) {
    return false;
  }
  if (userRecord.forward_usernum() == 0 && userRecord.forward_systemnum() == 0 &&
      userRecord.forward_systemnum() != INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
    return false;
  }
  if (userRecord.forward_systemnum() != 0) {
    if (!userRecord.forwarded_to_internet()) {
      const int network_number = a()->net_num();
      set_net_num(userRecord.forward_netnum());
      if (!valid_system(userRecord.forward_systemnum())) {
        set_net_num(network_number);
        return false;
      }
      if (!userRecord.forward_usernum()) {
        a()->net_email_name = read_inet_addr(*pUserNumber);
        if (!check_inet_addr(a()->net_email_name)) {
          return false;
        }
      }
      *pUserNumber = userRecord.forward_usernum();
      *pSystemNumber = userRecord.forward_systemnum();
      return true;
    }
    a()->net_email_name = read_inet_addr(*pUserNumber);
    *pUserNumber = 0;
    *pSystemNumber = 0;
    return false;
  }
  auto current_user = userRecord.forward_usernum();
  if (current_user == -1 || current_user == std::numeric_limits<decltype(current_user)>::max()) {
    bout << "Mailbox Closed.\r\n";
    if (so()) {
      bout << "(Forcing)\r\n";
    } else {
      *pUserNumber = 0;
      *pSystemNumber = 0;
    }
    return false;
  }
  std::unique_ptr<bool[]> ss(new bool[a()->config()->max_users() + 300]);

  ss[*pUserNumber] = true;
  a()->users()->readuser(&userRecord, current_user);
  while (userRecord.forward_usernum() || userRecord.forward_systemnum()) {
    if (userRecord.forward_systemnum()) {
      if (!valid_system(userRecord.forward_systemnum())) {
        return false;
      }
      *pUserNumber = userRecord.forward_usernum();
      *pSystemNumber = userRecord.forward_systemnum();
      set_net_num(userRecord.forward_netnum());
      return true;
    }
    if (ss[current_user]) {
      return false;
    }
    ss[current_user] = true;
    if (userRecord.mailbox_closed()) {
      bout << "Mailbox Closed.\r\n";
      if (so()) {
        bout << "(Forcing)\r\n";
        *pUserNumber = current_user;
        *pSystemNumber = 0;
      } else {
        *pUserNumber = 0;
        *pSystemNumber = 0;
      }
      return false;
    }
    current_user = userRecord.forward_usernum() ;
    a()->users()->readuser(&userRecord, current_user);
  }
  *pSystemNumber = 0;
  *pUserNumber = current_user;
  return true;
}

std::unique_ptr<File> OpenEmailFile(bool bAllowWrite) {
  const auto fn = FilePath(a()->config()->datadir(), EMAIL_DAT);

  if (!File::Exists(fn)) {
    // If it does not exist, try to create it via the open call (sf bug 1215434)
    auto file = std::make_unique<File>(fn);
    if (!file->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      LOG(ERROR) << "Unable to create email file: " << fn;
    }
    return file;
  }

  auto file = std::make_unique<File>(fn);
  for (auto num = 0; num < NUM_ATTEMPTS_TO_OPEN_EMAIL; num++) {
    if (file->Open(bAllowWrite ? File::modeBinary | File::modeCreateFile | File::modeReadWrite
                                  : File::modeBinary | File::modeReadOnly)) {
      return file;
    }
    sleep_for(seconds(DELAY_BETWEEN_EMAIL_ATTEMPTS));
  }
  if (!file->IsOpen()) {
    LOG(ERROR) << "Unable to open email file: " << fn;
  }
  return file;
}

void sendout_email(EmailData& data) {
  mailrec m{};
  mailrec messageRecord{};
  net_header_rec nh{};

  to_char_array(m.title, data.title);
  m.msg = *data.msg;
  m.anony = data.anony;
  if (data.from_system == a()->current_net().sysnum) {
    m.fromsys = 0;
  } else {
    m.fromsys = data.from_system;
  }
  m.fromuser = data.from_user;
  m.tosys = data.system_number;
  m.touser = data.user_number;
  m.status = 0;
  m.daten = daten_t_now();

  if (m.fromsys && wwiv::stl::ssize(a()->nets()) > 1) {
    m.status |= status_new_net;
    // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
    m.title[71] = '\0';
    m.network.network_msg.net_number = static_cast<int8_t>(data.from_network_number);
  }

  if (data.system_number == 0) {
    auto file_email(OpenEmailFile(true));
    if (!file_email->IsOpen()) {
      return;
    }
    auto nEmailFileLen = file_email->length() / sizeof(mailrec);
    File::size_type i = 0;
    if (nEmailFileLen != 0) {
      i = nEmailFileLen - 1;
      file_email->Seek(i * sizeof(mailrec), File::Whence::begin);
      file_email->Read(&messageRecord, sizeof(mailrec));
      while (i > 0 && messageRecord.tosys == 0 && messageRecord.touser == 0) {
        --i;
        file_email->Seek(i * sizeof(mailrec), File::Whence::begin);
        if (auto i1 = file_email->Read(&messageRecord, sizeof(mailrec)); i1 == -1) {
          bout << "|#6DIDN'T READ WRITE!\r\n";
        }
      }
      if (messageRecord.tosys || messageRecord.touser) {
        ++i;
      }
    }

    file_email->Seek(i * sizeof(mailrec), File::Whence::begin);
    auto bytes_written = file_email->Write(&m, sizeof(mailrec));
    file_email->Close();
    if (bytes_written == -1) {
      bout << "|#6DIDN'T SAVE RIGHT!\r\n";
    }
  } else {
    auto o = readfile(&m.msg, "email"); 
    if (!o) {
      return;
    }
    if (data.forwarded_code == 2) {
      // Not sure where this is ever set to 2...
      remove_link(&m.msg, "email");
    }
    nh.tosys  = static_cast<uint16_t>(data.system_number);
    nh.touser = static_cast<uint16_t>(data.user_number);
    if (data.from_system > 0) {
      nh.fromsys = static_cast<uint16_t>(data.from_system);
    } else {
      nh.fromsys = a()->current_net().sysnum;
    }
    nh.fromuser = static_cast<uint16_t>(data.from_user);
    nh.main_type = main_type_email;
    nh.minor_type = 0;
    nh.list_len = 0;
    nh.daten = m.daten;
    nh.method = 0;
    auto b = o.value();
    auto b1 = std::make_unique<char[]>(b.size() + 768);
    int i = 0;
    if (data.user_number == 0 && data.from_network_number == a()->net_num()) {
      nh.main_type = main_type_email_name;
      strcpy(&b1[i], a()->net_email_name.c_str());
      i += wwiv::stl::size_int(a()->net_email_name) + 1;
    }
    strcpy(&b1[i], m.title);
    i += ssize(m.title) + 1;
    memmove(&b1[i], b.c_str(), b.length());
    nh.length = wwiv::stl::size_int(b) + i;
    if (nh.length > 32760) {
      bout.bprintf("Message truncated by %lu bytes for the network.", nh.length - 32760L);
      nh.length = 32760;
    }
    if (data.from_network_number != a()->net_num()) {
      gate_msg(&nh, b1.get(), 
        a()->net_num(), a()->net_email_name, 
      {}, data.from_network_number);
    } else {
      const auto fn =
          fmt::format("{}{}", data.forwarded_code ? "p1" : "p0", a()->network_extension());
      File packet(FilePath(a()->network_directory(), fn));
      packet.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      packet.Seek(0L, File::Whence::end);
      packet.Write(&nh, sizeof(net_header_rec));
      packet.Write(b1.get(), nh.length);
      packet.Close();
    }
  }
  std::string logMessage = "Mail sent to ";
  if (data.system_number == 0) {
    User userRecord;
    a()->users()->readuser(&userRecord, data.user_number);
    userRecord.email_waiting(userRecord.email_waiting() + 1);
    a()->users()->writeuser(&userRecord, data.user_number);
    if (const auto instance = user_online(data.user_number)) {
      send_inst_sysstr(instance.value(), "You just received email.");
    }
    if (data.an) {
      logMessage += a()->names()->UserName(data.user_number);
      sysoplog() << logMessage;
    } else {
      auto tempLogMessage = StrCat(logMessage, a()->names()->UserName(data.user_number));
      sysoplog() << tempLogMessage;
      logMessage += ">UNKNOWN<";
    }
    if (data.system_number == 0 
        && a()->sess().effective_sl() >= a()->config()->validated_sl()
        && userRecord.forward_systemnum() == 0
        && !data.silent_mode) {
      bout << "|#5Attach a file to this message? ";
      if (bin.yesno()) {
        attach_file(1);
      }
    }
  } else {
    std::string logMessagePart;
    if ((data.system_number == 1 && a()->current_net().type == network_type_t::internet) ||
        data.system_number == INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
      logMessagePart = a()->net_email_name;
    } else {
      auto netname = (wwiv::stl::ssize(a()->nets()) > 1) ? a()->network_name() : "";
      logMessagePart = username_system_net_as_string(data.user_number, a()->net_email_name,
                                                     data.system_number, netname);
    }
    sysoplog() << logMessage << logMessagePart;
  }

  a()->status_manager()->Run([&](Status& status) {
    if (data.user_number == 1 && data.system_number == 0) {
      status.increment_feedback_today();
      a()->user()->feedback_sent(a()->user()->feedback_sent() + 1);
      a()->user()->feedback_today(a()->user()->feedback_today() + 1);
    } else {
      status.increment_email_today();
      a()->user()->email_today(a()->user()->email_today() + 1);
      if (data.system_number == 0) {
        a()->user()->email_sent(a()->user()->email_sent() + 1);
      } else {
        a()->user()->email_net(a()->user()->email_net() + 1);
      }
    }
  });
  if (!data.silent_mode) {
    bout.Color(3);
    bout << logMessage;
    bout.nl();
  }
}

bool ok_to_mail(uint16_t user_number, uint16_t system_number, bool bForceit) {
  if (system_number != 0 && a()->current_net().sysnum == 0) {
    bout << "\r\nSorry, this system is not a part of WWIVnet.\r\n\n";
    return false;
  }
  if (system_number == 0) {
    if (user_number == 0) {
      return false;
    }
    User userRecord;
    a()->users()->readuser(&userRecord, user_number);
    if ((userRecord.sl() == 255 &&
         userRecord.email_waiting() >
             (a()->config()->max_waiting() * 5)) ||
        (userRecord.sl() != 255 &&
         userRecord.email_waiting() > a()->config()->max_waiting()) ||
        userRecord.email_waiting() > 200) {
      if (!bForceit) {
        bout << "\r\nMailbox full.\r\n";
        return false;
      }
    }
    if (userRecord.deleted()) {
      bout << "\r\nDeleted user.\r\n\n";
      return false;
    }
  } else {
    if (!valid_system(system_number)) {
      bout << "\r\nUnknown system number.\r\n\n";
      return false;
    }
    if (a()->user()->restrict_net()) {
      bout << "\r\nYou can't send mail off the system.\r\n";
      return false;
    }
  }
  if (!bForceit) {
    if (((user_number == 1 && system_number == 0 &&
          (a()->user()->feedback_today() >= 10)) ||
         ((user_number != 1 || system_number != 0) &&
          (a()->user()->email_today() >= a()->config()->sl(a()->sess().effective_sl()).emails)))
        && !cs()) {
      bout << "\r\nToo much mail sent today.\r\n\n";
      return false;
    }
    if (a()->user()->restrict_email() && user_number != 1) {
      bout << "\r\nYou can't send mail.\r\n\n";
      return false;
    }
  }
  return true;
}

void email(const std::string& title, uint16_t user_number, uint16_t system_number, bool forceit, int anony, bool bAllowFSED) {
  auto nNumUsers = 0;
  messagerec msg{};
  std::string destination;
  struct {
    uint16_t user_number;
    uint16_t system_number;
    int net_num;
    char net_name[20], net_email_name[40];
  } carbon_copy[20];

  auto cc = false, bcc = false;

  if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
    bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  bout.nl();
  if (ForwardMessage(&user_number, &system_number)) {
    if (system_number == INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
      destination = read_inet_addr(user_number);
    }
    bout << "\r\nMail Forwarded.\r\n\n";
    if (user_number == 0 && system_number == 0) {
      bout << "Forwarded to unknown user.\r\n";
      return;
    }
  }
  if (!user_number && !system_number) {
    return;
  }
  if (!ok_to_mail(user_number, system_number, forceit)) {
    return;
  }
  std::string destination_bbs_name;
  std::optional<net_system_list_rec> csne;
  if (system_number) {
    csne = next_system(system_number);
    destination_bbs_name = csne->name;
  }
  bool an = true;
  if (a()->config()->sl(a()->sess().effective_sl()).ability & ability_read_email_anony) {
    an = true;
  } else if (anony & (anony_sender | anony_sender_da | anony_sender_pp)) {
    an = false;
  } else {
    an = true;
  }
  if (system_number == 0) {
    set_net_num(0);
    if (an) {
      destination = a()->names()->UserName(user_number);
    } else {
      destination = ">UNKNOWN<";
    }
  } else {
    if (a()->current_net().type == network_type_t::internet) {
      destination = a()->net_email_name;
    } else if (a()->current_net().type == network_type_t::ftn) {
      destination = a()->net_email_name;
      try {
        auto addr = fido::get_address_from_single_line(destination);
        if (addr.zone() == -1) {
          bout << "Bad FTN Address: " << destination;
          return;
        }
        if (auto & net = a()->mutable_current_net(); net.try_load_nodelist()) {
          if (auto & nl = *net.nodelist; nl.contains(addr)) {
            const auto& e = nl.entry(addr);
            destination_bbs_name = e.name();
          } else {
            bout.format("|#6Address '|#2{}|#6' does not existing in the nodelist.\r\n", addr);
            bout.nl(2);
            bout.bputs("|#5Are you sure you want to send to this address? ");
            if (!bin.noyes()) {
              return;
            }
          }
        } else {
          bout << "Unable to validate FTN address against nodelist." << wwiv::endl;
          sysoplog() << "WARNING: Unable to validate FTN address against nodelist.";
          sysoplog() << "Nodelist base: " << net.fido.nodelist_base
                     << " does not exist in netdir: " << net.dir;
        }
      } catch (const fido::bad_fidonet_address& e) {
        bout << "Bad FTN Address: " << destination << "; error: " << e.what();
        return;
      }
    } else {
      std::string netname = (wwiv::stl::ssize(a()->nets()) > 1) ? a()->network_name() : "";
      destination =
          username_system_net_as_string(user_number, a()->net_email_name, system_number, netname);
    }
  }
  bout << "|#9E-mailing:      |#2" << destination;
  if (system_number != 0) {
    bout.format(" |#9(|#1{}|#9)", a()->current_net().name); 
  }
  bout.nl();
  uint8_t i = (a()->config()->sl(a()->sess().effective_sl()).ability & ability_email_anony) ? anony_enable_anony : anony_none;

  if (anony & (anony_receiver_pp | anony_receiver_da)) {
    i = anony_enable_dear_abby;
  }
  if (anony & anony_receiver) {
    i = anony_enable_anony;
  }
  if (i == anony_enable_anony && a()->user()->restrict_anony()) {
    i = 0;
  }
  if (system_number != 0) {
    i = 0;
    anony = 0;
    bout << "|#9Name of system: |#2" << destination_bbs_name << wwiv::endl;
    if (a()->current_net().type == network_type_t::wwivnet) {
      bout.nl();
      CHECK(csne.has_value());
      bout << "|#9Number of hops: |#2" << csne->numhops << wwiv::endl;
      bout.nl();
    }
  }
  write_inst(INST_LOC_EMAIL, (system_number == 0) ? user_number : 0, INST_FLAGS_NONE);

  msg.storage_type = EMAIL_STORAGE;
  MessageEditorData data(a()->user()->name_and_number());
  data.title = title;
  data.need_title = true;
  data.fsed_flags = (bAllowFSED) ? FsedFlags::FSED : FsedFlags::NOFSED;
  data.anonymous_flag = i;
  data.aux = "email";
  data.to_name = destination;
  data.sub_name = "WWIV E-mail";
  data.msged_flags = MSGED_FLAG_NONE;
  if (!inmsg(data)) {
    return;
  }
  savefile(data.text, &msg, data.aux);
  i = data.anonymous_flag;
  if (anony & anony_sender) {
    i |= anony_receiver;
  }
  if (anony & anony_sender_da) {
    i |= anony_receiver_da;
  }
  if (anony & anony_sender_pp) {
    i |= anony_receiver_pp;
  }

  if (a()->IsCarbonCopyEnabled()) {
    bout.nl();
    bout << "|#9Copy this mail to others? ";
    nNumUsers = 0;
    if (bin.yesno()) {
      bool done = false;
      carbon_copy[nNumUsers].user_number = user_number;
      carbon_copy[nNumUsers].system_number = system_number;
      to_char_array(carbon_copy[nNumUsers].net_name, a()->network_name());
      to_char_array(carbon_copy[nNumUsers].net_email_name, a()->net_email_name);
      carbon_copy[nNumUsers].net_num = a()->net_num();
      nNumUsers++;
      do {
        bout << "|#9Enter Address (blank to end) : ";
        std::string emailAddress = bin.input(75);
        if (emailAddress.empty()) {
          done = true;
          break;
        }
        if (auto [tu, ts] = parse_email_info(emailAddress); tu || ts) {
          carbon_copy[nNumUsers].user_number = tu;
          carbon_copy[nNumUsers].system_number = ts;
          to_char_array(carbon_copy[nNumUsers].net_name, a()->network_name());
          to_char_array(carbon_copy[nNumUsers].net_email_name, a()->net_email_name);
          carbon_copy[nNumUsers].net_num = a()->net_num();
          nNumUsers++;
          cc = true;
        }
        if (nNumUsers == 20) {
          bout << "|#6Maximum number of addresses reached!";
          done = true;
        }
      } while (!done);
      if (cc) {
        bout << "|#9Show Recipients in message? ";
        bcc = !bin.yesno();
      }
    }
  }

  if (cc && !bcc) {
    int listed = 0;
    std::string s1 = "\003""6Carbon Copy: \003""1";
    lineadd(&msg, "\003""7----", "email");
    for (int j = 0; j < nNumUsers; j++) {
      if (carbon_copy[j].system_number == 0) {
        set_net_num(0);
        destination = a()->names()->UserName(carbon_copy[j].user_number);
      } else {
        if (carbon_copy[j].system_number == 1 &&
            carbon_copy[j].user_number == 0 &&
            a()->nets()[carbon_copy[j].net_num].type == network_type_t::internet) {
          destination = carbon_copy[j].net_email_name;
        } else {
          set_net_num(carbon_copy[j].net_num);
          if (wwiv::stl::ssize(a()->nets()) > 1) {
            if (carbon_copy[j].user_number == 0) {
              destination = fmt::sprintf("%s@%u.%s", carbon_copy[j].net_email_name, carbon_copy[j].system_number,
                      carbon_copy[j].net_name);
            } else {
              destination = fmt::sprintf("#%u@%u.%s", carbon_copy[j].user_number, carbon_copy[j].system_number, carbon_copy[j].net_name);
            }
          } else {
            if (carbon_copy[j].user_number == 0) {
              destination = fmt::sprintf("%s@%u", carbon_copy[j].net_email_name, carbon_copy[j].system_number);
            } else {
              destination = fmt::sprintf("#%u@%u", carbon_copy[j].user_number, carbon_copy[j].system_number);
            }
          }
        }
      }
      if (j == 0) {
        s1 = StrCat("\003""6Original To: \003""1", destination);
        lineadd(&msg, s1, "email");
        s1 = "\003""6Carbon Copy: \003""1";
      } else {
        if (s1.length() + destination.length() < 77) {
          s1 += destination;
          if (j + 1 < nNumUsers) {
            s1 += ", ";
          } else {
            s1 += "  ";
          }
          listed = 0;
        } else {
          lineadd(&msg, s1, "email");
          s1 += "\003""1             ";
          j--;
          listed = 1;
        }
      }
    }
    if (!listed) {
      lineadd(&msg, s1, "email");
    }
  }

  EmailData email(data);
  email.msg = &msg;
  email.anony = i;
  email.an = an;
  email.set_from_user(a()->sess().user_num());
  email.from_system = a()->current_net().sysnum;
  email.forwarded_code = 0;

  if (!cc) {
    email.system_number = system_number;
    email.user_number = user_number;
    email.from_network_number = a()->net_num();
    sendout_email(email);
    return;
  }
  for (int counter = 0; counter < nNumUsers; counter++) {
    set_net_num(carbon_copy[counter].net_num);
    email.user_number = carbon_copy[counter].user_number;
    email.system_number = carbon_copy[counter].system_number;
    email.from_network_number = carbon_copy[counter].net_num;
    sendout_email(email);
  }
}

void imail(const std::string& title, uint16_t user_number, uint16_t system_number) {
  bool fwdm = false;

  if (ForwardMessage(&user_number, &system_number)) {
    bout << "Mail forwarded.\r\n";
    fwdm = true;
  }

  if (!user_number && !system_number) {
    return;
  }

  std::string internet_email_address;
  if (fwdm) {
    internet_email_address = read_inet_addr(user_number);
  }
  
  bool doit = true;
  if (system_number == 0) {
    if (auto user = a()->users()->readuser(user_number, UserManager::mask::non_deleted)) {
      bout << "|#5E-mail " << user->name_and_number() << "? ";
      if (!bin.yesno()) {
        doit = false;
      }
    } else {
      doit = false;
    }
  } else {
    if (fwdm) {
      bout << "|#5E-mail " << internet_email_address << "? ";
    } else {
      bout << "|#5E-mail User " << user_number << " @" << system_number << " ? ";
    }
    if (!bin.yesno()) {
      doit = false;
    }
  }
  clear_quotes(a()->sess());
  if (doit) {
    email(title, user_number, system_number, false, 0);
  }
}

void delmail(File& f, size_t loc) {
  mailrec m{};
  User user;

  f.Seek(loc * sizeof(mailrec), File::Whence::begin);
  f.Read(&m, sizeof(mailrec));

  if (m.touser == 0 && m.tosys == 0) {
    return;
  }

  bool rm = true;
  if (m.status & status_multimail) {
    const auto num_records = f.length() / sizeof(mailrec);
    auto otf = false;
    for (size_t i = 0; i < num_records; i++) {
      if (i != loc) {
        mailrec m1{};
        f.Seek(i * sizeof(mailrec), File::Whence::begin);
        f.Read(&m1, sizeof(mailrec));
        if (m.msg.stored_as == m1.msg.stored_as && 
            m.msg.storage_type == m1.msg.storage_type &&
            m1.daten != 0xffffffff) {
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
    a()->users()->readuser(&user, m.touser);
    if (user.email_waiting()) {
      user.email_waiting(user.email_waiting() - 1);
      a()->users()->writeuser(&user, m.touser);
    }
  }
  f.Seek(loc * sizeof(mailrec), File::Whence::begin);
  m.touser = 0;
  m.tosys = 0;
  m.daten = 0xffffffff;
  m.msg.storage_type = 0;
  m.msg.stored_as = 0xffffffff;
  f.Write(&m, sizeof(mailrec));
}

std::string fixup_user_entered_email(const std::string& user_input) {
  if (user_input.empty()) {
    return{};
  }
  if (const auto at_pos = user_input.find('@'); at_pos != std::string::npos &&
                                                at_pos < user_input.size() - 1 &&
                                                isalpha(user_input.at(at_pos + 1))) {
    if (!contains(user_input, INTERNET_EMAIL_FAKE_OUTBOUND_ADDRESS)) {
      return StrCat(ToStringLowerCase(user_input), " ", INTERNET_EMAIL_FAKE_OUTBOUND_ADDRESS);
    }
    return user_input;
  }

  const auto first = user_input.find_last_of('(');
  const auto last = user_input.find_last_of(')');
  if (first != std::string::npos && last != std::string::npos) {
    // This is where we'd check for (NNNN) and add in the @NNN for the FTN networks.
    if (last > first) {
      const auto inner = user_input.substr(first + 1, last - first - 1);
      if (wwiv::stl::contains(inner, '/') && !contains(user_input, FTN_FAKE_OUTBOUND_ADDRESS)) {
        // At least need a FTN address.
        return StrCat(user_input, " ", FTN_FAKE_OUTBOUND_ADDRESS);
        //bout << "\r\n|#9Sending to FTN Address: |#2" << inner << wwiv::endl;
      }
    }
  }
  return user_input;
}
