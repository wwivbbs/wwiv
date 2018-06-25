/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/vars.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

using std::string;;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

/*
 * Handles reading short messages. This is also where PackScan file requests
 * plug in, if such are used.
 */
void rsm(int nUserNum, User *pUser, bool bAskToSaveMsgs) {
  if (!pUser->HasShortMessage()) {
    return;
  }
  DataFile<shortmsgrec> file(a()->config()->datadir(), SMW_DAT, File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (!file) {
    return;
  }
  bool bShownAnyMessage = false;
  int bShownAllMessages = true;
  int number_of_records = file.number_of_records();
  shortmsgrec sm{};
  for (int cur = 0; cur < number_of_records; cur++) {
    file.Read(cur, &sm);
    if (sm.touser == nUserNum && sm.tosys == 0) {
      bout << "|#9" << sm.message << "\r\n";
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
        memset(&sm.message, 0, sizeof(sm.message));
        file.Write(cur, &sm);
      } else {
        bShownAllMessages = false;
      }
    }
  }
  file.Close();
  smwcheck = true;
  if (bShownAnyMessage) {
    bout.nl();
  }
  if (bShownAllMessages) {
    pUser->ClearStatusFlag(User::SMW);
  }
}

static void SendLocalShortMessage(unsigned int nUserNum, const char *messageText) {
  User user;
  a()->users()->readuser(&user, nUserNum);
  if (!user.IsUserDeleted()) {
    File file(a()->config()->datadir(), SMW_DAT);
    if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return;
    }
    int nTotalMsgsInFile = static_cast<int>(file.length() / sizeof(shortmsgrec));
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
    to_char_array(sm.message, messageText);
    sm.message[80] = '\0';
    file.Seek(nNewMsgPos * sizeof(shortmsgrec), File::Whence::begin);
    file.Write(&sm, sizeof(shortmsgrec));
    file.Close();
    user.SetStatusFlag(User::SMW);
    a()->users()->writeuser(&user, nUserNum);
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
  nh.daten = daten_t_now();
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
  if (un_ == 65535 || un_ == 0 || sn_ == INTERNET_FAKE_OUTBOUND_NODE) {
    return;
  }

  const auto& s = stream_.str();

  if (sn_ == 0) {
    SendLocalShortMessage(un_, s.c_str());
  } else {
    SendRemoteShortMessage(un_, sn_, s.c_str());
  }
}

