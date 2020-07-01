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
#include "bbs/email.h"

#include "bbs/attach.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/inetmsg.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/quote.h"
#include "bbs/sysoplog.h"
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
#include <chrono>
#include <memory>
#include <string>

static constexpr int NUM_ATTEMPTS_TO_OPEN_EMAIL = 5;
static constexpr int DELAY_BETWEEN_EMAIL_ATTEMPTS = 9;

using std::string;
using std::stringstream;
using std::unique_ptr;
using std::chrono::seconds;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// returns true on success (i.e. the message gets forwarded)
bool ForwardMessage(uint16_t *pUserNumber, uint16_t *pSystemNumber) {
  if (*pSystemNumber) {
    return false;
  }

  User userRecord;
  a()->users()->readuser(&userRecord, *pUserNumber);
  if (userRecord.IsUserDeleted()) {
    return false;
  }
  if (userRecord.GetForwardUserNumber() == 0 && userRecord.GetForwardSystemNumber() == 0 &&
      userRecord.GetForwardSystemNumber() != INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
    return false;
  }
  if (userRecord.GetForwardSystemNumber() != 0) {
    if (!userRecord.IsMailForwardedToInternet()) {
      int network_number = a()->net_num();
      set_net_num(userRecord.GetForwardNetNumber());
      if (!valid_system(userRecord.GetForwardSystemNumber())) {
        set_net_num(network_number);
        return false;
      }
      if (!userRecord.GetForwardUserNumber()) {
        read_inet_addr(a()->net_email_name, *pUserNumber);
        if (!check_inet_addr(a()->net_email_name)) {
          return false;
        }
      }
      *pUserNumber = userRecord.GetForwardUserNumber();
      *pSystemNumber = userRecord.GetForwardSystemNumber();
      return true;
    }
    read_inet_addr(a()->net_email_name, *pUserNumber);
    *pUserNumber = 0;
    *pSystemNumber = 0;
    return false;
  }
  auto nCurrentUser = userRecord.GetForwardUserNumber();
  if (nCurrentUser == -1 || nCurrentUser == std::numeric_limits<decltype(nCurrentUser)>::max()) {
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
  a()->users()->readuser(&userRecord, nCurrentUser);
  while (userRecord.GetForwardUserNumber() || userRecord.GetForwardSystemNumber()) {
    if (userRecord.GetForwardSystemNumber()) {
      if (!valid_system(userRecord.GetForwardSystemNumber())) {
        return false;
      }
      *pUserNumber = userRecord.GetForwardUserNumber();
      *pSystemNumber = userRecord.GetForwardSystemNumber();
      set_net_num(userRecord.GetForwardNetNumber());
      return true;
    }
    if (ss[nCurrentUser]) {
      return false;
    }
    ss[nCurrentUser] = true;
    if (userRecord.IsMailboxClosed()) {
      bout << "Mailbox Closed.\r\n";
      if (so()) {
        bout << "(Forcing)\r\n";
        *pUserNumber = nCurrentUser;
        *pSystemNumber = 0;
      } else {
        *pUserNumber = 0;
        *pSystemNumber = 0;
      }
      return false;
    }
    nCurrentUser = userRecord.GetForwardUserNumber() ;
    a()->users()->readuser(&userRecord, nCurrentUser);
  }
  *pSystemNumber = 0;
  *pUserNumber = nCurrentUser;
  return true;
}

std::unique_ptr<File> OpenEmailFile(bool bAllowWrite) {
  const auto fn = FilePath(a()->config()->datadir(), EMAIL_DAT);

  // If the file doesn't exist, just return the opaque handle now instead of flailing
  // around trying to open it
  if (!File::Exists(fn)) {
    // If it does not exist, try to create it via the open call (sf bug 1215434)
    auto file = std::make_unique<File>(fn);
    file->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    return file;
  }

  auto file = std::make_unique<File>(fn);
  for (auto num = 0; num < NUM_ATTEMPTS_TO_OPEN_EMAIL; num++) {
    const auto mode = bAllowWrite ? File::modeBinary | File::modeCreateFile | File::modeReadWrite
                                  : File::modeBinary | File::modeReadOnly;
    if (file->Open(mode)) {
      return file;
    }
    sleep_for(seconds(DELAY_BETWEEN_EMAIL_ATTEMPTS));
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
        auto i1 = file_email->Read(&messageRecord, sizeof(mailrec));
        if (i1 == -1) {
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
    string b;
    if (!readfile(&(m.msg), "email", &b)) {
      return;
    }
    if (data.forwarded_code == 2) {
      // Not sure where this is ever set to 2...
      remove_link(&(m.msg), "email");
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
    unique_ptr<char[]> b1(new char[b.size() + 768]);
    int i = 0;
    if (data.user_number == 0 && data.from_network_number == a()->net_num()) {
      nh.main_type = main_type_email_name;
      strcpy(&(b1[i]), a()->net_email_name.c_str());
      i += wwiv::stl::ssize(a()->net_email_name) + 1;
    }
    strcpy(&b1[i], m.title);
    i += ssize(m.title) + 1;
    memmove(&b1[i], b.c_str(), b.length());
    nh.length = b.length() + i;
    if (nh.length > 32760) {
      bout.bprintf("Message truncated by %lu bytes for the network.", nh.length - 32760L);
      nh.length = 32760;
    }
    if (data.from_network_number != a()->net_num()) {
      gate_msg(&nh, b1.get(), 
        a()->net_num(), a()->net_email_name, 
      {}, data.from_network_number);
    } else {
      string net_filename;
      if (data.forwarded_code) {
        net_filename = StrCat(a()->network_directory(), "p1", a()->network_extension());
      } else {
        net_filename = StrCat(a()->network_directory(), "p0", a()->network_extension());
      }
      File fileNetworkPacket(net_filename);
      fileNetworkPacket.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      fileNetworkPacket.Seek(0L, File::Whence::end);
      fileNetworkPacket.Write(&nh, sizeof(net_header_rec));
      fileNetworkPacket.Write(b1.get(), nh.length);
      fileNetworkPacket.Close();
    }
  }
  string logMessage = "Mail sent to ";
  if (data.system_number == 0) {
    User userRecord;
    a()->users()->readuser(&userRecord, data.user_number);
    userRecord.SetNumMailWaiting(userRecord.GetNumMailWaiting() + 1);
    a()->users()->writeuser(&userRecord, data.user_number);
    int i;
    if (user_online(data.user_number, &i)) {
      send_inst_sysstr(i, "You just received email.");
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
        && a()->effective_sl() > a()->config()->newuser_sl()
        && userRecord.GetForwardSystemNumber() == 0
        && !data.silent_mode) {
      bout << "|#5Attach a file to this message? ";
      if (yesno()) {
        attach_file(1);
      }
    }
  } else {
    string logMessagePart;
    if ((data.system_number == 1 && a()->current_net().type == network_type_t::internet) ||
        data.system_number == INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
      logMessagePart = a()->net_email_name;
    } else {
      std::string netname = (wwiv::stl::ssize(a()->nets()) > 1) ? a()->network_name() : "";
      logMessagePart = username_system_net_as_string(data.user_number, a()->net_email_name,
                                                     data.system_number, netname);
    }
    sysoplog() << logMessage << logMessagePart;
  }

  auto status = a()->status_manager()->BeginTransaction();
  if (data.user_number == 1 && data.system_number == 0) {
    status->IncrementNumFeedbackSentToday();
    a()->user()->SetNumFeedbackSent(a()->user()->GetNumFeedbackSent() + 1);
    a()->user()->SetNumFeedbackSentToday(a()->user()->GetNumFeedbackSentToday() + 1);
  } else {
    status->IncrementNumEmailSentToday();
    a()->user()->SetNumEmailSentToday(a()->user()->GetNumEmailSentToday() + 1);
    if (data.system_number == 0) {
      a()->user()->SetNumEmailSent(a()->user()->GetNumEmailSent() + 1);
    } else {
      a()->user()->SetNumNetEmailSent(a()->user()->GetNumNetEmailSent() + 1);
    }
  }
  a()->status_manager()->CommitTransaction(std::move(status));
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
    if ((userRecord.GetSl() == 255 &&
         userRecord.GetNumMailWaiting() >
             (a()->config()->max_waiting() * 5)) ||
        (userRecord.GetSl() != 255 &&
         userRecord.GetNumMailWaiting() > a()->config()->max_waiting()) ||
        userRecord.GetNumMailWaiting() > 200) {
      if (!bForceit) {
        bout << "\r\nMailbox full.\r\n";
        return false;
      }
    }
    if (userRecord.IsUserDeleted()) {
      bout << "\r\nDeleted user.\r\n\n";
      return false;
    }
  } else {
    if (!valid_system(system_number)) {
      bout << "\r\nUnknown system number.\r\n\n";
      return false;
    }
    if (a()->user()->IsRestrictionNet()) {
      bout << "\r\nYou can't send mail off the system.\r\n";
      return false;
    }
  }
  if (!bForceit) {
    if (((user_number == 1 && system_number == 0 &&
          (a()->user()->GetNumFeedbackSentToday() >= 10)) ||
         ((user_number != 1 || system_number != 0) &&
          (a()->user()->GetNumEmailSentToday() >= a()->effective_slrec().emails)))
        && !cs()) {
      bout << "\r\nToo much mail sent today.\r\n\n";
      return false;
    }
    if (a()->user()->IsRestrictionEmail() && user_number != 1) {
      bout << "\r\nYou can't send mail.\r\n\n";
      return false;
    }
  }
  return true;
}

void email(const string& title, uint16_t user_number, uint16_t system_number, bool forceit, int anony, bool bAllowFSED) {
  auto nNumUsers = 0;
  messagerec msg{};
  string destination;
  net_system_list_rec *csne = nullptr;
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
      read_inet_addr(destination, user_number);
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
  if (system_number) {
    csne = next_system(system_number);
  }
  bool an = true;
  if (a()->effective_slrec().ability & ability_read_email_anony) {
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
    if (a()->current_net().type == network_type_t::internet
      || a()->current_net().type == network_type_t::ftn) {
      // Internet and 
      destination = a()->net_email_name;
    } else {
      std::string netname = (wwiv::stl::ssize(a()->nets()) > 1) ? a()->network_name() : "";
      destination =
          username_system_net_as_string(user_number, a()->net_email_name, system_number, netname);
    }
  }
  bout << "|#9E-mailing |#2" << destination;
  bout.nl();
  uint8_t i = (a()->effective_slrec().ability & ability_email_anony) ? anony_enable_anony : anony_none;

  if (anony & (anony_receiver_pp | anony_receiver_da)) {
    i = anony_enable_dear_abby;
  }
  if (anony & anony_receiver) {
    i = anony_enable_anony;
  }
  if (i == anony_enable_anony && a()->user()->IsRestrictionAnonymous()) {
    i = 0;
  }
  if (system_number != 0 && system_number != INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
    i = 0;
    anony = 0;
    bout.nl();
    CHECK_NOTNULL(csne);
    bout << "|#9Name of system: |#2" << csne->name << wwiv::endl;
    bout << "|#9Number of hops: |#2" << csne->numhops << wwiv::endl;
    bout.nl();
  }
  write_inst(INST_LOC_EMAIL, (system_number == 0) ? user_number : 0, INST_FLAGS_NONE);

  msg.storage_type = EMAIL_STORAGE;
  MessageEditorData data;
  data.title = title;
  data.need_title = true;
  data.fsed_flags = (bAllowFSED) ? FsedFlags::FSED : FsedFlags::NOFSED;
  data.anonymous_flag = i;
  data.aux = "email";
  data.to_name = destination;
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
    if (yesno()) {
      bool done = false;
      carbon_copy[nNumUsers].user_number = user_number;
      carbon_copy[nNumUsers].system_number = system_number;
      to_char_array(carbon_copy[nNumUsers].net_name, a()->network_name());
      to_char_array(carbon_copy[nNumUsers].net_email_name, a()->net_email_name);
      carbon_copy[nNumUsers].net_num = a()->net_num();
      nNumUsers++;
      do {
        bout << "|#9Enter Address (blank to end) : ";
        string emailAddress = input(75);
        if (emailAddress.empty()) {
          done = true;
          break;
        }
        uint16_t tu, ts;
        parse_email_info(emailAddress, &tu, &ts);
        if (tu || ts) {
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
        bcc = !yesno();
      }
    }
  }

  if (cc && !bcc) {
    int listed = 0;
    string s1 = "\003""6Carbon Copy: \003""1";
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
  email.from_user = a()->usernum;
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
  auto fwdu = user_number;
  bool fwdm = false;

  if (ForwardMessage(&user_number, &system_number)) {
    bout << "Mail forwarded.\r\n";
    fwdm = true;
  }

  if (!user_number && !system_number) {
    return;
  }

  string internet_email_address;
  if (fwdm) {
    read_inet_addr(internet_email_address, fwdu);
  }
  
  int i = 1;
  if (system_number == 0) {
    User userRecord;
    a()->users()->readuser(&userRecord, user_number);
    if (!userRecord.IsUserDeleted()) {
      bout << "|#5E-mail " << a()->names()->UserName(user_number) << "? ";
      if (!yesno()) {
        i = 0;
      }
    } else {
      i = 0;
    }
  } else {
    if (fwdm) {
      bout << "|#5E-mail " << internet_email_address << "? ";
    } else {
      bout << "|#5E-mail User " << user_number << " @" << system_number << " ? ";
    }
    if (!yesno()) {
      i = 0;
    }
  }
  clear_quotes();
  if (i) {
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
    auto t = f.length() / sizeof(mailrec);
    auto otf = false;
    for (size_t i = 0; i < t; i++) {
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
    if (user.GetNumMailWaiting()) {
      user.SetNumMailWaiting(user.GetNumMailWaiting() - 1);
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
