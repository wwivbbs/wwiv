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

#include "wwiv.h"
#include "instmsg.h"

#define EMAIL_STORAGE 2

#define NUM_ATTEMPTS_TO_OPEN_EMAIL 5
#define DELAY_BETWEEN_EMAIL_ATTEMPTS 9

#define GAT_NUMBER_ELEMENTS 2048
#define GAT_SECTION_SIZE    4096
#define MSG_BLOCK_SIZE      512


//
// Local function prototypes
//
WFile * OpenMessageFile(const std::string messageAreaFileName);
void set_gat_section(WFile *pMessageFile, int section);
void save_gat(WFile *pMessageFile);

static long gat_section;


/**
 * Sets the global variables pszOutOriginStr and pszOutOriginStr2.
 * Note: This is a private function
 */
void SetMessageOriginInfo(int nSystemNumber, int nUserNumber, std::string& strOutOriginStr,
                          std::string& strOutOriginStr2) {
  std::string netName;

  if (GetSession()->GetMaxNetworkNumber() > 1) {
    netName = net_networks[GetSession()->GetNetworkNumber()].name;
    netName += "- ";
  }

  strOutOriginStr.clear();
  strOutOriginStr2.clear();

  if (wwiv::strings::IsEqualsIgnoreCase(GetSession()->GetNetworkName(), "Internet") ||
      nSystemNumber == 32767) {
    strOutOriginStr = "Internet Mail and Newsgroups";
    return;
  }

  if (nSystemNumber && GetSession()->GetCurrentNetworkType() == net_type_wwivnet) {
    net_system_list_rec *csne = next_system(nSystemNumber);
    if (csne) {
      char szNetStatus[12];
      szNetStatus[0] = '\0';
      if (nUserNumber == 1) {
        if (csne->other & other_net_coord) {
          strcpy(szNetStatus, "{NC}");
        } else if (csne->other & other_group_coord) {
          sprintf(szNetStatus, "{GC%d}", csne->group);
        } else if (csne->other & other_area_coord) {
          strcpy(szNetStatus, "{AC}");
        }
      }
      char szFileName[ MAX_PATH ];
      sprintf(szFileName,
              "%s%s%c%s.%-3u",
              syscfg.datadir,
              REGIONS_DIR,
              WWIV_FILE_SEPERATOR_CHAR,
              REGIONS_DIR,
              atoi(csne->phone));

      char szDescription[ 81 ];
      if (WFile::Exists(szFileName)) {
        char szPhonePrefix[ 10 ];
        sprintf(szPhonePrefix, "%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
        describe_area_code_prefix(atoi(csne->phone), atoi(szPhonePrefix), szDescription);
      } else {
        describe_area_code(atoi(csne->phone), szDescription);
      }

      std::stringstream sstream;
      sstream << netName << csne->name << " [" << csne->phone << "] " << szNetStatus;
      strOutOriginStr = sstream.str();
      strOutOriginStr2 = (szDescription[0]) ? szDescription : "Unknown Area";
    } else {
      std::stringstream sstream;
      sstream << netName << "Unknown System";
      strOutOriginStr = sstream.str();
      strOutOriginStr2 = "Unknown Area";
    }
  }
}

/**
 * Deletes a message
 * This is a public function.
 */
void remove_link(messagerec * pMessageRecord, std::string fileName) {
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    WFile *pMessageFile = OpenMessageFile(fileName);
    if (pMessageFile->IsOpen()) {
      set_gat_section(pMessageFile, static_cast<int>(pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS));
      long lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
      while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
        long lNextSection = static_cast<long>(gat[ lCurrentSection ]);
        gat[lCurrentSection] = 0;
        lCurrentSection = lNextSection;
      }
      save_gat(pMessageFile);
      pMessageFile->Close();
      delete pMessageFile;
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
WFile * OpenMessageFile(const std::string messageAreaFileName) {
  GetApplication()->GetStatusManager()->RefreshStatusCache();

  std::stringstream sstream;
  sstream << syscfg.msgsdir << messageAreaFileName << FILENAME_DAT_EXTENSION << std::ends;
  WFile *pFileMessage = new WFile(sstream.str());
  if (!pFileMessage->Open(WFile::modeReadWrite | WFile::modeBinary)) {
    // Create message area file if it doesn't exist.
    pFileMessage->Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown,
                       WFile::permReadWrite);
    for (int i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
      gat[i] = 0;
    }
    pFileMessage->Write(gat, GAT_SECTION_SIZE);
    //strcpy( g_szMessageGatFileName, pFileMessage->GetFullPathName() );
    pFileMessage->SetLength(GAT_SECTION_SIZE + (75L * 1024L));
    gat_section = 0;
  }
  pFileMessage->Seek(0L, WFile::seekBegin);
  pFileMessage->Read(gat, GAT_SECTION_SIZE);
  //strcpy( g_szMessageGatFileName, pFileMessage->GetFullPathName() );
  gat_section = 0;
  return pFileMessage;
}


#define GATSECLEN ( GAT_SECTION_SIZE + GAT_NUMBER_ELEMENTS * MSG_BLOCK_SIZE )
#ifndef MSG_STARTING
#define MSG_STARTING ( static_cast<long>( gat_section ) * GATSECLEN + GAT_SECTION_SIZE )
#endif  // MSG_STARTING


void set_gat_section(WFile *pMessageFile, int section) {
  if (gat_section != section) {
    long lFileSize = pMessageFile->GetLength();
    long lSectionPos = static_cast<long>(section) * GATSECLEN;
    if (lFileSize < lSectionPos) {
      pMessageFile->SetLength(lSectionPos);
      lFileSize = lSectionPos;
    }
    pMessageFile->Seek(lSectionPos, WFile::seekBegin);
    if (lFileSize < (lSectionPos + GAT_SECTION_SIZE)) {
      for (int i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
        gat[ i ] = 0;
      }
      pMessageFile->Write(gat, GAT_SECTION_SIZE);
    } else {
      pMessageFile->Read(gat, GAT_SECTION_SIZE);
    }
    gat_section = section;
  }
}


void save_gat(WFile *pMessageFile) {
  long lSectionPos = static_cast<long>(gat_section) * GATSECLEN;
  pMessageFile->Seek(lSectionPos, WFile::seekBegin);
  pMessageFile->Write(gat, GAT_SECTION_SIZE);
  WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangePosts);
  GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
}


void savefile(char *b, long lMessageLength, messagerec * pMessageRecord, const std::string fileName) {
  WWIV_ASSERT(pMessageRecord);
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    int gati[128];
    WFile *pMessageFile = OpenMessageFile(fileName);
    if (pMessageFile->IsOpen()) {
      for (int section = 0; section < 1024; section++) {
        set_gat_section(pMessageFile, section);
        int gatp = 0;
        int nNumBlocksRequired = static_cast<int>((lMessageLength + 511L) / MSG_BLOCK_SIZE);
        int i4 = 1;
        while (gatp < nNumBlocksRequired && i4 < GAT_NUMBER_ELEMENTS) {
          if (gat[ i4 ] == 0) {
            gati[ gatp++ ] = i4;
          }
          ++i4;
        }
        if (gatp >= nNumBlocksRequired) {
          gati[ gatp ] = -1;
          for (int i = 0; i < nNumBlocksRequired; i++) {
            pMessageFile->Seek(MSG_STARTING + MSG_BLOCK_SIZE * static_cast<long>(gati[i]), WFile::seekBegin);
            pMessageFile->Write((&b[i * MSG_BLOCK_SIZE]), MSG_BLOCK_SIZE);
            gat[gati[i]] = static_cast< unsigned short >(gati[i + 1]);
          }
          save_gat(pMessageFile);
          break;
        }
      }
      pMessageFile->Close();
      delete pMessageFile;
    }
    pMessageRecord->stored_as = static_cast<long>(gati[0]) + static_cast<long>(gat_section) * GAT_NUMBER_ELEMENTS;
  }
  break;
  default: {
    GetSession()->bout.WriteFormatted("WWIV:ERROR:msgbase.cpp: Save - storage_type=%u!\r\n", pMessageRecord->storage_type);
    WWIV_ASSERT(false);
  }
  break;
  }
  BbsFreeMemory(b);
}


char *readfile(messagerec * pMessageRecord, std::string fileName, long *plMessageLength) {
  char *b =  NULL;

  *plMessageLength = 0L;
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    WFile * pMessageFile = OpenMessageFile(fileName);
    set_gat_section(pMessageFile, static_cast< int >(pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS));
    int lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
    long lMessageLength = 0;
    while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
      lMessageLength += MSG_BLOCK_SIZE;
      lCurrentSection = gat[ lCurrentSection ];
    }
    if (lMessageLength == 0) {
      GetSession()->bout << "\r\nNo message found.\r\n\n";
      pMessageFile->Close();
      delete pMessageFile;
      return NULL;
    }
    if ((b = static_cast<char *>(BbsAllocA(lMessageLength + 512))) == NULL) {         // was +3
      pMessageFile->Close();
      delete pMessageFile;
      return NULL;
    }
    lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
    long lMessageBytesRead = 0;
    while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
      pMessageFile->Seek(MSG_STARTING + MSG_BLOCK_SIZE * static_cast< long >(lCurrentSection), WFile::seekBegin);
      lMessageBytesRead += static_cast<long>(pMessageFile->Read(&(b[lMessageBytesRead]), MSG_BLOCK_SIZE));
      lCurrentSection = gat[ lCurrentSection ];
    }
    pMessageFile->Close();
    delete pMessageFile;
    long lRealMessageLength = lMessageBytesRead - MSG_BLOCK_SIZE;
    while ((lRealMessageLength < lMessageBytesRead) && (b[lRealMessageLength] != CZ)) {
      ++lRealMessageLength;
    }
    *plMessageLength = lRealMessageLength;
    b[ lRealMessageLength + 1 ] = '\0';
  }
  break;
  default:
    // illegal storage type
    *plMessageLength = 0L;
    b = NULL;
    break;
  }
  return b;
}


void LoadFileIntoWorkspace(const char *pszFileName, bool bNoEditAllowed) {
  WFile fileOrig(pszFileName);
  if (!fileOrig.Open(WFile::modeBinary | WFile::modeReadOnly)) {
    GetSession()->bout << "\r\nFile not found.\r\n\n";
    return;
  }

  long lOrigSize = fileOrig.GetLength();
  char* b = static_cast<char*>(BbsAllocA(lOrigSize + 1024));
  if (b == NULL) {
    fileOrig.Close();
    return;
  }
  fileOrig.Read(b, lOrigSize);
  fileOrig.Close();
  if (b[lOrigSize - 1] != CZ) {
    b[lOrigSize++] = CZ;
  }

  WFile fileOut(syscfgovr.tempdir, INPUT_MSG);
  fileOut.Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown,
               WFile::permReadWrite);
  fileOut.Write(b, lOrigSize);
  fileOut.Close();
  BbsFreeMemory(b);

  use_workspace = (bNoEditAllowed || !okfsed()) ? true : false;

  if (!GetSession()->IsNewMailWatiting()) {
    GetSession()->bout << "\r\nFile loaded into workspace.\r\n\n";
    if (!use_workspace) {
      GetSession()->bout << "Editing will be allowed.\r\n";
    }
  }
}

// returns true on success (i.e. the message gets forwarded)
bool ForwardMessage(int *pUserNumber, int *pSystemNumber) {
  if (*pSystemNumber) {
    return false;
  }

  WUser userRecord;
  GetApplication()->GetUserManager()->ReadUser(&userRecord, *pUserNumber);
  if (userRecord.IsUserDeleted()) {
    return false;
  }
  if (userRecord.GetForwardUserNumber() == 0 && userRecord.GetForwardSystemNumber() == 0 &&
      userRecord.GetForwardSystemNumber() != 32767) {
    return false;
  }
  if (userRecord.GetForwardSystemNumber() != 0) {
    if (!userRecord.IsMailForwardedToInternet()) {
      int nNetworkNumber = GetSession()->GetNetworkNumber();
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
    GetSession()->bout << "Mailbox Closed.\r\n";
    if (so()) {
      GetSession()->bout << "(Forcing)\r\n";
    } else {
      *pUserNumber = 0;
      *pSystemNumber = 0;
    }
    return false;
  }
  char *ss = static_cast<char*>(BbsAllocA(static_cast<long>(syscfg.maxusers) + 300L));
  if (ss == NULL) {
    return false;
  }
  for (int i = 0; i < syscfg.maxusers + 300; i++) {
    ss[i] = '\0';
  }
  ss[*pUserNumber] = 1;
  GetApplication()->GetUserManager()->ReadUser(&userRecord, nCurrentUser);
  while (userRecord.GetForwardUserNumber() || userRecord.GetForwardSystemNumber()) {
    if (userRecord.GetForwardSystemNumber()) {
      if (!valid_system(userRecord.GetForwardSystemNumber())) {
        return false;
      }
      *pUserNumber = userRecord.GetForwardUserNumber();
      *pSystemNumber = userRecord.GetForwardSystemNumber();
      BbsFreeMemory(ss);
      set_net_num(userRecord.GetForwardNetNumber());
      return true;
    }
    if (ss[ nCurrentUser ]) {
      BbsFreeMemory(ss);
      return false;
    }
    ss[ nCurrentUser ] = 1;
    if (userRecord.IsMailboxClosed()) {
      GetSession()->bout << "Mailbox Closed.\r\n";
      if (so()) {
        GetSession()->bout << "(Forcing)\r\n";
        *pUserNumber = nCurrentUser;
        *pSystemNumber = 0;
      } else {
        *pUserNumber = 0;
        *pSystemNumber = 0;
      }
      BbsFreeMemory(ss);
      return false;
    }
    nCurrentUser = userRecord.GetForwardUserNumber() ;
    GetApplication()->GetUserManager()->ReadUser(&userRecord, nCurrentUser);
  }
  BbsFreeMemory(ss);
  *pSystemNumber = 0;
  *pUserNumber = nCurrentUser;
  return true;
}


WFile *OpenEmailFile(bool bAllowWrite) {
  WFile *file = new WFile(syscfg.datadir, EMAIL_DAT);

  // If the file doesn't exist, just return the opaque handle now instead of flailing
  // around trying to open it
  if (!file->Exists()) {
    // if it does not exist, try to create it via the open call
    // sf bug 1215434
    file->Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite);
    return file;
  }

  for (int nAttempNum = 0; nAttempNum < NUM_ATTEMPTS_TO_OPEN_EMAIL; nAttempNum++) {
    if (bAllowWrite) {
      file->Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite);
    } else {
      file->Open(WFile::modeBinary | WFile::modeReadOnly);
    }
    if (file->IsOpen()) {
      break;
    }
    wait1(DELAY_BETWEEN_EMAIL_ATTEMPTS);
  }

  return file;
}


void sendout_email(const char *pszTitle, messagerec * pMessageRec, int anony, int nUserNumber, int nSystemNumber,
                   int an, int nFromUser, int nFromSystem, int nForwardedCode, int nFromNetworkNumber) {
  mailrec m, messageRecord;
  net_header_rec nh;
  int i;
  char *b, *b1;

  strcpy(m.title, pszTitle);
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
  m.daten = static_cast<unsigned long>(time(NULL));

  if (m.fromsys && GetSession()->GetMaxNetworkNumber() > 1) {
    m.status |= status_new_net;
    m.title[79] = '\0';
    m.title[80] = static_cast<char>(nFromNetworkNumber);
  }

  if (nSystemNumber == 0) {
    WFile *pFileEmail = OpenEmailFile(true);
    WWIV_ASSERT(pFileEmail);
    if (!pFileEmail->IsOpen()) {
      return;
    }
    int nEmailFileLen = static_cast< int >(pFileEmail->GetLength() / sizeof(mailrec));
    if (nEmailFileLen == 0) {
      i = 0;
    } else {
      i = nEmailFileLen - 1;
      pFileEmail->Seek(i * sizeof(mailrec), WFile::seekBegin);
      pFileEmail->Read(&messageRecord, sizeof(mailrec));
      while (i > 0 && messageRecord.tosys == 0 && messageRecord.touser == 0) {
        --i;
        pFileEmail->Seek(i * sizeof(mailrec), WFile::seekBegin);
        int i1 = pFileEmail->Read(&messageRecord, sizeof(mailrec));
        if (i1 == -1) {
          GetSession()->bout << "|#6DIDN'T READ WRITE!\r\n";
        }
      }
      if (messageRecord.tosys || messageRecord.touser) {
        ++i;
      }
    }

    pFileEmail->Seek(i * sizeof(mailrec), WFile::seekBegin);
    int nBytesWritten = pFileEmail->Write(&m, sizeof(mailrec));
    pFileEmail->Close();
    delete pFileEmail;
    if (nBytesWritten == -1) {
      GetSession()->bout << "|#6DIDN'T SAVE RIGHT!\r\n";
    }
  } else {
    long lEmailFileLen;
    if ((b = readfile(&(m.msg), "email", &lEmailFileLen)) == NULL) {
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
    if ((b1 = static_cast<char*>(BbsAllocA(lEmailFileLen + 768))) == NULL) {
      BbsFreeMemory(b);
      return;
    }
    i = 0;
    if (nUserNumber == 0 && nFromNetworkNumber == GetSession()->GetNetworkNumber()) {
      nh.main_type = main_type_email_name;
      strcpy(&(b1[i]), net_email_name);
      i += strlen(net_email_name) + 1;
    }
    strcpy(&(b1[i]), m.title);
    i += strlen(m.title) + 1;
    memmove(&(b1[i]), b,  lEmailFileLen);
    nh.length = lEmailFileLen + i;
    if (nh.length > 32760) {
      GetSession()->bout.WriteFormatted("Message truncated by %lu bytes for the network.", nh.length - 32760L);
      nh.length = 32760;
    }
    if (nFromNetworkNumber != GetSession()->GetNetworkNumber()) {
      gate_msg(&nh, b1, GetSession()->GetNetworkNumber(), net_email_name, NULL, nFromNetworkNumber);
    } else {
      char szNetFileName[MAX_PATH];
      if (nForwardedCode) {
        sprintf(szNetFileName, "%sp1%s", GetSession()->GetNetworkDataDirectory(), GetApplication()->GetNetworkExtension());
      } else {
        sprintf(szNetFileName, "%sp0%s", GetSession()->GetNetworkDataDirectory(), GetApplication()->GetNetworkExtension());
      }
      WFile fileNetworkPacket(szNetFileName);
      fileNetworkPacket.Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown,
                             WFile::permReadWrite);
      fileNetworkPacket.Seek(0L, WFile::seekBegin);
      fileNetworkPacket.Write(&nh, sizeof(net_header_rec));
      fileNetworkPacket.Write(b1, nh.length);
      fileNetworkPacket.Close();
    }
    BbsFreeMemory(b);
    BbsFreeMemory(b1);
  }
  std::string logMessage = "Mail sent to ";
  if (nSystemNumber == 0) {
    WUser userRecord;
    GetApplication()->GetUserManager()->ReadUser(&userRecord, nUserNumber);
    userRecord.SetNumMailWaiting(userRecord.GetNumMailWaiting() + 1);
    GetApplication()->GetUserManager()->WriteUser(&userRecord, nUserNumber);
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
      std::string tempLogMessage = logMessage;
      tempLogMessage += userRecord.GetUserNameAndNumber(nUserNumber);
      sysoplog(tempLogMessage);
      logMessage += ">UNKNOWN<";
    }
    if (nSystemNumber == 0 && GetSession()->GetEffectiveSl() > syscfg.newusersl && userRecord.GetForwardSystemNumber() == 0
        && !GetSession()->IsNewMailWatiting()) {
      GetSession()->bout << "|#5Attach a file to this message? ";
      if (yesno()) {
        attach_file(1);
      }
    }
  } else {
    std::string logMessagePart;
    if ((nSystemNumber == 1 &&
         wwiv::strings::IsEqualsIgnoreCase(GetSession()->GetNetworkName(), "Internet")) ||
        nSystemNumber == 32767) {
      logMessagePart = net_email_name;
    } else {
      if (GetSession()->GetMaxNetworkNumber() > 1) {
        if (nUserNumber == 0) {
          logMessagePart = wwiv::strings::StringPrintf("%s @%u.%s", net_email_name, nSystemNumber,
                           GetSession()->GetNetworkName());
        } else {
          logMessagePart = wwiv::strings::StringPrintf("#%u @%u.%s", nUserNumber, nSystemNumber, GetSession()->GetNetworkName());
        }
      } else {
        if (nUserNumber == 0) {
          logMessagePart = wwiv::strings::StringPrintf("%s @%u", net_email_name, nSystemNumber);
        } else {
          logMessagePart = wwiv::strings::StringPrintf("#%u @%u", nUserNumber, nSystemNumber);
        }
      }
    }
    logMessage += logMessagePart;
    sysoplog(logMessage);
  }

  WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
  if (nUserNumber == 1 && nSystemNumber == 0) {
    pStatus->IncrementNumFeedbackSentToday();
    GetSession()->GetCurrentUser()->SetNumFeedbackSent(GetSession()->GetCurrentUser()->GetNumFeedbackSent() + 1);
    GetSession()->GetCurrentUser()->SetNumFeedbackSentToday(GetSession()->GetCurrentUser()->GetNumFeedbackSentToday() + 1);
    ++fsenttoday;
  } else {
    pStatus->IncrementNumEmailSentToday();
    GetSession()->GetCurrentUser()->SetNumEmailSentToday(GetSession()->GetCurrentUser()->GetNumEmailSentToday() + 1);
    if (nSystemNumber == 0) {
      GetSession()->GetCurrentUser()->SetNumEmailSent(GetSession()->GetCurrentUser()->GetNumEmailSent() + 1);
    } else {
      GetSession()->GetCurrentUser()->SetNumNetEmailSent(GetSession()->GetCurrentUser()->GetNumNetEmailSent() + 1);
    }
  }
  GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
  if (!GetSession()->IsNewMailWatiting()) {
    GetSession()->bout.Color(3);
    GetSession()->bout << logMessage;
    GetSession()->bout.NewLine();
  }
}


bool ok_to_mail(int nUserNumber, int nSystemNumber, bool bForceit) {
  if (nSystemNumber != 0 && net_sysnum == 0) {
    GetSession()->bout << "\r\nSorry, this system is not a part of WWIVnet.\r\n\n";
    return false;
  }
  if (nSystemNumber == 0) {
    if (nUserNumber == 0) {
      return false;
    }
    WUser userRecord;
    GetApplication()->GetUserManager()->ReadUser(&userRecord, nUserNumber);
    if ((userRecord.GetSl() == 255 && userRecord.GetNumMailWaiting() > (syscfg.maxwaiting * 5)) ||
        (userRecord.GetSl() != 255 && userRecord.GetNumMailWaiting() > syscfg.maxwaiting) ||
        userRecord.GetNumMailWaiting() > 200) {
      if (!bForceit) {
        GetSession()->bout << "\r\nMailbox full.\r\n";
        return false;
      }
    }
    if (userRecord.IsUserDeleted()) {
      GetSession()->bout << "\r\nDeleted user.\r\n\n";
      return false;
    }
  } else {
    if (!valid_system(nSystemNumber)) {
      GetSession()->bout << "\r\nUnknown system number.\r\n\n";
      return false;
    }
    if (GetSession()->GetCurrentUser()->IsRestrictionNet()) {
      GetSession()->bout << "\r\nYou can't send mail off the system.\r\n";
      return false;
    }
  }
  if (!GetSession()->IsNewMailWatiting() && !bForceit) {
    if (((nUserNumber == 1 && nSystemNumber == 0 &&
          (fsenttoday >= 5 || GetSession()->GetCurrentUser()->GetNumFeedbackSentToday() >= 10)) ||
         ((nUserNumber != 1 || nSystemNumber != 0) &&
          (GetSession()->GetCurrentUser()->GetNumEmailSentToday() >= getslrec(GetSession()->GetEffectiveSl()).emails)))
        && !cs()) {
      GetSession()->bout << "\r\nToo much mail sent today.\r\n\n";
      return false;
    }
    if (GetSession()->GetCurrentUser()->IsRestrictionEmail() && nUserNumber != 1) {
      GetSession()->bout << "\r\nYou can't send mail.\r\n\n";
      return false;
    }
  }
  return true;
}


void email(int nUserNumber, int nSystemNumber, bool forceit, int anony, bool force_title, bool bAllowFSED) {
  int an;
  int nNumUsers = 0;
  messagerec messageRecord;
  char szDestination[81], szTitle[81];
  WUser userRecord;
  net_system_list_rec *csne = NULL;
  struct {
    int nUserNumber, nSystemNumber, net_num;
    char net_name[20], net_email_name[40];
  } carbon_copy[20];

  bool cc = false, bcc = false;

  if (freek1(syscfg.msgsdir) < 10.0) {
    GetSession()->bout << "\r\nSorry, not enough disk space left.\r\n\n";
    return;
  }
  GetSession()->bout.NewLine();
  if (ForwardMessage(&nUserNumber, &nSystemNumber)) {
    if (nSystemNumber == 32767) {
      read_inet_addr(szDestination, nUserNumber);
    }
    GetSession()->bout << "\r\nMail Forwarded.\r\n\n";
    if (nUserNumber == 0 && nSystemNumber == 0) {
      GetSession()->bout << "Forwarded to unknown user.\r\n";
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
  if (getslrec(GetSession()->GetEffectiveSl()).ability & ability_read_email_anony) {
    an = 1;
  } else if (anony & (anony_sender | anony_sender_da | anony_sender_pp)) {
    an = 0;
  } else {
    an = 1;
  }
  if (nSystemNumber == 0) {
    set_net_num(0);
    if (an) {
      GetApplication()->GetUserManager()->ReadUser(&userRecord, nUserNumber);
      strcpy(szDestination, userRecord.GetUserNameAndNumber(nUserNumber));
    } else {
      strcpy(szDestination, ">UNKNOWN<");
    }
  } else {
    if ((nSystemNumber == 1 && nUserNumber == 0 &&
         wwiv::strings::IsEqualsIgnoreCase(GetSession()->GetNetworkName(), "Internet")) ||
        nSystemNumber == 32767) {
      strcpy(szDestination, net_email_name);
    } else {
      if (GetSession()->GetMaxNetworkNumber() > 1) {
        if (nUserNumber == 0) {
          sprintf(szDestination, "%s @%u.%s", net_email_name, nSystemNumber, GetSession()->GetNetworkName());
        } else {
          sprintf(szDestination, "#%u @%u.%s", nUserNumber, nSystemNumber, GetSession()->GetNetworkName());
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
  if (!GetSession()->IsNewMailWatiting()) {
    GetSession()->bout << "|#9E-mailing |#2";
  }
  GetSession()->bout << szDestination;
  GetSession()->bout.NewLine();
  int i = (getslrec(GetSession()->GetEffectiveSl()).ability & ability_email_anony) ? anony_enable_anony : 0;

  if (anony & (anony_receiver_pp | anony_receiver_da)) {
    i = anony_enable_dear_abby;
  }
  if (anony & anony_receiver) {
    i = anony_enable_anony;
  }
  if (i == anony_enable_anony && GetSession()->GetCurrentUser()->IsRestrictionAnonymous()) {
    i = 0;
  }
  if (nSystemNumber != 0) {
    if (nSystemNumber != 32767) {
      i = 0;
      anony = 0;
      GetSession()->bout.NewLine();
      WWIV_ASSERT(csne);
      GetSession()->bout << "|#9Name of system: |#2" << csne->name << wwiv::endl;
      GetSession()->bout << "|#9Number of hops: |#2" << csne->numhops << wwiv::endl;
      GetSession()->bout.NewLine();
    }
  }
  write_inst(INST_LOC_EMAIL, (nSystemNumber == 0) ? nUserNumber : 0, INST_FLAGS_NONE);

  messageRecord.storage_type = EMAIL_STORAGE;
  int nUseFSED = (bAllowFSED) ? INMSG_FSED : INMSG_NOFSED;
  inmsg(&messageRecord, szTitle, &i, !forceit, "email", nUseFSED, szDestination, MSGED_FLAG_NONE, force_title);
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

  if (GetSession()->IsCarbonCopyEnabled()) {
    if (!GetSession()->IsNewMailWatiting()) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#9Copy this mail to others? ";
      nNumUsers = 0;
      if (yesno()) {
        bool done = false;
        carbon_copy[nNumUsers].nUserNumber = nUserNumber;
        carbon_copy[nNumUsers].nSystemNumber = nSystemNumber;
        strcpy(carbon_copy[nNumUsers].net_name, GetSession()->GetNetworkName());
        strcpy(carbon_copy[nNumUsers].net_email_name, net_email_name);
        carbon_copy[nNumUsers].net_num = GetSession()->GetNetworkNumber();
        nNumUsers++;
        do {
          std::string emailAddress;
          GetSession()->bout << "|#9Enter Address (blank to end) : ";
          input(emailAddress, 75);
          if (emailAddress.empty()) {
            done = true;
            break;
          }
          int tu, ts;
          parse_email_info(emailAddress, &tu, &ts);
          if (tu || ts) {
            carbon_copy[nNumUsers].nUserNumber = tu;
            carbon_copy[nNumUsers].nSystemNumber = ts;
            strcpy(carbon_copy[nNumUsers].net_name, GetSession()->GetNetworkName());
            strcpy(carbon_copy[nNumUsers].net_email_name, net_email_name);
            carbon_copy[nNumUsers].net_num = GetSession()->GetNetworkNumber();
            nNumUsers++;
            cc = true;
          }
          if (nNumUsers == 20) {
            GetSession()->bout << "|#6Maximum number of addresses reached!";
            done = true;
          }
        } while (!done);
        if (cc) {
          GetSession()->bout << "|#9Show Recipients in message? ";
          bcc = !yesno();
        }
      }
    }

    if (cc && !bcc) {
      int listed = 0;
      std::string s1 = "\003""6Carbon Copy: \003""1";
      lineadd(&messageRecord, "\003""7----", "email");
      for (int j = 0; j < nNumUsers; j++) {
        if (carbon_copy[j].nSystemNumber == 0) {
          set_net_num(0);
          GetApplication()->GetUserManager()->ReadUser(&userRecord, carbon_copy[j].nUserNumber);
          strcpy(szDestination, userRecord.GetUserNameAndNumber(carbon_copy[j].nUserNumber));
        } else {
          if (carbon_copy[j].nSystemNumber == 1 &&
              carbon_copy[j].nUserNumber == 0 &&
              wwiv::strings::IsEqualsIgnoreCase(carbon_copy[j].net_name, "Internet")) {
            strcpy(szDestination, carbon_copy[j].net_email_name);
          } else {
            set_net_num(carbon_copy[j].net_num);
            if (GetSession()->GetMaxNetworkNumber() > 1) {
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
          s1 = wwiv::strings::StringPrintf("\003""6Original To: \003""1%s", szDestination);
          lineadd(&messageRecord, s1.c_str(), "email");
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
            lineadd(&messageRecord, s1.c_str(), "email");
            s1 += "\003""1             ";
            j--;
            listed = 1;
          }
        }
      }
      if (!listed) {
        lineadd(&messageRecord, s1.c_str(), "email");
      }
    }
  }
  if (cc) {
    for (int nCounter = 0; nCounter < nNumUsers; nCounter++) {
      set_net_num(carbon_copy[nCounter].net_num);
      sendout_email(szTitle, &messageRecord, i, carbon_copy[nCounter].nUserNumber, carbon_copy[nCounter].nSystemNumber, an,
                    GetSession()->usernum, net_sysnum, 0, carbon_copy[nCounter].net_num);
    }
  } else {
    sendout_email(szTitle, &messageRecord, i, nUserNumber, nSystemNumber, an, GetSession()->usernum, net_sysnum, 0,
                  GetSession()->GetNetworkNumber());
  }
}


void imail(int nUserNumber, int nSystemNumber) {
  char szInternetAddr[ 255 ];
  WUser userRecord;

  int fwdu = nUserNumber;
  bool fwdm = false;

  if (ForwardMessage(&nUserNumber, &nSystemNumber)) {
    GetSession()->bout << "Mail forwarded.\r\n";
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
    GetApplication()->GetUserManager()->ReadUser(&userRecord, nUserNumber);
    if (!userRecord.IsUserDeleted()) {
      GetSession()->bout << "|#5E-mail " << userRecord.GetUserNameAndNumber(nUserNumber) << "? ";
      if (!yesno()) {
        i = 0;
      }
    } else {
      i = 0;
    }
  } else {
    if (fwdm) {
      GetSession()->bout << "|#5E-mail " << szInternetAddr << "? ";
    } else {
      GetSession()->bout << "|#5E-mail User " << nUserNumber << " @" << nSystemNumber << " ? ";
    }
    if (!yesno()) {
      i = 0;
    }
  }
  grab_quotes(NULL, NULL);
  if (i) {
    email(nUserNumber, nSystemNumber, false, 0);
  }
}


void read_message1(messagerec * pMessageRecord, char an, bool readit, bool *next, const char *pszFileName,
                   int nFromSystem, int nFromUser) {
  std::string strName, strDate;
  char s[205];
  long lMessageTextLength;

  std::string origin_str;
  std::string origin_str2;

  // Moved internally from outside this method
  SetMessageOriginInfo(nFromSystem, nFromUser, origin_str, origin_str2);

  g_flags &= ~g_flag_ansi_movement;

  char* ss = NULL;
  bool ansi = false;
  *next = false;
  long lCurrentCharPointer = 0;
  bool abort = false;
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
  case 2: {
    ss = readfile(pMessageRecord, pszFileName, &lMessageTextLength);
    if (ss == NULL) {
      plan(6, "File not found.", &abort, next);
      GetSession()->bout.NewLine();
      return;
    }
    int nNamePtr = 0;
    while ((ss[nNamePtr] != RETURN) &&
           (static_cast<long>(nNamePtr) < lMessageTextLength) &&
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
      strDate.push_back(ss[(nDatePtr++) + nNamePtr ]);
    }

    lCurrentCharPointer = nNamePtr + nDatePtr + 1;
  }
  break;
  default:
    // illegal storage type
    GetSession()->bout.NewLine();
    GetSession()->bout << "->ILLEGAL STORAGE TYPE<-\r\n\n";
    return;
  }

  irt_name[0] = '\0';
  g_flags |= g_flag_disable_mci;
  switch (an) {
  default:
  case 0:
    if (syscfg.sysconfig & sysconfig_enable_mci) {
      g_flags &= ~g_flag_disable_mci;
    }
    osan("|#1Name|#7: ", &abort, next);
    plan(GetSession()->GetMessageColor(), strName, &abort, next);
    strcpy(irt_name, strName.c_str());
    osan("|#1Date|#7: ", &abort, next);
    plan(GetSession()->GetMessageColor(), strDate, &abort, next);
    if (!origin_str.empty()) {
      if (strName[1] == '`') {
        osan("|#1Gated From|#7: ", &abort, next);
      } else {
        osan("|#1From|#7: ", &abort, next);
      }
      plan(GetSession()->GetMessageColor(), origin_str, &abort, next);
    }
    if (!origin_str2.empty()) {
      osan("|#1Loc|#7:  ", &abort, next);
      plan(GetSession()->GetMessageColor(), origin_str2, &abort, next);
    }
    break;
  case anony_sender:
    if (readit) {
      osan("|#1Name|#7: ", &abort, next);
      std::stringstream toName;
      toName << "<<< " << strName << " >>>";
      plan(GetSession()->GetMessageColor(), toName.str(), &abort, next);
      osan("|#1Date|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), strDate, &abort, next);
    } else {
      osan("|#1Name|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), ">UNKNOWN<", &abort, next);
      osan("|#1Date|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), ">UNKNOWN<", &abort, next);
    }
    break;
  case anony_sender_da:
  case anony_sender_pp:
    if (an == anony_sender_da) {
      osan("|#1Name|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), "Abby", &abort, next);
    } else {
      osan("|#1Name|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), "Problemed Person", &abort, next);
    }
    if (readit) {
      osan("|#1Name|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), strName, &abort, next);
      osan("|#1Date|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), strDate, &abort, next);
    } else {
      osan("|#1Date|#7: ", &abort, next);
      plan(GetSession()->GetMessageColor(), ">UNKNOWN<", &abort, next);
    }
    break;
  }

  int nNumCharsPtr  = 0;
  int nLineLenPtr   = 0;
  int ctrld     = 0;
  char ch       = 0;
  bool done     = false;
  bool printit    = false;
  bool ctrla      = false;
  bool centre     = false;

  GetSession()->bout.NewLine();
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
            if (GetSession()->GetCurrentUser()->GetOptionalVal() < (ch - '0')) {
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
            if (GetSession()->localIO()->GetTopLine() && GetSession()->localIO()->GetScreenBottom() == 24) {
              GetSession()->localIO()->set_protect(0);
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
            int nSpacesToCenter = (GetSession()->GetCurrentUser()->GetScreenChars() - GetSession()->localIO()->WhereX() -
                                   nLineLenPtr) / 2;
            osan(charstr(nSpacesToCenter, ' '), &abort, next);
          }
          if (nNumCharsPtr) {
            if (ctrld != -1) {
              if ((GetSession()->localIO()->WhereX() + nLineLenPtr >= GetSession()->GetCurrentUser()->GetScreenChars()) && !centre
                  && !ansi) {
                GetSession()->bout.NewLine();
              }
              s[nNumCharsPtr] = '\0';
              osan(s, &abort, next);
              if (ctrla && s[nNumCharsPtr - 1] != SPACE && !ansi) {
                if (GetSession()->localIO()->WhereX() < GetSession()->GetCurrentUser()->GetScreenChars() - 1) {
                  bputch(SPACE);
                } else {
                  GetSession()->bout.NewLine();
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
              GetSession()->bout.NewLine();
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
    GetSession()->bout << s;
    GetSession()->bout.NewLine();
  }
  GetSession()->bout.Color(0);
  GetSession()->bout.NewLine();
  if (express && abort && !*next) {
    expressabort = true;
  }
  if (ss != NULL) {
    BbsFreeMemory(ss);
  }
  tmp_disable_pause(false);
  if (ansi && GetSession()->topdata && GetSession()->IsUserOnline()) {
    GetApplication()->UpdateTopScreen();
  }
  if (syscfg.sysconfig & sysconfig_enable_mci) {
    g_flags &= ~g_flag_disable_mci;
  }
}




void read_message(int n, bool *next, int *val) {
  GetSession()->bout.NewLine();
  bool abort = false;
  *next = false;
  if (GetSession()->GetCurrentUser()->IsUseClearScreen()) {
    GetSession()->bout.ClearScreen();
  }
  if (forcescansub) {
    GetSession()->bout.ClearScreen();
    GetSession()->bout.GotoXY(1, 1);
    GetSession()->bout << "|#4   FORCED SCAN OF SYSOP INFORMATION - YOU MAY NOT ABORT.  PLEASE READ THESE!  |#0\r\n";
  }

  std::string subjectLine;
  GetSession()->bout.WriteFormatted(" |#1Msg|#7: [|#2%u|#7/|#2%lu|#7]|#%d %s\r\n", n,
                                    GetSession()->GetNumMessagesInCurrentMessageArea(), GetSession()->GetMessageColor(),
                                    subboards[GetSession()->GetCurrentReadMessageArea()].name);
  subjectLine = "|#1Subj|#7: ";
  osan(subjectLine, &abort, next);
  GetSession()->bout.Color(GetSession()->GetMessageColor());
  postrec p = *get_post(n);
  if (p.status & (status_unvalidated | status_delete)) {
    plan(6, "<<< NOT VALIDATED YET >>>", &abort, next);
    if (!lcs()) {
      return;
    }
    *val |= 1;
    osan(subjectLine, &abort, next);
    GetSession()->bout.Color(GetSession()->GetMessageColor());
  }
  strncpy(irt, p.title, 60);
  irt_name[0] = '\0';
  plan(GetSession()->GetMessageColor(), irt, &abort, next);
  if ((p.status & status_no_delete) && lcs()) {
    osan("|#1Info|#7: ", &abort, next);
    plan(GetSession()->GetMessageColor(), "Permanent Message", &abort, next);
  }
  if ((p.status & status_pending_net) && GetSession()->GetCurrentUser()->GetSl() > syscfg.newusersl) {
    osan("|#1Val|#7:  ", &abort, next);
    plan(GetSession()->GetMessageColor(), "Not Network Validated", &abort, next);
    *val |= 2;
  }
  if (!abort) {
    bool bReadit = (lcs() || (getslrec(GetSession()->GetEffectiveSl()).ability & ability_read_post_anony)) ? true : false;
    int nNetNumSaved = GetSession()->GetNetworkNumber();

    if (p.status & status_post_new_net) {
      set_net_num(p.title[80]);
    }
    read_message1(&(p.msg), static_cast<char>(p.anony & 0x0f), bReadit, next,
                  (subboards[GetSession()->GetCurrentReadMessageArea()].filename), p.ownersys, p.owneruser);

    if (nNetNumSaved != GetSession()->GetNetworkNumber()) {
      set_net_num(nNetNumSaved);
    }

    GetSession()->GetCurrentUser()->SetNumMessagesRead(GetSession()->GetCurrentUser()->GetNumMessagesRead() + 1);
    GetSession()->SetNumMessagesReadThisLogon(GetSession()->GetNumMessagesReadThisLogon() + 1);
  } else if (express && !*next) {
    expressabort = true;
  }
  if (p.qscan > qsc_p[GetSession()->GetCurrentReadMessageArea()]) {
    qsc_p[GetSession()->GetCurrentReadMessageArea()] = p.qscan;
  }
  WStatus *pStatus = GetApplication()->GetStatusManager()->GetStatus();
  // not sure why we check this twice...
  // maybe we need a getCachedQScanPointer?
  unsigned long lQScanPointer = pStatus->GetQScanPointer();
  delete pStatus;
  if (p.qscan >= lQScanPointer) {
    WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
    if (p.qscan >= pStatus->GetQScanPointer()) {
      pStatus->SetQScanPointer(p.qscan + 1);
    }
    GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
  }
}


void lineadd(messagerec * pMessageRecord, const char *sx, std::string fileName) {
  char szLine[ 255 ];
  sprintf(szLine, "%s\r\n\x1a", sx);

  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2: {
    WFile * pMessageFile = OpenMessageFile(fileName);
    set_gat_section(pMessageFile, pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS);
    int new1 = 1;
    while (new1 < GAT_NUMBER_ELEMENTS && gat[new1] != 0) {
      ++new1;
    }
    int i = static_cast<int>(pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS);
    while (gat[i] != 65535) {
      i = gat[i];
    }
    char *b = NULL;
    if ((b = static_cast<char*>(BbsAllocA(GAT_NUMBER_ELEMENTS))) == NULL) {
      pMessageFile->Close();
      delete pMessageFile;
      return;
    }
    pMessageFile->Seek(MSG_STARTING + static_cast<long>(i) * MSG_BLOCK_SIZE, WFile::seekBegin);
    pMessageFile->Read(b, MSG_BLOCK_SIZE);
    int j = 0;
    while (j < MSG_BLOCK_SIZE && b[j] != CZ) {
      ++j;
    }
    strcpy(&(b[j]), szLine);
    pMessageFile->Seek(MSG_STARTING + static_cast<long>(i) * MSG_BLOCK_SIZE, WFile::seekBegin);
    pMessageFile->Write(b, MSG_BLOCK_SIZE);
    if (((j + strlen(szLine)) > MSG_BLOCK_SIZE) && (new1 != GAT_NUMBER_ELEMENTS)) {
      pMessageFile->Seek(MSG_STARTING + static_cast<long>(new1)  * MSG_BLOCK_SIZE, WFile::seekBegin);
      pMessageFile->Write(b + MSG_BLOCK_SIZE, MSG_BLOCK_SIZE);
      gat[new1] = 65535;
      gat[i] = static_cast< unsigned short >(new1);
      save_gat(pMessageFile);
    }
    BbsFreeMemory(b);
    pMessageFile->Close();
    delete pMessageFile;
  }
  break;
  default:
    // illegal storage type
    break;
  }
}


