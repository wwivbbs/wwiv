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
#include "bbsovl1.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/conf.h"
#include "bbs/email.h"
#include "bbs/external_edit.h"
#include "bbs/instmsg.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/com.h"
#include "common/input.h"
#include "common/message_editor_data.h"
#include "common/output.h"
#include "common/quote.h"
#include "common/workspace.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"

#include <string>

using namespace std::chrono;
using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

//////////////////////////////////////////////////////////////////////////////
// Implementation

/**
 * Displays a horizontal bar of nSize characters in nColor
 * @param width Length of the horizontal bar to display
 * @param color Color of the horizontal bar to display
 */
void DisplayHorizontalBar(int width, int color) {
  const auto ch = (okansi()) ? '\xC4' : '-';
  bout.Color(color);
  bout.outstr(std::string(width, ch));
  bout.nl();
}

/**
 * Displays some basic user statistics for the current user.
 */
void YourInfo() {
  if (bout.printfile("yourinfo")) {
    // We displayed the new file based yourinfo, exit here.
    return;
  }
  // Legacy yourinfo used.
  LOG(WARNING) << "Legacy yourinfo used, please add yourinfo.msg to gfiles directory.";
  bout.cls();
  bout.litebar("Your User Information");
  bout.nl();
  bout.print("|#9Your name      : |#2{}\r\n", a()->user()->name_and_number());
  bout.print("|#9Phone number   : |#2{}\r\n", a()->user()->voice_phone());
  if (a()->user()->email_waiting() > 0) {
    bout.print("|#9Mail Waiting   : |#2{}\r\n", a()->user()->email_waiting());
  }
  bout.print("|#9Security Level : |#2{}\r\n", a()->user()->sl());
  if (a()->sess().effective_sl() != a()->user()->sl()) {
    bout.print("|#1 (temporarily |#2{}|#1)", a()->sess().effective_sl());
  }
  bout.nl();
  bout.print("|#9Transfer SL    : |#2{}\r\n", a()->user()->dsl());
  bout.print("|#9Date Last On   : |#2{}\r\n", a()->user()->laston());
  bout.print("|#9Times on       : |#2{}\r\n", a()->user()->logons());
  bout.print("|#9On today       : |#2{}\r\n", a()->user()->ontoday());
  bout.print("|#9Messages posted: |#2{}\r\n", a()->user()->messages_posted());
  const auto total_mail_sent =
      a()->user()->email_sent() + a()->user()->feedback_sent() + a()->user()->email_net();
  bout.print("|#9E-mail sent    : |#2{}\r\n", total_mail_sent);
  const auto minutes_used =
      duration_cast<minutes>(a()->user()->timeon()  + a()->sess().duration_used_this_session()).count();
  bout.print("|#9Time spent on  : |#2{} |#9Minutes\r\n", minutes_used);

  // Transfer Area Statistics
  bout.print("|#9Uploads        : |#2{}|#9k in|#2 {} |#9files\r\n", a()->user()->uk(),
             a()->user()->uploaded());
  bout.print("|#9Downloads      : |#2{}|#9k in|#2 {} |#9files\r\n", a()->user()->dk(),
             a()->user()->downloaded());
  bout.print("|#9Transfer Ratio : |#2{}\r\n", a()->user()->ratio());
  bout.nl();
  bout.pausescr();
}

/**
 * Gets the maximum number of lines allowed for a post by the current user.
 * @return The maximum message length in lines
 */
int GetMaxMessageLinesAllowed() {
  if (so()) {
    return 120;
  }
  return cs() ? 100 : 80;
}

/**
 * Allows user to upload a post.
 */
void upload_post() {
  File file(FilePath(a()->sess().dirs().temp_directory(), INPUT_MSG));
  const auto max_bytes = 250 * static_cast<File::size_type>(GetMaxMessageLinesAllowed());

  bout.print("\r\nYou may now upload a message, max bytes: {}\r\n\r\n", max_bytes);
  auto i = 0;
  receive_file(file.full_pathname(), &i, INPUT_MSG, -1);
  if (file.Open(File::modeReadOnly | File::modeBinary)) {
    const auto file_size = file.length();
    if (file_size > max_bytes) {
      bout.outstr("\r\n|#6Sorry, your message is too long.  Not saved.\r\n\n");
      file.Close();
      File::Remove(file.path());
    } else {
      file.Close();
      use_workspace = true;
      bout.outstr("\r\n|#7* |#1Message uploaded.  The next post or email will contain that text.\r\n\n");
    }
  } else {
    bout.outstr("\r\n|#3Nothing saved.\r\n\n");
  }
}

/**
 * High-level function for sending email.
 */
void send_email() {
  write_inst(INST_LOC_EMAIL, 0, INST_FLAGS_NONE);
  a()->sess().clear_irt();

  bout.outstr("\r\n\n|#9Enter user name or number:\r\n:");
  const auto username = fixup_user_entered_email(bin.input_text(75));

  auto [user_number, system_number] = parse_email_info(username);
  clear_quotes(a()->sess());
  if (user_number || system_number) {
    email("", user_number, system_number, false, 0);
  }
}

/**
 * High-level function for selecting conference type to edit.
 */
void edit_confs() {
  if (!ValidateSysopPassword()) {
    return;
  }

  while (!a()->sess().hangup()) {
    bout.cls();
    bout.litebar("Conference Editor");
    bout.outstr("|#5Edit Which Conferences:\r\n\n");
    bout.outstr("|#21|#9)|#1 Subs\r\n");
    bout.outstr("|#22|#9)|#1 Dirs\r\n");
    bout.outstr("\r\n|#9Select [|#21|#9,|#22|#9,|#2Q|#9]: ");
    const auto ch = onek("Q12", true);
    switch (ch) {
    case '1':
      conf_edit(a()->all_confs().subs_conf());
      break;
    case '2':
      conf_edit(a()->all_confs().dirs_conf());
      break;
    case 'Q':
      return;
    }
    a()->CheckForHangup();
  }
}

/**
 * Sends Feedback to the SysOp.  If  bNewUserFeedback is true then this is
 * new user feedback, otherwise it is "normal" feedback.
 * The user can choose to email anyone listed.
 * Users with a()->sess().user_num() < 10 who have sysop privs will be listed, so
 * this user can select which sysop to leave feedback to.
 */
void feedback(bool bNewUserFeedback) {
  int i;
  char onek_str[20], ch;

  clear_quotes(a()->sess());

  if (bNewUserFeedback) {
    const auto title =
        fmt::format("|#1Validation Feedback (|#6{}|#2 slots left|#1)",
                     a()->config()->max_users() - a()->status_manager()->user_count());
    // We disable the fsed here since it was hanging on some systems.  Not sure why
    // but it's better to be safe -- Rushfan 2003-12-04
    email(title, 1, 0, true, 0, false);
    return;
  }
  if (a()->user()->guest_user()) {
    a()->status_manager()->reload_status();
    email("Guest Account Feedback", 1, 0, true, 0, true);
    return;
  }
  const int num_user_records = a()->users()->num_user_records();
  int i1 = 0;

  for (i = 2; i < 10 && i < num_user_records; i++) {
    if (const auto user = a()->users()->readuser(i, UserManager::mask::non_deleted); 
      user->sl() == 255 || a()->config()->sl(user->sl()).ability & ability_cosysop) {
      i1++;
    }
  }

  if (!i1) {
    i = 1;
  } else {
    onek_str[0] = '\0';
    i1 = 0;
    bout.nl();
    for (i = 1; (i < 10 && i < num_user_records); i++) {
      User user;
      a()->users()->readuser(&user, i);
      if ((user.sl() == 255 || a()->config()->sl(user.sl()).ability & ability_cosysop) &&
          !user.deleted()) {
        bout.print("|#2{}|#7)|#1 {}\r\n", i, a()->names()->UserName(i));
        onek_str[i1++] = static_cast<char>('0' + i);
      }
    }
    const auto key_quit = bout.lang().value("KEY_QUIT", "Q").front();
    onek_str[i1++] = key_quit;
    onek_str[i1] = '\0';
    bout.nl();
    bout.print("|#1Feedback to ({}): ", onek_str);
    ch = onek(onek_str, true);
    if (ch == key_quit) {
      return;
    }
    bout.nl();
    i = ch - '0';
  }

  email("|#1Feedback", static_cast<uint16_t>(i), 0, false, 0, true);
}

/**
 * Allows editing of ASCII text files. Must have an external editor defined,
 * and toggled for use in defaults.
 */
void text_edit() {
  bout.nl();
  bout.outstr("|#9Enter Filename: ");
  const auto filename = bin.input_filename(12);
  if (filename.find(".log") != std::string::npos || !okfn(filename)) {
    return;
  }
  sysoplog(fmt::format("@ Edited: {}", filename));
  if (ok_external_fsed()) {
    fsed_text_edit(filename, a()->config()->gfilesdir(), 500, MSGED_FLAG_NO_TAGLINE);
  }
}
