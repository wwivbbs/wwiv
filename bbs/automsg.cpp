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
#include "bbs/automsg.h"

#include <memory>
#include <string>
#include <vector>

#include "bbs/wwiv.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/textfile.h"

using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using namespace wwiv::strings;

char ShowAMsgMenuAndGetInput(const string& autoMessageLockFileName);
void write_automessage();

/**
 * Reads the auto message
 */
void read_automessage() {
  bout.nl();
  unique_ptr<WStatus> status(application()->GetStatusManager()->GetStatus());
  bool bAutoMessageAnonymous = status->IsAutoMessageAnonymous();

  TextFile autoMessageFile(syscfg.gfilesdir, AUTO_MSG, "rt");
  string line;
  if (!autoMessageFile.IsOpen() || !autoMessageFile.ReadLine(&line)) {
    bout << "|#3No auto-message.\r\n";
    bout.nl();
    return;
  }

  StringTrimEnd(&line);
  string authorName = line;
  if (bAutoMessageAnonymous) {
    if (getslrec(session()->GetEffectiveSl()).ability & ability_read_post_anony) {
      stringstream ss;
      ss << "<<< " << line << " >>>";
      authorName = ss.str();
    } else {
      authorName = ">UNKNOWN<";
    }
  }
  bout << "\r\n|#9Auto message by: |#2" << authorName << "|#0\r\n\n";

  int nLineNumber = 0;
  while (autoMessageFile.ReadLine(&line) && nLineNumber++ < 10) {
    StringTrim(&line);
    bout.Color(9);
    bout << "|#9" << line << wwiv::endl;
  }
  bout.nl();
}


/**
 * Writes the auto message
 */
void write_automessage() {
  vector<string> lines;
  string rollOver;

  bout << "\r\n|#9Enter auto-message. Max 5 lines. Colors allowed:|#0\r\n\n";
  for (int i = 0; i < 5; i++) {
    bout << "|#7" << i + 1 << ":|#0";
    string line;
    inli(&line, &rollOver, 70);
    StringTrimEnd(&line);
    lines.push_back(line);
  }
  bout.nl();
  bool bAnonStatus = false;
  if (getslrec(session()->GetEffectiveSl()).ability & ability_post_anony) {
    bout << "|#9Anonymous? ";
    bAnonStatus = yesno();
  }

  bout << "|#9Is this OK? ";
  if (yesno()) {
    WStatus *pStatus = application()->GetStatusManager()->BeginTransaction();
    pStatus->SetAutoMessageAnonymous(bAnonStatus);
    pStatus->SetAutoMessageAuthorUserNumber(session()->usernum);
    application()->GetStatusManager()->CommitTransaction(pStatus);

    TextFile file(syscfg.gfilesdir, AUTO_MSG, "wt");
    string authorName = session()->user()->GetUserNameAndNumber(session()->usernum);
    file.WriteFormatted("%s\r\n", authorName.c_str());
    sysoplog("Changed Auto-message");
    for (const auto& line : lines) {
      file.Write(line);
      file.Write("\r\n");
      sysoplog(line, true);
    }
    bout << "\r\n|#5Auto-message saved.\r\n\n";
    file.Close();
  }
}


char ShowAMsgMenuAndGetInput(const string& autoMessageLockFileName) {
  bool bCanWrite = false;
  if (!session()->user()->IsRestrictionAutomessage() && !File::Exists(autoMessageLockFileName)) {
    bCanWrite = (getslrec(session()->GetEffectiveSl()).posts) ? true : false;
  }

  char cmdKey = 0;
  if (cs()) {
    bout <<
                       "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite, (|#2L|#9)ock, (|#2D|#9)el, (|#2U|#9)nlock : ";
    cmdKey = onek("QRWALDU", true);
  } else if (bCanWrite) {
    bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite : ";
    cmdKey = onek("QRWA", true);
  } else {
    bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply : ";
    cmdKey = onek("QRA", true);
  }
  return cmdKey;
}

/**
 * Main Automessage menu.  Displays the auto message then queries for input.
 */
void do_automessage() {
  stringstream lockFileStream;
  lockFileStream << syscfg.gfilesdir << LOCKAUTO_MSG;
  string automessageLockFile = lockFileStream.str();

  stringstream autoMessageStream;
  autoMessageStream << syscfg.gfilesdir << AUTO_MSG;
  string autoMessageFile = autoMessageStream.str();

  // initally show the auto message
  read_automessage();

  bool done = false;
  do {
    bout.nl();
    char cmdKey = ShowAMsgMenuAndGetInput(automessageLockFile);
    switch (cmdKey) {
    case 'Q':
      done = true;
      break;
    case 'R':
      read_automessage();
      break;
    case 'W':
      write_automessage();
      break;
    case 'A': {
      grab_quotes(nullptr, nullptr);
      unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
      if (pStatus->GetAutoMessageAuthorUserNumber() > 0) {
        strcpy(irt, "Re: AutoMessage");
        email(pStatus->GetAutoMessageAuthorUserNumber(), 0, false, pStatus->IsAutoMessageAnonymous() ? anony_sender : 0);
      }
    }
    break;
    case 'D':
      bout << "\r\n|#3Delete Auto-message, Are you sure? ";
      if (yesno()) {
        File::Remove(autoMessageFile);
      }
      bout.nl(2);
      break;
    case 'L':
      if (File::Exists(automessageLockFile)) {
        bout << "\r\n|#3Message is already locked.\r\n\n";
      } else {
        bout <<  "|#9Do you want to lock the Auto-message? ";
        if (yesno()) {
          /////////////////////////////////////////////////////////
          // This makes a file in your GFILES dir 1 bytes long,
          // to tell the board if it is locked or not. It consists
          // of a space.
          //
          TextFile lockFile(automessageLockFile, "w+t");
          lockFile.WriteChar(' ');
          lockFile.Close();
        }
      }
      break;
    case 'U':
      if (!File::Exists(automessageLockFile)) {
        bout << "Message not locked.\r\n";
      } else {
        bout << "|#5Unlock message? ";
        if (yesno()) {
          File::Remove(automessageLockFile);
        }
      }
      break;
    }
  } while (!done && !hangup);
}
