/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "bbs/wwiv.h"
#include "core/strings.h"
#include "core/wwivassert.h"

#define EMAIL_STORAGE 2

#define NUM_ATTEMPTS_TO_OPEN_EMAIL 5
#define DELAY_BETWEEN_EMAIL_ATTEMPTS 9

#define GAT_NUMBER_ELEMENTS 2048
#define GAT_SECTION_SIZE    4096
#define MSG_BLOCK_SIZE      512

using std::string;
using std::stringstream;
using std::unique_ptr;
using namespace wwiv::strings;

// Local function prototypes
File* OpenMessageFile(const string messageAreaFileName);
void set_gat_section(File *pMessageFile, int section);
void save_gat(File *pMessageFile);

static long gat_section;

/**
 * Sets the global variables pszOutOriginStr and pszOutOriginStr2.
 * Note: This is a private function
 */
static void SetMessageOriginInfo(int nSystemNumber, int nUserNumber, string* outNetworkName,
                                 string* outLocation) {
  string netName;

  if (session()->GetMaxNetworkNumber() > 1) {
    netName = StrCat(net_networks[session()->GetNetworkNumber()].name, "- ");
  }

  outNetworkName->clear();
  outLocation->clear();

  if (IsEqualsIgnoreCase(session()->GetNetworkName(), "Internet") ||
      nSystemNumber == 32767) {
    outNetworkName->assign("Internet Mail and Newsgroups");
    return;
  }

  if (nSystemNumber && session()->GetCurrentNetworkType() == net_type_wwivnet) {
    net_system_list_rec *csne = next_system(nSystemNumber);
    if (csne) {
      string netstatus;
      if (nUserNumber == 1) {
        if (csne->other & other_net_coord) {
          netstatus = "{NC}";
        } else if (csne->other & other_group_coord) {
          netstatus = StringPrintf("{GC%d}", csne->group);
        } else if (csne->other & other_area_coord) {
          netstatus = "{AC}";
        }
      }
      const string filename = StringPrintf(
              "%s%s%c%s.%-3u",
              syscfg.datadir,
              REGIONS_DIR,
              File::pathSeparatorChar,
              REGIONS_DIR,
              atoi(csne->phone));

      string description;
      if (File::Exists(filename)) {
        // Try to use the town first.
        const string phone_prefix = StringPrintf("%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
        description = describe_area_code_prefix(atoi(csne->phone), atoi(phone_prefix.c_str()));
      }
      if (description.empty()) {
        // Try area code if we still don't have a description.
        description = describe_area_code(atoi(csne->phone));
      }

      *outNetworkName = StrCat(netName, csne->name, " [", csne->phone, "] ", netstatus.c_str());
      *outLocation = (!description.empty()) ? description : "Unknown Area";
    } else {
      *outNetworkName = StrCat(netName, "Unknown System");
      *outLocation = "Unknown Area";
    }
  }
}

/**
 * Deletes a message
 * This is a public function.
 */
void remove_link(messagerec * pMessageRecord, string fileName) {
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    unique_ptr<File> file(OpenMessageFile(fileName));
    if (file->IsOpen()) {
      set_gat_section(file.get(), static_cast<int>(pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS));
      long lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
      while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
        long lNextSection = static_cast<long>(gat[lCurrentSection]);
        gat[lCurrentSection] = 0;
        lCurrentSection = lNextSection;
      }
      save_gat(file.get());
      file->Close();
    }
  }
  break;
  default:
    // illegal storage type
    break;
  }
}

/**
 * Opens the message area file {pszMessageAreaFileName} and returns the file handle.
 * Note: This is a Private method to this module.
 */
File* OpenMessageFile(const string messageAreaFileName) {
  application()->GetStatusManager()->RefreshStatusCache();

  const string filename = StrCat(syscfg.msgsdir, messageAreaFileName, FILENAME_DAT_EXTENSION);
  File *pFileMessage = new File(filename);
  if (!pFileMessage->Open(File::modeReadWrite | File::modeBinary)) {
    // Create message area file if it doesn't exist.
    pFileMessage->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    for (int i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
      gat[i] = 0;
    }
    pFileMessage->Write(gat, GAT_SECTION_SIZE);
    pFileMessage->SetLength(GAT_SECTION_SIZE + (75L * 1024L));
    gat_section = 0;
  }
  pFileMessage->Seek(0L, File::seekBegin);
  pFileMessage->Read(gat, GAT_SECTION_SIZE);

  gat_section = 0;
  return pFileMessage;
}

#define GATSECLEN (GAT_SECTION_SIZE + GAT_NUMBER_ELEMENTS * MSG_BLOCK_SIZE)
#ifndef MSG_STARTING
#define MSG_STARTING (gat_section * GATSECLEN + GAT_SECTION_SIZE)
#endif  // MSG_STARTING

long current_gat_section() { return gat_section; }
void current_gat_section(long section) { gat_section = section; }

void set_gat_section(File *pMessageFile, int section) {
  if (gat_section != section) {
    long lFileSize = pMessageFile->GetLength();
    long lSectionPos = static_cast<long>(section) * GATSECLEN;
    if (lFileSize < lSectionPos) {
      pMessageFile->SetLength(lSectionPos);
      lFileSize = lSectionPos;
    }
    pMessageFile->Seek(lSectionPos, File::seekBegin);
    if (lFileSize < (lSectionPos + GAT_SECTION_SIZE)) {
      for (int i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
        gat[i] = 0;
      }
      pMessageFile->Write(gat, GAT_SECTION_SIZE);
    } else {
      pMessageFile->Read(gat, GAT_SECTION_SIZE);
    }
    gat_section = section;
  }
}

void save_gat(File *pMessageFile) {
  long lSectionPos = static_cast<long>(gat_section) * GATSECLEN;
  pMessageFile->Seek(lSectionPos, File::seekBegin);
  pMessageFile->Write(gat, GAT_SECTION_SIZE);
  WStatus *pStatus = application()->GetStatusManager()->BeginTransaction();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangePosts);
  application()->GetStatusManager()->CommitTransaction(pStatus);
}

void savefile(char *b, long lMessageLength, messagerec * pMessageRecord, const string fileName) {
  WWIV_ASSERT(pMessageRecord);
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    int gati[128];
    unique_ptr<File> pMessageFile(OpenMessageFile(fileName));
    if (pMessageFile->IsOpen()) {
      for (int section = 0; section < 1024; section++) {
        set_gat_section(pMessageFile.get(), section);
        int gatp = 0;
        int nNumBlocksRequired = static_cast<int>((lMessageLength + 511L) / MSG_BLOCK_SIZE);
        int i4 = 1;
        while (gatp < nNumBlocksRequired && i4 < GAT_NUMBER_ELEMENTS) {
          if (gat[i4] == 0) {
            gati[gatp++] = i4;
          }
          ++i4;
        }
        if (gatp >= nNumBlocksRequired) {
          gati[gatp] = -1;
          for (int i = 0; i < nNumBlocksRequired; i++) {
            pMessageFile->Seek(MSG_STARTING + MSG_BLOCK_SIZE * static_cast<long>(gati[i]), File::seekBegin);
            pMessageFile->Write((&b[i * MSG_BLOCK_SIZE]), MSG_BLOCK_SIZE);
            gat[gati[i]] = static_cast< unsigned short >(gati[i + 1]);
          }
          save_gat(pMessageFile.get());
          break;
        }
      }
      pMessageFile->Close();
    }
    pMessageRecord->stored_as = static_cast<long>(gati[0]) + static_cast<long>(gat_section) * GAT_NUMBER_ELEMENTS;
  }
  break;
  default: {
    bout.bprintf("WWIV:ERROR:msgbase.cpp: Save - storage_type=%u!\r\n", pMessageRecord->storage_type);
    WWIV_ASSERT(false);
  }
  break;
  }
}

char *readfile(messagerec * pMessageRecord, string fileName, long *plMessageLength) {
  *plMessageLength = 0L;
  if (pMessageRecord->storage_type == 2) {
    unique_ptr<File> file(OpenMessageFile(fileName));
    set_gat_section(file.get(), pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS);
    int lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
    long lMessageLength = 0;
    while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
      lMessageLength += MSG_BLOCK_SIZE;
      lCurrentSection = gat[lCurrentSection];
    }
    if (lMessageLength == 0) {
      bout << "\r\nNo message found.\r\n\n";
      return nullptr;
    }
    char* b = new char[lMessageLength + 512];
    if (!b) {
      return nullptr;
    }
    lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
    long lMessageBytesRead = 0;
    while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
      file->Seek(MSG_STARTING + MSG_BLOCK_SIZE * static_cast< long >(lCurrentSection), File::seekBegin);
      lMessageBytesRead += static_cast<long>(file->Read(&(b[lMessageBytesRead]), MSG_BLOCK_SIZE));
      lCurrentSection = gat[lCurrentSection];
    }
    file->Close();
    long lRealMessageLength = lMessageBytesRead - MSG_BLOCK_SIZE;
    while ((lRealMessageLength < lMessageBytesRead) && (b[lRealMessageLength] != CZ)) {
      ++lRealMessageLength;
    }
    *plMessageLength = lRealMessageLength;
    b[lRealMessageLength + 1] = '\0';
    return b;
  }
  return nullptr;
}

void LoadFileIntoWorkspace(const char *pszFileName, bool bNoEditAllowed) {
  File fileOrig(pszFileName);
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
  if (ss == nullptr) {
    return false;
  }
  for (int i = 0; i < syscfg.maxusers + 300; i++) {
    ss[i] = '\0';
  }
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
                   int an, int nFromUser, int nFromSystem, int nForwardedCode, int nFromNetworkNumber) {
  mailrec m, messageRecord;
  net_header_rec nh;
  int i;

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
    m.title[79] = '\0';
    m.title[80] = static_cast<char>(nFromNetworkNumber);
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
    long lEmailFileLen;
    unique_ptr<char[]> b(readfile(&(m.msg), "email", &lEmailFileLen));
    if (!b) {
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
    unique_ptr<char[]> b1(new char[lEmailFileLen + 768]);
    i = 0;
    if (nUserNumber == 0 && nFromNetworkNumber == session()->GetNetworkNumber()) {
      nh.main_type = main_type_email_name;
      strcpy(&(b1[i]), net_email_name);
      i += strlen(net_email_name) + 1;
    }
    strcpy(&(b1[i]), m.title);
    i += strlen(m.title) + 1;
    memmove(&(b1[i]), b.get(), lEmailFileLen);
    nh.length = lEmailFileLen + i;
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

void email(int nUserNumber, int nSystemNumber, bool forceit, int anony, bool force_title, bool bAllowFSED) {
  int an;
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
  if (getslrec(session()->GetEffectiveSl()).ability & ability_read_email_anony) {
    an = 1;
  } else if (anony & (anony_sender | anony_sender_da | anony_sender_pp)) {
    an = 0;
  } else {
    an = 1;
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
  int nUseFSED = (bAllowFSED) ? INMSG_FSED : INMSG_NOFSED;
  string title;
  inmsg(&messageRecord, &title, &i, !forceit, "email", nUseFSED, szDestination, MSGED_FLAG_NONE, force_title);
  if (messageRecord.stored_as == 0xffffffff) {
    return;
  }
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
      sendout_email(title, &messageRecord, i, carbon_copy[nCounter].nUserNumber, carbon_copy[nCounter].nSystemNumber, an,
                    session()->usernum, net_sysnum, 0, carbon_copy[nCounter].net_num);
    }
  } else {
    sendout_email(title, &messageRecord, i, nUserNumber, nSystemNumber, an, session()->usernum, net_sysnum, 0,
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
    email(nUserNumber, nSystemNumber, false, 0);
  }
}

void read_message1(messagerec * pMessageRecord, char an, bool readit, bool *next, const char *pszFileName,
                   int nFromSystem, int nFromUser) {
  string strName, strDate;
  char s[205];
  long lMessageTextLength;

  string origin_str;
  string origin_str2;

  // Moved internally from outside this method
  SetMessageOriginInfo(nFromSystem, nFromUser, &origin_str, &origin_str2);

  g_flags &= ~g_flag_ansi_movement;

  unique_ptr<char[]> ss;
  bool ansi = false;
  *next = false;
  long lCurrentCharPointer = 0;
  bool abort = false;
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
  case 2: {
    ss.reset(readfile(pMessageRecord, pszFileName, &lMessageTextLength));
    if (ss == nullptr) {
      plan(6, "File not found.", &abort, next);
      bout.nl();
      return;
    }
    int nNamePtr = 0;
    while ((ss[nNamePtr] != RETURN) &&
           (nNamePtr < lMessageTextLength) &&
           (nNamePtr < 200)) {
      strName.push_back(ss[nNamePtr++]);
    }
    ++nNamePtr;
    int nDatePtr = 0;
    if (ss[nNamePtr] == SOFTRETURN) {
      ++nNamePtr;
    }
    while ((ss[nNamePtr + nDatePtr] != RETURN) &&
           static_cast<long>(nNamePtr + nDatePtr < lMessageTextLength) &&
           (nDatePtr < 60)) {
      strDate.push_back(ss[(nDatePtr++) + nNamePtr]);
    }

    lCurrentCharPointer = nNamePtr + nDatePtr + 1;
  }
  break;
  default:
    // illegal storage type
    bout.nl();
    bout << "->ILLEGAL STORAGE TYPE<-\r\n\n";
    return;
  }

  irt_name[0] = '\0';
  g_flags |= g_flag_disable_mci;
  string name;
  string date = strDate;
  string from;
  string loc;
  switch (an) {
  default:
  case 0:
    if (syscfg.sysconfig & sysconfig_enable_mci) {
      g_flags &= ~g_flag_disable_mci;
    }
    strcpy(irt_name, strName.c_str());
    name = strName;
    from = origin_str;
    loc = origin_str2;
    break;
  case anony_sender:
    if (readit) {
      name = StrCat("<<< ", strName, " >>>");
    } else {
      name = ">UNKNOWN<";
    }
    break;
  case anony_sender_da:
  case anony_sender_pp:
    date = ">UNKNOWN<";
    if (an == anony_sender_da) {
      name = "Abby";
    } else {
      name = "Problemed Person";
    }
    if (readit) {
      name = StrCat("<<< ", strName, " >>>");
    }
    break;
  }

  bout << "|#9Name|#7: |#1" << name << wwiv::endl;
  bout << "|#9Date|#7: |#1" << date << wwiv::endl;
  if (!from.empty()) {
    bout << "|#9From|#7: |#1" << from << wwiv::endl;
  }
  if (!loc.empty()) {
    bout << "|#9Loc|#7:  |#1" << loc << wwiv::endl;
  }

  int nNumCharsPtr  = 0;
  int nLineLenPtr   = 0;
  int ctrld     = 0;
  char ch       = 0;
  bool done     = false;
  bool printit    = false;
  bool ctrla      = false;
  bool centre     = false;

  checka(&abort, next);
  bout.nl();
  while (!done && !abort && !hangup) {
    switch (pMessageRecord->storage_type) {
    case 0:
    case 1:
    case 2:
      ch = ss[lCurrentCharPointer];
      if (lCurrentCharPointer >= lMessageTextLength) {
        ch = CZ;
      }
      break;
    }
    if (ch == CZ) {
      done = true;
    } else {
      if (ch != SOFTRETURN) {
        if (ch == RETURN || !ch) {
          printit = true;
        } else if (ch == CA) {
          ctrla = true;
        } else if (ch == CB) {
          centre = true;
        } else if (ch == CD) {
          ctrld = 1;
        } else if (ctrld == 1) {
          if (ch >= '0' && ch <= '9') {
            if (session()->user()->GetOptionalVal() < (ch - '0')) {
              ctrld = 0;
            } else {
              ctrld = -1;
            }
          } else {
            ctrld = 0;
          }
        } else {
          if (ch == ESC) {
            ansi = true;
          }
          if (g_flags & g_flag_ansi_movement) {
            g_flags &= ~g_flag_ansi_movement;
            lines_listed = 0;
            if (session()->localIO()->GetTopLine() && session()->localIO()->GetScreenBottom() == 24) {
              session()->localIO()->set_protect(0);
            }
          }
          s[nNumCharsPtr++] = ch;
          if (ch == CC || ch == BACKSPACE) {
            --nLineLenPtr;
          } else {
            ++nLineLenPtr;
          }
        }

        if (printit || ansi || nLineLenPtr >= 80) {
          if (centre && (ctrld != -1)) {
            int nSpacesToCenter = (session()->user()->GetScreenChars() - session()->localIO()->WhereX() -
                                   nLineLenPtr) / 2;
            osan(charstr(nSpacesToCenter, ' '), &abort, next);
          }
          if (nNumCharsPtr) {
            if (ctrld != -1) {
              if ((session()->localIO()->WhereX() + nLineLenPtr >= session()->user()->GetScreenChars()) && !centre
                  && !ansi) {
                bout.nl();
              }
              s[nNumCharsPtr] = '\0';
              osan(s, &abort, next);
              if (ctrla && s[nNumCharsPtr - 1] != SPACE && !ansi) {
                if (session()->localIO()->WhereX() < session()->user()->GetScreenChars() - 1) {
                  bputch(SPACE);
                } else {
                  bout.nl();
                }
                checka(&abort, next);
              }
            }
            nLineLenPtr  = 0;
            nNumCharsPtr = 0;
          }
          centre = false;
        }
        if (ch == RETURN) {
          if (ctrla == false) {
            if (ctrld != -1) {
              bout.nl();
              checka(&abort, next);
            }
          } else {
            ctrla = false;
          }
          if (printit) {
            ctrld = 0;
          }
        }
        printit = false;
      } else {
        ctrld = 0;
      }
    }
    ++lCurrentCharPointer;
  }
  if (!abort && nNumCharsPtr) {
    s[nNumCharsPtr] = '\0';
    bout << s;
    bout.nl();
  }
  bout.Color(0);
  bout.nl();
  if (express && abort && !*next) {
    expressabort = true;
  }
  if (ansi && session()->topdata && session()->IsUserOnline()) {
    application()->UpdateTopScreen();
  }
  if (syscfg.sysconfig & sysconfig_enable_mci) {
    g_flags &= ~g_flag_disable_mci;
  }
}

void read_message(int n, bool *next, int *val) {
  bout.nl();
  bool abort = false;
  *next = false;
  if (session()->user()->IsUseClearScreen()) {
    bout.cls();
  }
  if (forcescansub) {
    bout.cls();
    bout.GotoXY(1, 1);
    bout << "|#4   FORCED SCAN OF SYSOP INFORMATION - YOU MAY NOT ABORT.  PLEASE READ THESE!  |#0\r\n";
  }

  bout.bprintf(" |#9Msg|#7: [|#1%u|#7/|#1%lu|#7]|#%d |#2%s\r\n", n,
                                    session()->GetNumMessagesInCurrentMessageArea(), session()->GetMessageColor(),
                                    subboards[session()->GetCurrentReadMessageArea()].name);
  const string subjectLine = "|#9Subj|#7: ";
  osan(subjectLine, &abort, next);
  bout.Color(session()->GetMessageColor());
  postrec p = *get_post(n);
  if (p.status & (status_unvalidated | status_delete)) {
    plan(6, "<<< NOT VALIDATED YET >>>", &abort, next);
    if (!lcs()) {
      return;
    }
    *val |= 1;
    osan(subjectLine, &abort, next);
    bout.Color(session()->GetMessageColor());
  }
  strncpy(irt, p.title, 60);
  irt_name[0] = '\0';
  plan(session()->GetMessageColor(), irt, &abort, next);
  if ((p.status & status_no_delete) && lcs()) {
    osan("|#9Info|#7: ", &abort, next);
    plan(session()->GetMessageColor(), "Permanent Message", &abort, next);
  }
  if ((p.status & status_pending_net) && session()->user()->GetSl() > syscfg.newusersl) {
    osan("|#9Val|#7:  ", &abort, next);
    plan(session()->GetMessageColor(), "Not Network Validated", &abort, next);
    *val |= 2;
  }
  if (!abort) {
    bool bReadit = (lcs() || (getslrec(session()->GetEffectiveSl()).ability & ability_read_post_anony)) ? true : false;
    int nNetNumSaved = session()->GetNetworkNumber();

    if (p.status & status_post_new_net) {
      set_net_num(p.title[80]);
    }
    read_message1(&(p.msg), static_cast<char>(p.anony & 0x0f), bReadit, next,
                  (subboards[session()->GetCurrentReadMessageArea()].filename), p.ownersys, p.owneruser);

    if (nNetNumSaved != session()->GetNetworkNumber()) {
      set_net_num(nNetNumSaved);
    }

    session()->user()->SetNumMessagesRead(session()->user()->GetNumMessagesRead() + 1);
    session()->SetNumMessagesReadThisLogon(session()->GetNumMessagesReadThisLogon() + 1);
  } else if (express && !*next) {
    expressabort = true;
  }
  if (p.qscan > qsc_p[session()->GetCurrentReadMessageArea()]) {
    qsc_p[session()->GetCurrentReadMessageArea()] = p.qscan;
  }
  
  WStatus *pStatus = application()->GetStatusManager()->GetStatus();
  // not sure why we check this twice...
  // maybe we need a getCachedQScanPointer?
  unsigned long lQScanPointer = pStatus->GetQScanPointer();
  delete pStatus;
  if (p.qscan >= lQScanPointer) {
    WStatus* pStatus = application()->GetStatusManager()->BeginTransaction();
    if (p.qscan >= pStatus->GetQScanPointer()) {
      pStatus->SetQScanPointer(p.qscan + 1);
    }
    application()->GetStatusManager()->CommitTransaction(pStatus);
  }
}

void lineadd(messagerec* pMessageRecord, const string& sx, string fileName) {
  const string line = StringPrintf("%s\r\n\x1a", sx.c_str());

  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    unique_ptr<File> message_file(OpenMessageFile(fileName));
    set_gat_section(message_file.get(), pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS);
    int new1 = 1;
    while (new1 < GAT_NUMBER_ELEMENTS && gat[new1] != 0) {
      ++new1;
    }
    int i = static_cast<int>(pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS);
    while (gat[i] != 65535) {
      i = gat[i];
    }
    char *b = nullptr;
    if ((b = static_cast<char*>(BbsAllocA(GAT_NUMBER_ELEMENTS))) == nullptr) {
      message_file->Close();
      return;
    }
    message_file->Seek(MSG_STARTING + static_cast<long>(i) * MSG_BLOCK_SIZE, File::seekBegin);
    message_file->Read(b, MSG_BLOCK_SIZE);
    int j = 0;
    while (j < MSG_BLOCK_SIZE && b[j] != CZ) {
      ++j;
    }
    strcpy(&(b[j]), line.c_str());
    message_file->Seek(MSG_STARTING + static_cast<long>(i) * MSG_BLOCK_SIZE, File::seekBegin);
    message_file->Write(b, MSG_BLOCK_SIZE);
    if (((j + line.size()) > MSG_BLOCK_SIZE) && (new1 != GAT_NUMBER_ELEMENTS)) {
      message_file->Seek(MSG_STARTING + static_cast<long>(new1)  * MSG_BLOCK_SIZE, File::seekBegin);
      message_file->Write(b + MSG_BLOCK_SIZE, MSG_BLOCK_SIZE);
      gat[new1] = 65535;
      gat[i] = static_cast< unsigned short >(new1);
      save_gat(message_file.get());
    }
    free(b);
    message_file->Close();
  }
  break;
  default:
    // illegal storage type
    break;
  }
}
