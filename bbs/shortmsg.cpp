/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/datetime.h"
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
  DataFile<shortmsgrec> file(FilePath(a()->config()->datadir(), SMW_DAT),
                             File::modeReadWrite | File::modeBinary | File::modeCreateFile);
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
  a()->received_short_message_ = true;
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
    File file(FilePath(a()->config()->datadir(), SMW_DAT));
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

static void SendRemoteShortMessage(uint16_t user_num, uint16_t system_num, const std::string text,
                                   const net_networks_rec& net) {
  net_header_rec nh;
  nh.tosys = system_num;
  nh.touser = user_num;
  nh.fromsys = net.sysnum;
  nh.fromuser = static_cast<uint16_t>(a()->usernum);
  nh.main_type = main_type_ssm;
  nh.minor_type = 0;
  nh.list_len = 0;
  nh.daten = daten_t_now();
  string msg = text;
  if (msg.size() > 79) {
    msg.resize(79);
  }
  nh.length = msg.size();
  nh.method = 0;
  const auto packet_filename = StrCat(net.dir, "p0", a()->network_extension());
  File file(packet_filename);
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(0L, File::Whence::end);
  file.Write(&nh, sizeof(net_header_rec));
  file.Write(msg.c_str(), msg.size());
  file.Close();
}

ssm::~ssm() {
  if (un_ == 65535 || un_ == 0 || sn_ == INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
    return;
  }

  const auto& s = stream_.str();

  if (sn_ == 0) {
    SendLocalShortMessage(un_, s.c_str());
  } else {
    if (net_ != nullptr) {
      SendRemoteShortMessage(un_, sn_, s, *net_);
    } else {
      LOG(ERROR) << "Tried to send remote SSM when net_ was null: " << s;
    }
  }
}

