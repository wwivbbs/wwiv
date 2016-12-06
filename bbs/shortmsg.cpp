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
/*    "AS IS"  BASIS, WITHOUT  WAxRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/shortmsg.h"

#include <cstdarg>
#include <string>
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::sdk;
using namespace wwiv::strings;

/*
 * Handles reading short messages. This is also where PackScan file requests
 * plug in, if such are used.
 */
void rsm(int nUserNum, User *pUser, bool bAskToSaveMsgs) {
  bool bShownAnyMessage = false;
  int bShownAllMessages = true;
  if (pUser->HasShortMessage()) {
    File file(a()->config()->datadir(), SMW_DAT);
    if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return;
    }
    int nTotalMsgsInFile = static_cast<int>(file.GetLength() / sizeof(shortmsgrec));
    for (int nCurrentMsg = 0; nCurrentMsg < nTotalMsgsInFile; nCurrentMsg++) {
      shortmsgrec sm;
      file.Seek(nCurrentMsg * sizeof(shortmsgrec), File::Whence::begin);
      file.Read(&sm, sizeof(shortmsgrec));
      if (sm.touser == nUserNum && sm.tosys == 0) {
        bout.Color(9);
        bout << sm.message;
        bout.nl();
        bool bHandledMessage = false;
        bShownAnyMessage = true;
        if (!so() || !bAskToSaveMsgs) {
          bHandledMessage = true;
        } else {
          if (a()->HasConfigFlag(OP_FLAGS_CAN_SAVE_SSM)) {
            if (!bHandledMessage && bAskToSaveMsgs) {
              bout << "|#5Would you like to save this notification? ";
              bHandledMessage = !yesno();
            }
          } else {
            bHandledMessage = true;
          }

        }
        if (bHandledMessage) {
          sm.touser = 0;
          sm.tosys = 0;
          sm.message[0] = 0;
          file.Seek(nCurrentMsg * sizeof(shortmsgrec), File::Whence::begin);
          file.Write(&sm, sizeof(shortmsgrec));
        } else {
          bShownAllMessages = false;
        }
      }
    }
    file.Close();
    smwcheck = true;
  }
  if (bShownAnyMessage) {
    bout.nl();
  }
  if (bShownAllMessages) {
    pUser->SetStatusFlag(User::SMW);
  }
}

static void SendLocalShortMessage(unsigned int nUserNum, const char *messageText) {
  User user;
  a()->users()->ReadUser(&user, nUserNum);
  if (!user.IsUserDeleted()) {
    File file(a()->config()->datadir(), SMW_DAT);
    if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return;
    }
    int nTotalMsgsInFile = static_cast<int>(file.GetLength() / sizeof(shortmsgrec));
    int nNewMsgPos = nTotalMsgsInFile - 1;
    shortmsgrec sm;
    if (nNewMsgPos >= 0) {
      file.Seek(nNewMsgPos * sizeof(shortmsgrec), File::Whence::begin);
      file.Read(&sm, sizeof(shortmsgrec));
      while (sm.tosys == 0 && sm.touser == 0 && nNewMsgPos > 0) {
        --nNewMsgPos;
        file.Seek(nNewMsgPos * sizeof(shortmsgrec), File::Whence::begin);
        file.Read(&sm, sizeof(shortmsgrec));
      }
      if (sm.tosys != 0 || sm.touser != 0) {
        nNewMsgPos++;
      }
    } else {
      nNewMsgPos = 0;
    }
    sm.tosys = static_cast<uint16_t>(0);  // 0 means local
    sm.touser = static_cast<uint16_t>(nUserNum);
    strncpy(sm.message, messageText, 80);
    sm.message[80] = '\0';
    file.Seek(nNewMsgPos * sizeof(shortmsgrec), File::Whence::begin);
    file.Write(&sm, sizeof(shortmsgrec));
    file.Close();
    user.SetStatusFlag(User::SMW);
    a()->users()->WriteUser(&user, nUserNum);
  }
}

static void SendRemoteShortMessage(int nUserNum, int nSystemNum, const char *messageText) {
  net_header_rec nh;
  nh.tosys = static_cast<uint16_t>(nSystemNum);
  nh.touser = static_cast<uint16_t>(nUserNum);
  nh.fromsys = a()->current_net().sysnum;
  nh.fromuser = static_cast<uint16_t>(a()->usernum);
  nh.main_type = main_type_ssm;
  nh.minor_type = 0;
  nh.list_len = 0;
  nh.daten = static_cast<uint32_t>(time(nullptr));
  string msg = messageText;
  if (msg.size() > 79) {
    msg.resize(79);
  }
  nh.length = msg.size();
  nh.method = 0;
  const string packet_filename = StringPrintf("%sp0%s", 
    a()->network_directory().c_str(),
    a()->network_extension().c_str());
  File file(packet_filename);
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(0L, File::Whence::end);
  file.Write(&nh, sizeof(net_header_rec));
  file.Write(msg.c_str(), nh.length);
  file.Close();
}

ssm::~ssm() {
  if (un_ == 65535 || un_== 0 || sn_== 32767) {
    return;
  }

  const auto& s = stream_.str();

  if (sn_ == 0) {
    SendLocalShortMessage(un_, s.c_str());
  } else {
    SendRemoteShortMessage(un_, sn_, s.c_str());
  }
}

