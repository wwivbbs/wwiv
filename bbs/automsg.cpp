/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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

#include "instmsg.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/email.h"
#include "bbs/sysoplog.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "common/quote.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/status.h"

#include <memory>
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static bool is_automessage_locked() {
  const auto lock_file = FilePath(a()->config()->gfilesdir(), LOCKAUTO_MSG);
  return File::Exists(lock_file);
}

/**
 * Reads the auto message
 */
void read_automessage() {
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  bout.nl();
  const auto current_status = a()->status_manager()->get_status();
  const auto anonymous = current_status->automessage_anon();

  TextFile autoMessageFile(FilePath(a()->config()->gfilesdir(), AUTO_MSG), "rt");
  std::string line;
  if (!autoMessageFile.IsOpen() || !autoMessageFile.ReadLine(&line)) {
    bout.outstr("|#3No auto-message.\r\n");
    bout.nl();
    return;
  }

  StringTrimEnd(&line);
  auto authorName = line;
  if (anonymous) {
    if (a()->config()->sl(a()->sess().effective_sl()).ability & ability_read_post_anony) {
      std::stringstream ss;
      ss << "<<< " << line << " >>>";
      authorName = ss.str();
    } else {
      authorName = ">UNKNOWN<";
    }
  }
  bout.print("\r\n|#9Auto message by: |#2{}|#0\r\n\n", authorName);

  auto line_number = 0;
  while (autoMessageFile.ReadLine(&line) && line_number++ < 10) {
    StringTrim(&line);
    bout.ansic(9);
    bout.pl(line);
  }
  bout.nl();
}

/**
 * Writes the auto message
 */
void write_automessage() {
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  if (is_automessage_locked()) {
    bout.outstr("\r\n|#3Message is locked.\r\n\n");
    bout.pausescr();
    return;
  }
  if (a()->user()->restrict_automessage()) {
    bout.outstr("\r\n|#6Not allowed to write to automessage (RESTRICT).\r\n\n");
    bout.pausescr();
    return;
  }
  std::vector<std::string> lines;
  std::string rollOver;

  bout.outstr("\r\n|#9Enter auto-message. Max 5 lines. Colors allowed:|#0\r\n\n");
  for (int i = 0; i < 5; i++) {
    bout.print("|#7{}:|#0", i + 1);
    std::string line;
    inli(&line, &rollOver, 70);
    StringTrimEnd(&line);
    lines.push_back(line);
  }
  bout.nl();
  bool bAnonStatus = false;
  if (a()->config()->sl(a()->sess().effective_sl()).ability & ability_post_anony) {
    bout.outstr("|#9Anonymous? ");
    bAnonStatus = bin.yesno();
  }

  bout.outstr("|#9Is this OK? ");
  if (bin.yesno()) {
    a()->status_manager()->Run([bAnonStatus](Status& s) {
      s.automessage_anon(bAnonStatus);
      s.automessage_usernum(a()->sess().user_num());
    });

    TextFile file(FilePath(a()->config()->gfilesdir(), AUTO_MSG), "wt");
    const auto authorName = a()->user()->name_and_number();
    file.WriteLine(authorName);
    sysoplog("Changed Auto-message");
    for (const auto& line : lines) {
      file.WriteLine(line);
      sysoplog(true, line);
    }
    bout.outstr("\r\n|#5Auto-message saved.\r\n\n");
    file.Close();
  }
}

static char ShowAMsgMenuAndGetInput() {
  auto bCanWrite = false;
  if (!a()->user()->restrict_automessage()) {
    bCanWrite = a()->user()->sl() > 20;
  }

  if (cs()) {
    bout.outstr("|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite, (|#2L|#9)ock, "
              "(|#2D|#9)el, (|#2U|#9)nlock : ");
    return onek("QRWALDU", true);
  }
  if (bCanWrite) {
    bout.outstr("|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite : ");
    return onek("QRWA", true);
  }
  bout.outstr("|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply : ");
  return onek("QRA", true);
}

void email_automessage_author() {
  clear_quotes(a()->sess());
  const auto status = a()->status_manager()->get_status();
  if (status->automessage_usernum() > 0) {
    email("Re: AutoMessage", status->automessage_usernum(), 0,
          false, status->automessage_anon() ? anony_sender : 0);
  }
}

void delete_automessage() {
  if (!cs()) {
    bout.outstr("|#6Unauthorized.\r\n");
    return;
  }
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  bout.outstr("\r\n|#3Delete Auto-message, Are you sure? ");
  if (bin.yesno()) {
    const auto file = FilePath(a()->config()->gfilesdir(), AUTO_MSG);
    File::Remove(file);
  }
  bout.nl(2);
}

void lock_automessage() {
  if (!cs()) {
    bout.outstr("|#6Unauthorized.\r\n");
    return;
  }
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  const auto lock_file = FilePath(a()->config()->gfilesdir(), LOCKAUTO_MSG);
  if (File::Exists(lock_file)) {
    bout.outstr("\r\n|#3Message is already locked.\r\n\n");
    return;
  }
  bout.outstr("|#9Do you want to lock the Auto-message? ");
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

void unlock_automessage() {
  if (!cs()) {
    bout.outstr("|#6Unauthorized.\r\n");
    return;
  }
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  const auto lock_file = FilePath(a()->config()->gfilesdir(), LOCKAUTO_MSG);
  if (!File::Exists(lock_file)) {
    bout.outstr("Message not locked.\r\n");
  } else {
    bout.outstr("|#5Unlock message? ");
    if (bin.yesno()) {
      File::Remove(lock_file);
    }
  }
}
/**
 * Main Auto Message menu.  Displays the auto message then queries for input.
 */
void do_legacy_automessage() {
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  // initially show the auto message
  read_automessage();

  auto done = false;
  do {
    bout.nl();
    const auto cmdKey = ShowAMsgMenuAndGetInput();
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
      email_automessage_author();
    } break;
    case 'D':
      delete_automessage();
      break;
    case 'L':
      lock_automessage();
      break;
    case 'U':
      unlock_automessage();
      break;
    default: break;
    }
  } while (!done);
}
