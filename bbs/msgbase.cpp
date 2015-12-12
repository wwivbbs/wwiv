/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "bbs/attach.h"
#include "bbs/instmsg.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/read_message.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

#define NUM_ATTEMPTS_TO_OPEN_EMAIL 5
#define DELAY_BETWEEN_EMAIL_ATTEMPTS 9

#define GAT_NUMBER_ELEMENTS 2048
#define GAT_SECTION_SIZE    4096
#define MSG_BLOCK_SIZE      512

using std::string;
using std::stringstream;
using std::unique_ptr;
using namespace wwiv::strings;

// returns true on success (i.e. the message gets forwarded)
bool ForwardMessage(int *pUserNumber, int *pSystemNumber) {
  if (*pSystemNumber) {
    return false;
  }

  WUser userRecord;
  application()->users()->ReadUser(&userRecord, *pUserNumber);
  if (userRecord.IsUserDeleted()) {
    return false;
  }
  if (userRecord.GetForwardUserNumber() == 0 && userRecord.GetForwardSystemNumber() == 0 &&
      userRecord.GetForwardSystemNumber() != 32767) {
    return false;
  }
  if (userRecord.GetForwardSystemNumber() != 0) {
    if (!userRecord.IsMailForwardedToInternet()) {
      int nNetworkNumber = session()->GetNetworkNumber();
      set_net_num(userRecord.GetForwardNetNumber());
      if (!valid_system(userRecord.GetForwardSystemNumber())) {
        set_net_num(nNetworkNumber);
        return false;
      }
      if (!userRecord.GetForwardUserNumber()) {
        read_inet_addr(net_email_name, *pUserNumber);
        if (!check_inet_addr(net_email_name)) {
          return false;
        }
      }
      *pUserNumber = userRecord.GetForwardUserNumber();
      *pSystemNumber = userRecord.GetForwardSystemNumber();
      return true;
    } else {
      read_inet_addr(net_email_name, *pUserNumber);
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
  char *ss = static_cast<char*>(BbsAllocA(static_cast<long>(syscfg.maxusers) + 300L));

  ss[*pUserNumber] = 1;
  application()->users()->ReadUser(&userRecord, nCurrentUser);
  while (userRecord.GetForwardUserNumber() || userRecord.GetForwardSystemNumber()) {
    if (userRecord.GetForwardSystemNumber()) {
      if (!valid_system(userRecord.GetForwardSystemNumber())) {
        return false;
      }
      *pUserNumber = userRecord.GetForwardUserNumber();
      *pSystemNumber = userRecord.GetForwardSystemNumber();
      free(ss);
      set_net_num(userRecord.GetForwardNetNumber());
      return true;
    }
    if (ss[nCurrentUser]) {
      free(ss);
      return false;
    }
    ss[nCurrentUser] = 1;
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
      free(ss);
      return false;
    }
    nCurrentUser = userRecord.GetForwardUserNumber() ;
    application()->users()->ReadUser(&userRecord, nCurrentUser);
  }
  free(ss);
  *pSystemNumber = 0;
  *pUserNumber = nCurrentUser;
  return true;
}

std::unique_ptr<File> OpenEmailFile(bool bAllowWrite) {
  File *file = new File(syscfg.datadir, EMAIL_DAT);

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
    Wait(DELAY_BETWEEN_EMAIL_ATTEMPTS);
  }
  return unique_ptr<File>(file);
}

void sendout_email(const string& title, messagerec * pMessageRec, int anony, int nUserNumber, int nSystemNumber,
                   bool an, int nFromUser, int nFromSystem, int nForwardedCode, int nFromNetworkNumber) {
  mailrec m, messageRecord;
  net_header_rec nh;
  int i;

  memset(&m, 0, sizeof(mailrec));
  strcpy(m.title, title.c_str());
  m.msg = *pMessageRec;
  m.anony = static_cast< unsigned char >(anony);
  if (nFromSystem == net_sysnum) {
    m.fromsys = 0;
  } else {
    m.fromsys = static_cast< unsigned short >(nFromSystem);
  }
  m.fromuser  = static_cast< unsigned short >(nFromUser);
  m.tosys   = static_cast< unsigned short >(nSystemNumber);
  m.touser  = static_cast< unsigned short >(nUserNumber);
  m.status  = 0;
  m.daten = static_cast<unsigned long>(time(nullptr));

  if (m.fromsys && session()->GetMaxNetworkNumber() > 1) {
    m.status |= status_new_net;
    // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
    m.title[71] = '\0';
    m.network.network_msg.net_number = static_cast<int8_t>(nFromNetworkNumber);
  }

  if (nSystemNumber == 0) {
    unique_ptr<File> pFileEmail(OpenEmailFile(true));
    if (!pFileEmail->IsOpen()) {
      return;
    }
    int nEmailFileLen = static_cast< int >(pFileEmail->GetLength() / sizeof(mailrec));
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
    if (nForwardedCode == 2) {
      remove_link(&(m.msg), "email");
    }
    nh.tosys  = static_cast< unsigned short >(nSystemNumber);
    nh.touser = static_cast< unsigned short >(nUserNumber);
    if (nFromSystem > 0) {
      nh.fromsys = static_cast< unsigned short >(nFromSystem);
    } else {
      nh.fromsys = net_sysnum;
    }
    nh.fromuser = static_cast< unsigned short >(nFromUser);
    nh.main_type = main_type_email;
    nh.minor_type = 0;
    nh.list_len = 0;
    nh.daten = m.daten;
    nh.method = 0;
    unique_ptr<char[]> b1(new char[b.size() + 768]);
    i = 0;
    if (nUserNumber == 0 && nFromNetworkNumber == session()->GetNetworkNumber()) {
      nh.main_type = main_type_email_name;
      strcpy(&(b1[i]), net_email_name);
      i += strlen(net_email_name) + 1;
    }
    strcpy(&(b1[i]), m.title);
    i += strlen(m.title) + 1;
    memmove(&(b1[i]), b.c_str(), b.length());
    nh.length = b.length() + i;
    if (nh.length > 32760) {
      bout.bprintf("Message truncated by %lu bytes for the network.", nh.length - 32760L);
      nh.length = 32760;
    }
    if (nFromNetworkNumber != session()->GetNetworkNumber()) {
      gate_msg(&nh, b1.get(), session()->GetNetworkNumber(), net_email_name, nullptr, nFromNetworkNumber);
    } else {
      string net_filename;
      if (nForwardedCode) {
        net_filename = StringPrintf("%sp1%s",
          session()->GetNetworkDataDirectory().c_str(),
          application()->GetNetworkExtension().c_str());
      } else {
        net_filename = StringPrintf("%sp0%s",
          session()->GetNetworkDataDirectory().c_str(),
          application()->GetNetworkExtension().c_str());
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
  if (nSystemNumber == 0) {
    WUser userRecord;
    application()->users()->ReadUser(&userRecord, nUserNumber);
    userRecord.SetNumMailWaiting(userRecord.GetNumMailWaiting() + 1);
    application()->users()->WriteUser(&userRecord, nUserNumber);
    if (nUserNumber == 1) {
      ++fwaiting;
    }
    if (user_online(nUserNumber, &i)) {
      send_inst_sysstr(i, "You just received email.");
    }
    if (an) {
      logMessage += userRecord.GetUserNameAndNumber(nUserNumber);
      sysoplog(logMessage.c_str());
    } else {
      string tempLogMessage = logMessage;
      tempLogMessage += userRecord.GetUserNameAndNumber(nUserNumber);
      sysoplog(tempLogMessage);
      logMessage += ">UNKNOWN<";
    }
    if (nSystemNumber == 0 && session()->GetEffectiveSl() > syscfg.newusersl && userRecord.GetForwardSystemNumber() == 0
        && !session()->IsNewMailWatiting()) {
      bout << "|#5Attach a file to this message? ";
      if (yesno()) {
        attach_file(1);
      }
    }
  } else {
    string logMessagePart;
    if ((nSystemNumber == 1 && IsEqualsIgnoreCase(session()->GetNetworkName(), "Internet")) ||
        nSystemNumber == 32767) {
      logMessagePart = net_email_name;
    } else {
      if (session()->GetMaxNetworkNumber() > 1) {
        if (nUserNumber == 0) {
          logMessagePart = StringPrintf("%s @%u.%s", net_email_name, nSystemNumber,
                           session()->GetNetworkName());
        } else {
          logMessagePart = StringPrintf("#%u @%u.%s", nUserNumber, nSystemNumber, session()->GetNetworkName());
        }
      } else {
        if (nUserNumber == 0) {
          logMessagePart = StringPrintf("%s @%u", net_email_name, nSystemNumber);
        } else {
          logMessagePart = StringPrintf("#%u @%u", nUserNumber, nSystemNumber);
        }
      }
    }
    logMessage += logMessagePart;
    sysoplog(logMessage);
  }

  WStatus* pStatus = application()->GetStatusManager()->BeginTransaction();
  if (nUserNumber == 1 && nSystemNumber == 0) {
    pStatus->IncrementNumFeedbackSentToday();
    session()->user()->SetNumFeedbackSent(session()->user()->GetNumFeedbackSent() + 1);
    session()->user()->SetNumFeedbackSentToday(session()->user()->GetNumFeedbackSentToday() + 1);
    ++fsenttoday;
  } else {
    pStatus->IncrementNumEmailSentToday();
    session()->user()->SetNumEmailSentToday(session()->user()->GetNumEmailSentToday() + 1);
    if (nSystemNumber == 0) {
      session()->user()->SetNumEmailSent(session()->user()->GetNumEmailSent() + 1);
    } else {
      session()->user()->SetNumNetEmailSent(session()->user()->GetNumNetEmailSent() + 1);
    }
  }
  application()->GetStatusManager()->CommitTransaction(pStatus);
  if (!session()->IsNewMailWatiting()) {
    bout.Color(3);
    bout << logMessage;
    bout.nl();
  }
}

bool ok_to_mail(int nUserNumber, int nSystemNumber, bool bForceit) {
  if (nSystemNumber != 0 && net_sysnum == 0) {
    bout << "\r\nSorry, this system is not a part of WWIVnet.\r\n\n";
    return false;
  }
  if (nSystemNumber == 0) {
    if (nUserNumber == 0) {
      return false;
    }
    WUser userRecord;
    application()->users()->ReadUser(&userRecord, nUserNumber);
    if ((userRecord.GetSl() == 255 && userRecord.GetNumMailWaiting() > (syscfg.maxwaiting * 5)) ||
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
    if (!valid_system(nSystemNumber)) {
      bout << "\r\nUnknown system number.\r\n\n";
      return false;
    }
    if (session()->user()->IsRestrictionNet()) {
      bout << "\r\nYou can't send mail off the system.\r\n";
      return false;
    }
  }
  if (!session()->IsNewMailWatiting() && !bForceit) {
    if (((nUserNumber == 1 && nSystemNumber == 0 &&
          (fsenttoday >= 5 || session()->user()->GetNumFeedbackSentToday() >= 10)) ||
         ((nUserNumber != 1 || nSystemNumber != 0) &&
          (session()->user()->GetNumEmailSentToday() >= getslrec(session()->GetEffectiveSl()).emails)))
        && !cs()) {
      bout << "\r\nToo much mail sent today.\r\n\n";
      return false;
    }
    if (session()->user()->IsRestrictionEmail() && nUserNumber != 1) {
      bout << "\r\nYou can't send mail.\r\n\n";
      return false;
    }
  }
  return true;
}

void email(const string& title, int nUserNumber, int nSystemNumber, bool forceit, int anony, bool bAllowFSED) {
  int nNumUsers = 0;
  messagerec messageRecord;
  char szDestination[81];
  WUser userRecord;
  net_system_list_rec *csne = nullptr;
  struct {
    int nUserNumber, nSystemNumber, net_num;
    char net_name[20], net_email_name[40];
  } carbon_copy[20];

  bool cc = false, bcc = false;

  if (freek1(syscfg.msgsdir) < 10) {
    bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  bout.nl();
  if (ForwardMessage(&nUserNumber, &nSystemNumber)) {
    if (nSystemNumber == 32767) {
      read_inet_addr(szDestination, nUserNumber);
    }
    bout << "\r\nMail Forwarded.\r\n\n";
    if (nUserNumber == 0 && nSystemNumber == 0) {
      bout << "Forwarded to unknown user.\r\n";
      return;
    }
  }
  if (!nUserNumber && !nSystemNumber) {
    return;
  }
  if (!ok_to_mail(nUserNumber, nSystemNumber, forceit)) {
    return;
  }
  if (nSystemNumber) {
    csne = next_system(nSystemNumber);
  }
  bool an = true;
  if (getslrec(session()->GetEffectiveSl()).ability & ability_read_email_anony) {
    an = true;
  } else if (anony & (anony_sender | anony_sender_da | anony_sender_pp)) {
    an = false;
  } else {
    an = true;
  }
  if (nSystemNumber == 0) {
    set_net_num(0);
    if (an) {
      application()->users()->ReadUser(&userRecord, nUserNumber);
      strcpy(szDestination, userRecord.GetUserNameAndNumber(nUserNumber));
    } else {
      strcpy(szDestination, ">UNKNOWN<");
    }
  } else {
    if ((nSystemNumber == 1 && nUserNumber == 0 &&
         IsEqualsIgnoreCase(session()->GetNetworkName(), "Internet")) ||
        nSystemNumber == 32767) {
      strcpy(szDestination, net_email_name);
    } else {
      if (session()->GetMaxNetworkNumber() > 1) {
        if (nUserNumber == 0) {
          sprintf(szDestination, "%s @%u.%s", net_email_name, nSystemNumber, session()->GetNetworkName());
        } else {
          sprintf(szDestination, "#%u @%u.%s", nUserNumber, nSystemNumber, session()->GetNetworkName());
        }
      } else {
        if (nUserNumber == 0) {
          sprintf(szDestination, "%s @%u", net_email_name, nSystemNumber);
        } else {
          sprintf(szDestination, "#%u @%u", nUserNumber, nSystemNumber);
        }
      }
    }
  }
  if (!session()->IsNewMailWatiting()) {
    bout << "|#9E-mailing |#2";
  }
  bout << szDestination;
  bout.nl();
  int i = (getslrec(session()->GetEffectiveSl()).ability & ability_email_anony) ? anony_enable_anony : 0;

  if (anony & (anony_receiver_pp | anony_receiver_da)) {
    i = anony_enable_dear_abby;
  }
  if (anony & anony_receiver) {
    i = anony_enable_anony;
  }
  if (i == anony_enable_anony && session()->user()->IsRestrictionAnonymous()) {
    i = 0;
  }
  if (nSystemNumber != 0) {
    if (nSystemNumber != 32767) {
      i = 0;
      anony = 0;
      bout.nl();
      WWIV_ASSERT(csne);
      bout << "|#9Name of system: |#2" << csne->name << wwiv::endl;
      bout << "|#9Number of hops: |#2" << csne->numhops << wwiv::endl;
      bout.nl();
    }
  }
  write_inst(INST_LOC_EMAIL, (nSystemNumber == 0) ? nUserNumber : 0, INST_FLAGS_NONE);

  messageRecord.storage_type = EMAIL_STORAGE;
  MessageEditorData data;
  data.title = title;
  data.fsed_flags = (bAllowFSED) ? INMSG_FSED : INMSG_NOFSED;
  data.anonymous_flag = i;
  data.aux = "email";
  data.to_name = szDestination;
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
    if (!session()->IsNewMailWatiting()) {
      bout.nl();
      bout << "|#9Copy this mail to others? ";
      nNumUsers = 0;
      if (yesno()) {
        bool done = false;
        carbon_copy[nNumUsers].nUserNumber = nUserNumber;
        carbon_copy[nNumUsers].nSystemNumber = nSystemNumber;
        strcpy(carbon_copy[nNumUsers].net_name, session()->GetNetworkName());
        strcpy(carbon_copy[nNumUsers].net_email_name, net_email_name);
        carbon_copy[nNumUsers].net_num = session()->GetNetworkNumber();
        nNumUsers++;
        do {
          string emailAddress;
          bout << "|#9Enter Address (blank to end) : ";
          input(&emailAddress, 75);
          if (emailAddress.empty()) {
            done = true;
            break;
          }
          int tu, ts;
          parse_email_info(emailAddress, &tu, &ts);
          if (tu || ts) {
            carbon_copy[nNumUsers].nUserNumber = tu;
            carbon_copy[nNumUsers].nSystemNumber = ts;
            strcpy(carbon_copy[nNumUsers].net_name, session()->GetNetworkName());
            strcpy(carbon_copy[nNumUsers].net_email_name, net_email_name);
            carbon_copy[nNumUsers].net_num = session()->GetNetworkNumber();
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
        if (carbon_copy[j].nSystemNumber == 0) {
          set_net_num(0);
          application()->users()->ReadUser(&userRecord, carbon_copy[j].nUserNumber);
          strcpy(szDestination, userRecord.GetUserNameAndNumber(carbon_copy[j].nUserNumber));
        } else {
          if (carbon_copy[j].nSystemNumber == 1 &&
              carbon_copy[j].nUserNumber == 0 &&
              IsEqualsIgnoreCase(carbon_copy[j].net_name, "Internet")) {
            strcpy(szDestination, carbon_copy[j].net_email_name);
          } else {
            set_net_num(carbon_copy[j].net_num);
            if (session()->GetMaxNetworkNumber() > 1) {
              if (carbon_copy[j].nUserNumber == 0) {
                sprintf(szDestination, "%s@%u.%s", carbon_copy[j].net_email_name, carbon_copy[j].nSystemNumber,
                        carbon_copy[j].net_name);
              } else {
                sprintf(szDestination, "#%u@%u.%s", carbon_copy[j].nUserNumber, carbon_copy[j].nSystemNumber, carbon_copy[j].net_name);
              }
            } else {
              if (carbon_copy[j].nUserNumber == 0) {
                sprintf(szDestination, "%s@%u", carbon_copy[j].net_email_name, carbon_copy[j].nSystemNumber);
              } else {
                sprintf(szDestination, "#%u@%u", carbon_copy[j].nUserNumber, carbon_copy[j].nSystemNumber);
              }
            }
          }
        }
        if (j == 0) {
          s1 = StringPrintf("\003""6Original To: \003""1%s", szDestination);
          lineadd(&messageRecord, s1, "email");
          s1 = "\003""6Carbon Copy: \003""1";
        } else {
          if (s1.length() + strlen(szDestination) < 77) {
            s1 += szDestination;
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
  }
  if (cc) {
    for (int nCounter = 0; nCounter < nNumUsers; nCounter++) {
      set_net_num(carbon_copy[nCounter].net_num);
      sendout_email(data.title, &messageRecord, i, carbon_copy[nCounter].nUserNumber, carbon_copy[nCounter].nSystemNumber, an,
                    session()->usernum, net_sysnum, false, carbon_copy[nCounter].net_num);
    }
  } else {
    sendout_email(data.title, &messageRecord, i, nUserNumber, nSystemNumber, an, session()->usernum, net_sysnum, false,
                  session()->GetNetworkNumber());
  }
}

void imail(int nUserNumber, int nSystemNumber) {
  char szInternetAddr[255];
  WUser userRecord;

  int fwdu = nUserNumber;
  bool fwdm = false;

  if (ForwardMessage(&nUserNumber, &nSystemNumber)) {
    bout << "Mail forwarded.\r\n";
    fwdm = true;
  }

  if (!nUserNumber && !nSystemNumber) {
    return;
  }

  if (fwdm) {
    read_inet_addr(szInternetAddr, fwdu);
  }

  int i = 1;
  if (nSystemNumber == 0) {
    application()->users()->ReadUser(&userRecord, nUserNumber);
    if (!userRecord.IsUserDeleted()) {
      bout << "|#5E-mail " << userRecord.GetUserNameAndNumber(nUserNumber) << "? ";
      if (!yesno()) {
        i = 0;
      }
    } else {
      i = 0;
    }
  } else {
    if (fwdm) {
      bout << "|#5E-mail " << szInternetAddr << "? ";
    } else {
      bout << "|#5E-mail User " << nUserNumber << " @" << nSystemNumber << " ? ";
    }
    if (!yesno()) {
      i = 0;
    }
  }
  grab_quotes(nullptr, nullptr);
  if (i) {
    email("", nUserNumber, nSystemNumber, false, 0);
  }
}

void LoadFileIntoWorkspace(const std::string& filename, bool bNoEditAllowed) {
  File fileOrig(filename);
  if (!fileOrig.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "\r\nFile not found.\r\n\n";
    return;
  }

  long lOrigSize = fileOrig.GetLength();
  unique_ptr<char[]> b(new char[lOrigSize + 1024]);
  fileOrig.Read(b.get(), lOrigSize);
  fileOrig.Close();
  if (b[lOrigSize - 1] != CZ) {
    b[lOrigSize++] = CZ;
  }

  File fileOut(syscfgovr.tempdir, INPUT_MSG);
  fileOut.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  fileOut.Write(b.get(), lOrigSize);
  fileOut.Close();

  use_workspace = (bNoEditAllowed || !okfsed()) ? true : false;

  if (!session()->IsNewMailWatiting()) {
    bout << "\r\nFile loaded into workspace.\r\n\n";
    if (!use_workspace) {
      bout << "Editing will be allowed.\r\n";
    }
  }
}
