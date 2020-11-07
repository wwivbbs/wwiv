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
#include "bbs/automsg.h"

#include <memory>
#include <string>
#include <vector>
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "common/com.h"
#include "bbs/email.h"
#include "bbs/sysoplog.h"
#include "common/quote.h"
#include "common/input.h"
#include "common/output.h"
#include "bbs/application.h"
#include "sdk/status.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"

using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

/**
 * Reads the auto message
 */
void read_automessage() {
  bout.nl();
  const auto current_status = a()->status_manager()->GetStatus();
  const bool anonymous = current_status->IsAutoMessageAnonymous();

  TextFile autoMessageFile(FilePath(a()->config()->gfilesdir(), AUTO_MSG), "rt");
  string line;
  if (!autoMessageFile.IsOpen() || !autoMessageFile.ReadLine(&line)) {
    bout << "|#3No auto-message.\r\n";
    bout.nl();
    return;
  }

  StringTrimEnd(&line);
  auto authorName = line;
  if (anonymous) {
    if (a()->effective_slrec().ability & ability_read_post_anony) {
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
static void write_automessage() {
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
  if (a()->effective_slrec().ability & ability_post_anony) {
    bout << "|#9Anonymous? ";
    bAnonStatus = bin.yesno();
  }

  bout << "|#9Is this OK? ";
  if (bin.yesno()) {
    a()->status_manager()->Run([bAnonStatus](WStatus& s) {
      s.SetAutoMessageAnonymous(bAnonStatus);
      s.SetAutoMessageAuthorUserNumber(a()->sess().user_num());
    });

    TextFile file(FilePath(a()->config()->gfilesdir(), AUTO_MSG), "wt");
    const string authorName = a()->names()->UserName(a()->sess().user_num());
    file.WriteLine(authorName);
    sysoplog() << "Changed Auto-message";
    for (const auto& line : lines) {
      file.WriteLine(line);
      sysoplog(true) << line;
    }
    bout << "\r\n|#5Auto-message saved.\r\n\n";
    file.Close();
  }
}


static char ShowAMsgMenuAndGetInput(const std::filesystem::path& lock_file) {
  auto bCanWrite = false;
  if (!a()->user()->IsRestrictionAutomessage() && !File::Exists(lock_file)) {
    bCanWrite = a()->effective_slrec().posts > 0;
  }

  if (cs()) {
    bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite, (|#2L|#9)ock, (|#2D|#9)el, (|#2U|#9)nlock : ";
    return onek("QRWALDU", true);
  }
  if (bCanWrite) {
    bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite : ";
    return onek("QRWA", true);
  }
  bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply : ";
  return onek("QRA", true);
}

/**
 * Main Auto Message menu.  Displays the auto message then queries for input.
 */
void do_automessage() {
  const auto lock_file = FilePath(a()->config()->gfilesdir(), LOCKAUTO_MSG);
  const auto file = FilePath(a()->config()->gfilesdir(), AUTO_MSG);

  // initially show the auto message
  read_automessage();

  auto done = false;
  do {
    bout.nl();
    const auto cmdKey = ShowAMsgMenuAndGetInput(lock_file);
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
      clear_quotes(a()->sess());
      const auto status = a()->status_manager()->GetStatus();
      if (status->GetAutoMessageAuthorUserNumber() > 0) {
        email("Re: AutoMessage", static_cast<uint16_t>(status->GetAutoMessageAuthorUserNumber()), 0,
              false, status->IsAutoMessageAnonymous() ? anony_sender : 0);
      }
    }
    break;
    case 'D':
      bout << "\r\n|#3Delete Auto-message, Are you sure? ";
      if (bin.yesno()) {
        File::Remove(file);
      }
      bout.nl(2);
      break;
    case 'L':
      if (File::Exists(lock_file)) {
        bout << "\r\n|#3Message is already locked.\r\n\n";
      } else {
        bout <<  "|#9Do you want to lock the Auto-message? ";
        if (bin.yesno()) {
          /////////////////////////////////////////////////////////
          // This makes a file in your GFILES dir 1 bytes long,
          // to tell the board if it is locked or not. It consists
          // of a space.
          //
          TextFile lockFile(lock_file, "w+t");
          lockFile.WriteChar(' ');
          lockFile.Close();
        }
      }
      break;
    case 'U':
      if (!File::Exists(lock_file)) {
        bout << "Message not locked.\r\n";
      } else {
        bout << "|#5Unlock message? ";
        if (bin.yesno()) {
          File::Remove(lock_file);
        }
      }
      break;
    }
  } while (!done);
}
