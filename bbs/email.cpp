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
#include "bbs/email.h"

#include <chrono>
#include <memory>
#include <string>

#include "bbs/attach.h"
#include "bbs/instmsg.h"
#include "bbs/inmsg.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/read_message.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "sdk/status.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"
#include "sdk/user.h"


#define NUM_ATTEMPTS_TO_OPEN_EMAIL 5
#define DELAY_BETWEEN_EMAIL_ATTEMPTS 9

#define GAT_NUMBER_ELEMENTS 2048
#define GAT_SECTION_SIZE    4096
#define MSG_BLOCK_SIZE      512

using std::string;
using std::stringstream;
using std::unique_ptr;
using std::chrono::seconds;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// returns true on success (i.e. the message gets forwarded)
bool ForwardMessage(int *pUserNumber, int *pSystemNumber) {
  if (*pSystemNumber) {
    return false;
  }

  User userRecord;
  session()->users()->ReadUser(&userRecord, *pUserNumber);
  if (userRecord.IsUserDeleted()) {
    return false;
  }
  if (userRecord.GetForwardUserNumber() == 0 && userRecord.GetForwardSystemNumber() == 0 &&
      userRecord.GetForwardSystemNumber() != 32767) {
    return false;
  }
  if (userRecord.GetForwardSystemNumber() != 0) {
    if (!userRecord.IsMailForwardedToInternet()) {
      int network_number = session()->net_num();
      set_net_num(userRecord.GetForwardNetNumber());
      if (!valid_system(userRecord.GetForwardSystemNumber())) {
        set_net_num(network_number);
        return false;
      }
      if (!userRecord.GetForwardUserNumber()) {
        read_inet_addr(session()->net_email_name, *pUserNumber);
        if (!check_inet_addr(session()->net_email_name)) {
          return false;
        }
      }
      *pUserNumber = userRecord.GetForwardUserNumber();
      *pSystemNumber = userRecord.GetForwardSystemNumber();
      return true;
    } else {
      read_inet_addr(session()->net_email_name, *pUserNumber);
      *pUserNumber = 0;
      *pSystemNumber = 0;
      return false;
    }
  }
  int nCurrentUser = userRecord.GetForwardUserNumber();
  if (nCurrentUser == -1) {
    bout << "Mailbox Closed.\r\n";
    if (so()) {
      bout << "(Forcing)\r\n";
    } else {
      *pUserNumber = 0;
      *pSystemNumber = 0;
    }
    return false;
  }
  std::unique_ptr<bool[]> ss(new bool[syscfg.maxusers + 300]);

  ss[*pUserNumber] = true;
  session()->users()->ReadUser(&userRecord, nCurrentUser);
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
    session()->users()->ReadUser(&userRecord, nCurrentUser);
  }
  *pSystemNumber = 0;
  *pUserNumber = nCurrentUser;
  return true;
}

std::unique_ptr<File> OpenEmailFile(bool bAllowWrite) {
  File *file = new File(session()->config()->datadir(), EMAIL_DAT);

  // If the file doesn't exist, just return the opaque handle now instead of flailing
  // around trying to open it
  if (!file->Exists()) {
    // if it does not exist, try to create it via the open call
    // sf bug 1215434
    file->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    return unique_ptr<File>(file);
  }

  for (int nAttempNum = 0; nAttempNum < NUM_ATTEMPTS_TO_OPEN_EMAIL; nAttempNum++) {
    if (bAllowWrite) {
      file->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    } else {
      file->Open(File::modeBinary | File::modeReadOnly);
    }
    if (file->IsOpen()) {
      break;
    }
    sleep_for(seconds(DELAY_BETWEEN_EMAIL_ATTEMPTS));
  }
  return unique_ptr<File>(file);
}

void sendout_email(EmailData& data) {
  mailrec m, messageRecord;
  net_header_rec nh;
  int i;

  memset(&m, 0, sizeof(mailrec));
  strcpy(m.title, data.title.c_str());
  m.msg = *data.msg;
  m.anony = static_cast<unsigned char>(data.anony);
  if (data.from_system == net_sysnum) {
    m.fromsys = 0;
  } else {
    m.fromsys = static_cast<uint16_t>(data.from_system);
  }
  m.fromuser  = static_cast<uint16_t>(data.from_user);
  m.tosys   = static_cast<uint16_t>(data.system_number);
  m.touser  = static_cast<uint16_t>(data.user_number);
  m.status  = 0;
  m.daten = static_cast<uint32_t>(time(nullptr));

  if (m.fromsys && session()->max_net_num() > 1) {
    m.status |= status_new_net;
    // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
    m.title[71] = '\0';
    m.network.network_msg.net_number = static_cast<int8_t>(data.from_network_number);
  }

  if (data.system_number == 0) {
    unique_ptr<File> pFileEmail(OpenEmailFile(true));
    if (!pFileEmail->IsOpen()) {
      return;
    }
    int nEmailFileLen = static_cast<int>(pFileEmail->GetLength() / sizeof(mailrec));
    if (nEmailFileLen == 0) {
      i = 0;
    } else {
      i = nEmailFileLen - 1;
      pFileEmail->Seek(i * sizeof(mailrec), File::seekBegin);
      pFileEmail->Read(&messageRecord, sizeof(mailrec));
      while (i > 0 && messageRecord.tosys == 0 && messageRecord.touser == 0) {
        --i;
        pFileEmail->Seek(i * sizeof(mailrec), File::seekBegin);
        int i1 = pFileEmail->Read(&messageRecord, sizeof(mailrec));
        if (i1 == -1) {
          bout << "|#6DIDN'T READ WRITE!\r\n";
        }
      }
      if (messageRecord.tosys || messageRecord.touser) {
        ++i;
      }
    }

    pFileEmail->Seek(i * sizeof(mailrec), File::seekBegin);
    int nBytesWritten = pFileEmail->Write(&m, sizeof(mailrec));
    pFileEmail->Close();
    if (nBytesWritten == -1) {
      bout << "|#6DIDN'T SAVE RIGHT!\r\n";
    }
  } else {
    string b;
    if (!readfile(&(m.msg), "email", &b)) {
      return;
    }
    if (data.forwarded_code == 2) {
      remove_link(&(m.msg), "email");
    }
    nh.tosys  = static_cast<uint16_t>(data.system_number);
    nh.touser = static_cast<uint16_t>(data.user_number);
    if (data.from_system > 0) {
      nh.fromsys = static_cast<uint16_t>(data.from_system);
    } else {
      nh.fromsys = net_sysnum;
    }
    nh.fromuser = static_cast<uint16_t>(data.from_user);
    nh.main_type = main_type_email;
    nh.minor_type = 0;
    nh.list_len = 0;
    nh.daten = m.daten;
    nh.method = 0;
    unique_ptr<char[]> b1(new char[b.size() + 768]);
    i = 0;
    if (data.user_number == 0 && data.from_network_number == session()->net_num()) {
      nh.main_type = main_type_email_name;
      strcpy(&(b1[i]), session()->net_email_name.c_str());
      i += session()->net_email_name.size() + 1;
    }
    strcpy(&(b1[i]), m.title);
    i += strlen(m.title) + 1;
    memmove(&(b1[i]), b.c_str(), b.length());
    nh.length = b.length() + i;
    if (nh.length > 32760) {
      bout.bprintf("Message truncated by %lu bytes for the network.", nh.length - 32760L);
      nh.length = 32760;
    }
    if (data.from_network_number != session()->net_num()) {
      gate_msg(&nh, b1.get(), 
        session()->net_num(), session()->net_email_name.c_str(), 
      {}, data.from_network_number);
    } else {
      string net_filename;
      if (data.forwarded_code) {
        net_filename = StringPrintf("%sp1%s",
          session()->network_directory().c_str(),
          session()->network_extension().c_str());
      } else {
        net_filename = StringPrintf("%sp0%s",
          session()->network_directory().c_str(),
          session()->network_extension().c_str());
      }
      File fileNetworkPacket(net_filename);
      fileNetworkPacket.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      fileNetworkPacket.Seek(0L, File::seekEnd);
      fileNetworkPacket.Write(&nh, sizeof(net_header_rec));
      fileNetworkPacket.Write(b1.get(), nh.length);
      fileNetworkPacket.Close();
    }
  }
  string logMessage = "Mail sent to ";
  if (data.system_number == 0) {
    User userRecord;
    session()->users()->ReadUser(&userRecord, data.user_number);
    userRecord.SetNumMailWaiting(userRecord.GetNumMailWaiting() + 1);
    session()->users()->WriteUser(&userRecord, data.user_number);
    if (data.user_number == 1) {
      ++fwaiting;
    }
    if (user_online(data.user_number, &i)) {
      send_inst_sysstr(i, "You just received email.");
    }
    if (data.an) {
      logMessage += session()->names()->UserName(data.user_number);
      sysoplog(logMessage.c_str());
    } else {
      string tempLogMessage = StrCat(logMessage, session()->names()->UserName(data.user_number));
      sysoplog(tempLogMessage);
      logMessage += ">UNKNOWN<";
    }
    if (data.system_number == 0 
        && session()->GetEffectiveSl() > syscfg.newusersl
        && userRecord.GetForwardSystemNumber() == 0
        && !data.silent_mode) {
      bout << "|#5Attach a file to this message? ";
      if (yesno()) {
        attach_file(1);
      }
    }
  } else {
    string logMessagePart;
    if ((data.system_number == 1 && IsEqualsIgnoreCase(session()->network_name(), "Internet")) ||
      data.system_number == 32767) {
      logMessagePart = session()->net_email_name;
    } else {
      if (session()->max_net_num() > 1) {
        if (data.user_number == 0) {
          logMessagePart = StringPrintf("%s @%u.%s", session()->net_email_name.c_str(), data.system_number,
                           session()->network_name());
        } else {
          logMessagePart = StringPrintf("#%u @%u.%s", data.user_number, data.system_number, session()->network_name());
        }
      } else {
        if (data.user_number == 0) {
          logMessagePart = StringPrintf("%s @%u", session()->net_email_name.c_str(), data.system_number);
        } else {
          logMessagePart = StringPrintf("#%u @%u", data.user_number, data.system_number);
        }
      }
    }
    logMessage += logMessagePart;
    sysoplog(logMessage);
  }

  WStatus* pStatus = session()->status_manager()->BeginTransaction();
  if (data.user_number == 1 && data.system_number == 0) {
    pStatus->IncrementNumFeedbackSentToday();
    session()->user()->SetNumFeedbackSent(session()->user()->GetNumFeedbackSent() + 1);
    session()->user()->SetNumFeedbackSentToday(session()->user()->GetNumFeedbackSentToday() + 1);
    ++fsenttoday;
  } else {
    pStatus->IncrementNumEmailSentToday();
    session()->user()->SetNumEmailSentToday(session()->user()->GetNumEmailSentToday() + 1);
    if (data.system_number == 0) {
      session()->user()->SetNumEmailSent(session()->user()->GetNumEmailSent() + 1);
    } else {
      session()->user()->SetNumNetEmailSent(session()->user()->GetNumNetEmailSent() + 1);
    }
  }
  session()->status_manager()->CommitTransaction(pStatus);
  if (!data.silent_mode) {
    bout.Color(3);
    bout << logMessage;
    bout.nl();
  }
}

bool ok_to_mail(int user_number, int system_number, bool bForceit) {
  if (system_number != 0 && net_sysnum == 0) {
    bout << "\r\nSorry, this system is not a part of WWIVnet.\r\n\n";
    return false;
  }
  if (system_number == 0) {
    if (user_number == 0) {
      return false;
    }
    User userRecord;
    session()->users()->ReadUser(&userRecord, user_number);
    if ((userRecord.GetSl() == 255 && userRecord.GetNumMailWaiting() > (static_cast<unsigned>(syscfg.maxwaiting) * 5)) ||
        (userRecord.GetSl() != 255 && userRecord.GetNumMailWaiting() > syscfg.maxwaiting) ||
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
    if (session()->user()->IsRestrictionNet()) {
      bout << "\r\nYou can't send mail off the system.\r\n";
      return false;
    }
  }
  if (!bForceit) {
    if (((user_number == 1 && system_number == 0 &&
          (fsenttoday >= 5 || session()->user()->GetNumFeedbackSentToday() >= 10)) ||
         ((user_number != 1 || system_number != 0) &&
          (session()->user()->GetNumEmailSentToday() >= getslrec(session()->GetEffectiveSl()).emails)))
        && !cs()) {
      bout << "\r\nToo much mail sent today.\r\n\n";
      return false;
    }
    if (session()->user()->IsRestrictionEmail() && user_number != 1) {
      bout << "\r\nYou can't send mail.\r\n\n";
      return false;
    }
  }
  return true;
}

void email(const string& title, int user_number, int system_number, bool forceit, int anony, bool bAllowFSED) {
  int nNumUsers = 0;
  messagerec messageRecord;
  string destination;
  net_system_list_rec *csne = nullptr;
  struct {
    int user_number, system_number, net_num;
    char net_name[20], net_email_name[40];
  } carbon_copy[20];

  bool cc = false, bcc = false;

  if (freek1(syscfg.msgsdir) < 10) {
    bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  bout.nl();
  if (ForwardMessage(&user_number, &system_number)) {
    if (system_number == 32767) {
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
  if (getslrec(session()->GetEffectiveSl()).ability & ability_read_email_anony) {
    an = true;
  } else if (anony & (anony_sender | anony_sender_da | anony_sender_pp)) {
    an = false;
  } else {
    an = true;
  }
  if (system_number == 0) {
    set_net_num(0);
    if (an) {
      destination = session()->names()->UserName(user_number);
    } else {
      destination = ">UNKNOWN<";
    }
  } else {
    if ((system_number == 1 && user_number == 0 &&
         IsEqualsIgnoreCase(session()->network_name(), "Internet")) ||
        system_number == 32767) {
      destination = session()->net_email_name;
    } else {
      if (session()->max_net_num() > 1) {
        if (user_number == 0) {
          destination = StringPrintf("%s @%u.%s", session()->net_email_name.c_str(), system_number, session()->network_name());
        } else {
          destination = StringPrintf("#%u @%u.%s", user_number, system_number, session()->network_name());
        }
      } else {
        if (user_number == 0) {
          destination = StringPrintf("%s @%u", session()->net_email_name.c_str(), system_number);
        } else {
          destination = StringPrintf("#%u @%u", user_number, system_number);
        }
      }
    }
  }
  bout << "|#9E-mailing |#2" << destination;
  bout.nl();
  int i = (getslrec(session()->GetEffectiveSl()).ability & ability_email_anony) ? anony_enable_anony : anony_none;

  if (anony & (anony_receiver_pp | anony_receiver_da)) {
    i = anony_enable_dear_abby;
  }
  if (anony & anony_receiver) {
    i = anony_enable_anony;
  }
  if (i == anony_enable_anony && session()->user()->IsRestrictionAnonymous()) {
    i = 0;
  }
  if (system_number != 0) {
    if (system_number != 32767) {
      i = 0;
      anony = 0;
      bout.nl();
      WWIV_ASSERT(csne);
      bout << "|#9Name of system: |#2" << csne->name << wwiv::endl;
      bout << "|#9Number of hops: |#2" << csne->numhops << wwiv::endl;
      bout.nl();
    }
  }
  write_inst(INST_LOC_EMAIL, (system_number == 0) ? user_number : 0, INST_FLAGS_NONE);

  messageRecord.storage_type = EMAIL_STORAGE;
  MessageEditorData data;
  data.title = title;
  data.need_title = true;
  data.fsed_flags = (bAllowFSED) ? INMSG_FSED : INMSG_NOFSED;
  data.anonymous_flag = i;
  data.aux = "email";
  data.to_name = destination;
  data.msged_flags = MSGED_FLAG_NONE;
  if (!inmsg(data)) {
    return;
  }
  savefile(data.text, &messageRecord, data.aux);
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

  if (session()->IsCarbonCopyEnabled()) {
    bout.nl();
    bout << "|#9Copy this mail to others? ";
    nNumUsers = 0;
    if (yesno()) {
      bool done = false;
      carbon_copy[nNumUsers].user_number = user_number;
      carbon_copy[nNumUsers].system_number = system_number;
      strcpy(carbon_copy[nNumUsers].net_name, session()->network_name());
      strcpy(carbon_copy[nNumUsers].net_email_name, session()->net_email_name.c_str());
      carbon_copy[nNumUsers].net_num = session()->net_num();
      nNumUsers++;
      do {
        bout << "|#9Enter Address (blank to end) : ";
        string emailAddress = input(75);
        if (emailAddress.empty()) {
          done = true;
          break;
        }
        int tu, ts;
        parse_email_info(emailAddress, &tu, &ts);
        if (tu || ts) {
          carbon_copy[nNumUsers].user_number = tu;
          carbon_copy[nNumUsers].system_number = ts;
          strcpy(carbon_copy[nNumUsers].net_name, session()->network_name());
          strcpy(carbon_copy[nNumUsers].net_email_name, session()->net_email_name.c_str());
          carbon_copy[nNumUsers].net_num = session()->net_num();
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
    lineadd(&messageRecord, "\003""7----", "email");
    for (int j = 0; j < nNumUsers; j++) {
      if (carbon_copy[j].system_number == 0) {
        set_net_num(0);
        destination = session()->names()->UserName(carbon_copy[j].user_number);
      } else {
        if (carbon_copy[j].system_number == 1 &&
            carbon_copy[j].user_number == 0 &&
            IsEqualsIgnoreCase(carbon_copy[j].net_name, "Internet")) {
          destination = carbon_copy[j].net_email_name;
        } else {
          set_net_num(carbon_copy[j].net_num);
          if (session()->max_net_num() > 1) {
            if (carbon_copy[j].user_number == 0) {
              destination = StringPrintf("%s@%u.%s", carbon_copy[j].net_email_name, carbon_copy[j].system_number,
                      carbon_copy[j].net_name);
            } else {
              destination = StringPrintf("#%u@%u.%s", carbon_copy[j].user_number, carbon_copy[j].system_number, carbon_copy[j].net_name);
            }
          } else {
            if (carbon_copy[j].user_number == 0) {
              destination = StringPrintf("%s@%u", carbon_copy[j].net_email_name, carbon_copy[j].system_number);
            } else {
              destination = StringPrintf("#%u@%u", carbon_copy[j].user_number, carbon_copy[j].system_number);
            }
          }
        }
      }
      if (j == 0) {
        s1 = StrCat("\003""6Original To: \003""1", destination);
        lineadd(&messageRecord, s1, "email");
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
          lineadd(&messageRecord, s1, "email");
          s1 += "\003""1             ";
          j--;
          listed = 1;
        }
      }
    }
    if (!listed) {
      lineadd(&messageRecord, s1, "email");
    }
  }

  EmailData email(data);
  email.msg = &messageRecord;
  email.anony = i;
  email.an = an;
  email.from_user = session()->usernum;
  email.from_system = net_sysnum;
  email.forwarded_code = 0;

  if (!cc) {
    email.system_number = system_number;
    email.user_number = user_number;
    email.from_network_number = session()->net_num();
    sendout_email(email);
    return;
  }
  for (int nCounter = 0; nCounter < nNumUsers; nCounter++) {
    set_net_num(carbon_copy[nCounter].net_num);
    email.user_number = carbon_copy[nCounter].user_number;
    email.system_number = carbon_copy[nCounter].system_number;
    email.from_network_number = carbon_copy[nCounter].net_num;
    sendout_email(email);
  }
}

void imail(int user_number, int system_number) {
  int fwdu = user_number;
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
    session()->users()->ReadUser(&userRecord, user_number);
    if (!userRecord.IsUserDeleted()) {
      bout << "|#5E-mail " << session()->names()->UserName(user_number) << "? ";
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
  grab_quotes(nullptr, nullptr);
  if (i) {
    email("", user_number, system_number, false, 0);
  }
}

void delmail(File *pFile, int loc) {
  mailrec m, m1;
  User user;

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
    session()->users()->ReadUser(&user, m.touser);
    if (user.GetNumMailWaiting()) {
      user.SetNumMailWaiting(user.GetNumMailWaiting() - 1);
      session()->users()->WriteUser(&user, m.touser);
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
