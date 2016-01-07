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
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbsovl1.h"

#include <sstream>
#include <string>

#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/email.h"
//#include "bbs/fcns.h"
#include "bbs/external_edit.h"
#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/quote.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "bbs/wstatus.h"
#include "bbs/xfer.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::sdk;
using namespace wwiv::strings;

//////////////////////////////////////////////////////////////////////////////
// Implementation

extern char str_quit[];

/**
 * Displays a horizontal bar of nSize characters in nColor
 * @param nSize Length of the horizontal bar to display
 * @param nColor Color of the horizontal bar to display
 */
void DisplayHorizontalBar(int width, int color) {
  char ch = (okansi()) ? '\xC4' : '-';
  bout.Color(color);
  bout << string(width, ch);
  bout.nl();
}

/**
 * Displays some basic user statistics for the current user.
 */
void YourInfo() {
  bout.cls();
  if (okansi()) {
    bout.litebar("Your User Information");
  } else {
    bout << "|#5Your User Information:\r\n";
  }
  bout.nl();
  bout << "|#9Your name      : |#2" 
       << session()->names()->UserName(session()->usernum) << wwiv::endl;
  bout << "|#9Phone number   : |#2" << session()->user()->GetVoicePhoneNumber() << wwiv::endl;
  if (session()->user()->GetNumMailWaiting() > 0) {
    bout << "|#9Mail Waiting   : |#2" << session()->user()->GetNumMailWaiting() << wwiv::endl;
  }
  bout << "|#9Security Level : |#2" << session()->user()->GetSl() << wwiv::endl;
  if (session()->GetEffectiveSl() != session()->user()->GetSl()) {
    bout << "|#1 (temporarily |#2" << session()->GetEffectiveSl() << "|#1)";
  }
  bout.nl();
  bout << "|#9Transfer SL    : |#2" << session()->user()->GetDsl() << wwiv::endl;
  bout << "|#9Date Last On   : |#2" << session()->user()->GetLastOn() << wwiv::endl;
  bout << "|#9Times on       : |#2" << session()->user()->GetNumLogons() << wwiv::endl;
  bout << "|#9On today       : |#2" << session()->user()->GetTimesOnToday() << wwiv::endl;
  bout << "|#9Messages posted: |#2" << session()->user()->GetNumMessagesPosted() << wwiv::endl;
  bout << "|#9E-mail sent    : |#2" << (session()->user()->GetNumEmailSent() +
                     session()->user()->GetNumFeedbackSent() + session()->user()->GetNumNetEmailSent()) <<
                     wwiv::endl;
  bout << "|#9Time spent on  : |#2" << static_cast<long>((session()->user()->GetTimeOn() +
                     timer() - timeon) / SECONDS_PER_MINUTE_FLOAT) << " |#9Minutes" << wwiv::endl;

  // Transfer Area Statistics
  bout << "|#9Uploads        : |#2" << session()->user()->GetUploadK() << "|#9k in|#2 " <<
                     session()->user()->GetFilesUploaded() << " |#9files" << wwiv::endl;
  bout << "|#9Downloads      : |#2" << session()->user()->GetDownloadK() << "|#9k in|#2 " <<
                     session()->user()->GetFilesDownloaded() << " |#9files" << wwiv::endl;
  bout << "|#9Transfer Ratio : |#2" << ratio() << wwiv::endl;
  bout.nl();
  pausescr();
}


/**
 * Gets the maximum number of lines allowed for a post by the current user.
 * @return The maximum message length in lines
 */
int GetMaxMessageLinesAllowed() {
  if (so()) {
    return 120;
  }
  return (cs()) ? 100 : 80;
}


/**
 * Allows user to upload a post.
 */
void upload_post() {
  File file(syscfgovr.tempdir, INPUT_MSG);
  long lMaxBytes = 250 * static_cast<long>(GetMaxMessageLinesAllowed());

  bout << "\r\nYou may now upload a message, max bytes: " << lMaxBytes << wwiv::endl << wwiv::endl;
  int i = 0;
  receive_file(file.full_pathname().c_str(), &i, INPUT_MSG, -1);
  if (file.Open(File::modeReadOnly | File::modeBinary)) {
    long lFileSize = file.GetLength();
    if (lFileSize > lMaxBytes) {
      bout << "\r\n|#6Sorry, your message is too long.  Not saved.\r\n\n";
      file.Close();
      file.Delete();
    } else {
      file.Close();
      use_workspace = true;
      bout << "\r\n|#7* |#1Message uploaded.  The next post or email will contain that text.\r\n\n";
    }
  } else {
    bout << "\r\n|#3Nothing saved.\r\n\n";
  }
}


/**
 * High-level function for sending email.
 */
void send_email() {
  write_inst(INST_LOC_EMAIL, 0, INST_FLAGS_NONE);
  bout << "\r\n\n|#9Enter user name or number:\r\n:";
  string username = input(75, true);
  irt[0] = '\0';
  irt_name[0] = '\0';
  string::size_type atpos = username.find_first_of("@");
  if (atpos != string::npos && atpos != username.length() && isalpha(username[atpos + 1])) {
    if (username.find("@32767") == string::npos) {
      StringLowerCase(&username);
      username += " @32767";
    }
  }

  int system_number, user_number;
  parse_email_info(username, &user_number, &system_number);
  grab_quotes(nullptr, nullptr);
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

  while (!hangup) {
    bout << "\r\n\n|#5Edit Which Conferences:\r\n\n";
    bout << "|#21|#9)|#1 Subs\r\n";
    bout << "|#22|#9)|#1 Dirs\r\n";
    bout << "\r\n|#9Select [|#21|#9,|#22|#9,|#2Q|#9]: ";
    char ch = onek("Q12", true);
    switch (ch) {
    case '1':
      conf_edit(ConferenceType::CONF_SUBS);
      break;
    case '2':
      conf_edit(ConferenceType::CONF_DIRS);
      break;
    case 'Q':
      return;
    }
    CheckForHangup();
  }
}

/**
 * Sends Feedback to the SysOp.  If  bNewUserFeedback is true then this is
 * newuser feedback, otherwise it is "normal" feedback.
 * The user can choose to email anyone listed.
 * Users with session()->usernum < 10 who have sysop privs will be listed, so
 * this user can select which sysop to leave feedback to.
 */
void feedback(bool bNewUserFeedback) {
  int i;
  char onek_str[20], ch;

  irt_name[0] = '\0';
  grab_quotes(nullptr, nullptr);

  if (bNewUserFeedback) {
    sprintf(irt, "|#1Validation Feedback (|#6%d|#2 slots left|#1)",
            syscfg.maxusers - session()->status_manager()->GetUserCount());
    // We disable the fsed here since it was hanging on some systems.  Not sure why
    // but it's better to be safe -- Rushfan 2003-12-04
    email(irt, 1, 0, true, 0, false);
    return;
  }
  if (guest_user) {
    session()->status_manager()->RefreshStatusCache();
    strcpy(irt, "Guest Account Feedback");
    email(irt, 1, 0, true, 0, true);
    return;
  }
  strcpy(irt, "|#1Feedback");
  int nNumUserRecords = session()->users()->GetNumberOfUserRecords();
  int i1 = 0;

  for (i = 2; i < 10 && i < nNumUserRecords; i++) {
    User user;
    session()->users()->ReadUser(&user, i);
    if ((user.GetSl() == 255 || (getslrec(user.GetSl()).ability & ability_cosysop)) &&
        !user.IsUserDeleted()) {
      i1++;
    }
  }

  if (!i1) {
    i = 1;
  } else {
    onek_str[0] = '\0';
    i1 = 0;
    bout.nl();
    for (i = 1; (i < 10 && i < nNumUserRecords); i++) {
      User user;
      session()->users()->ReadUser(&user, i);
      if ((user.GetSl() == 255 || (getslrec(user.GetSl()).ability & ability_cosysop)) &&
          !user.IsUserDeleted()) {
        bout << "|#2" << i << "|#7)|#1 " << session()->names()->UserName(i) << wwiv::endl;
        onek_str[i1++] = static_cast<char>('0' + i);
      }
    }
    onek_str[i1++] = *str_quit;
    onek_str[i1] = '\0';
    bout.nl();
    bout << "|#1Feedback to (" << onek_str << "): ";
    ch = onek(onek_str, true);
    if (ch == *str_quit) {
      return;
    }
    bout.nl();
    i = ch - '0';
  }
  email(irt, static_cast<uint16_t>(i), 0, false, 0, true);
}

/**
 * Allows editing of ASCII textfiles. Must have an external editor defined,
 * and toggled for use in defaults.
 */
void text_edit() {
  bout.nl();
  bout << "|#9Enter Filename: ";
  string filename = input(12, true);
  if (filename.find(".log") != string::npos || !okfn(filename)) {
    return;
  }
  std::stringstream logText;
  logText << "@ Edited: " << filename;
  sysoplog(logText.str());
  if (okfsed()) {
    external_text_edit(filename.c_str(), syscfg.gfilesdir, 500, syscfg.gfilesdir, MSGED_FLAG_NO_TAGLINE);
  }
}

