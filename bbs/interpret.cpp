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
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/interpret.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl2.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/pause.h"
#include "bbs/datetime.h"
#include "local_io/wconstants.h"
#include "core/strings.h"
#include "core/datetime.h"

using std::string;
using std::to_string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

std::string MacroContext::interpret(char ch) const {
  if (!mci_enabled()) {
    return "";
  }

  switch (ch) {
  case '@':                               // Dir name
    return dir().name;
  case '~':                               // Total mails/feedbacks sent
    return to_string(u().GetNumEmailSent() +
             u().GetNumFeedbackSent() + u().GetNumNetEmailSent());
  case '/':                               // Today's date
    return fulldate();
  case '%':                               // Time left today
    return to_string(static_cast<int>(nsl() / 60));
    break;
  case '#':                               // User's number
    return to_string(a()->usernum);
  case '$':                               // File points
    return to_string(u().GetFilePoints());
  case '*':                               // User reg num
    return to_string(u().GetWWIVRegNumber());
  case '-':                               // Aggravation points
    return to_string(u().GetAssPoints());
  case ':':                               // Sub number
    return a()->current_user_sub().keys;
  case ';':                               // Directory number
    return a()->current_user_dir().keys;
  case '!':                               // Built-in pause
    pausescr();
    return "";
  case '&':
    return u().HasAnsi() ? "ANSI" : "ASCII";
  case 'A':                               // User's age
    return to_string(u().GetAge());
  case 'a':                               // User's language
    return a()->cur_lang_name;
  case 'B':                               // User's birthday
    return StringPrintf("%d/%d/%d", u().GetBirthdayMonth(),
             u().GetBirthdayDay(), u().GetBirthdayYear());
  case 'b':                               // Minutes in bank
    return to_string(u().GetTimeBankMinutes());
  case 'C':                               // User's city
    return u().GetCity();
  case 'c':                               // User's country
    return u().GetCountry();
  case 'D':                               // Files downloaded
    return to_string(u().GetFilesDownloaded());
  case 'd':                               // User's DSL
    return to_string(u().GetDsl());
  case 'E':                               // E-mails sent
    return to_string(u().GetNumEmailSent());
  case 'e':                               // Net E-mails sent
    return to_string(u().GetNumNetEmailSent());
  case 'F':
    return to_string(u().GetNumFeedbackSent());
  case 'f':                               // First time user called
    return u().GetFirstOn();
  case 'G':                               // MessaGes read
    return to_string(u().GetNumMessagesRead());
  case 'g':                               // Gold
    return to_string(u().GetGold());
  case 'I':                               // User's call sIgn
    return u().GetCallsign();
  case 'i':                               // Illegal log-ons
    return to_string(u().GetNumIllegalLogons());
  case 'J': {                             // Message conference
    auto x = a()->GetCurrentConferenceMessageArea();
    auto cnum = a()->uconfsub[x].confnum;
    return a()->subconfs[cnum].conf_name;
  }
  case 'j':                               // Transfer conference
    return a()->dirconfs[a()->uconfdir[a()->GetCurrentConferenceFileArea()].confnum].conf_name;
  case 'K':                               // Kb uploaded
    return to_string(u().GetUploadK());
  case 'k':                               // Kb downloaded
    return to_string(u().GetDownloadK());
  case 'L':                               // Last call
    return u().GetLastOn();
  case 'l':                               // Number of logons
    return to_string(u().GetNumLogons());
  case 'M':                               // Mail waiting
    return to_string(u().GetNumMailWaiting());
  case 'm':                               // Messages posted
    return to_string(u().GetNumMessagesPosted());
  case 'N':                               // User's name
    return u().GetName();
  case 'n':                               // Sysop's note
    return u().GetNote();
  case 'O':                               // Times on today
    return to_string(u().GetTimesOnToday());
  case 'o': {
    // Time on today
    auto used_this_session = (std::chrono::system_clock::now() - a()->system_logon_time());
    auto min_used = u().timeon() + used_this_session;
    return to_string(std::chrono::duration_cast<std::chrono::minutes>(min_used).count());
  }
  case 'P':                               // BBS phone
    return a()->config()->system_phone();
  case 'p':                               // User's phone
    return u().GetDataPhoneNumber();
  case 'R':                               // User's real name
    return u().GetRealName();
  case 'r':                               // Last baud rate
    return to_string(u().GetLastBaudRate());
  case 'S':                               // User's SL
    return to_string(u().GetSl());
  case 's':                               // User's street address
    return u().GetStreet();
  case 'T':                               // User's sTate
    return u().GetState();
  case 't':                               // Current time
    return times();
  case 'U':                               // Files uploaded
    return to_string(u().GetFilesUploaded());
  case 'u':                               // Current sub
    return a()->subs().sub(a()->current_user_sub().subnum).name;
  case 'W':                               // Total # of messages in sub
    return to_string(a()->GetNumMessagesInCurrentMessageArea());
  case 'X':                               // User's sex
    return StringPrintf("%c", u().GetGender());
  case 'Y':                               // Your BBS name
    return a()->config()->system_name();
  case 'y':                               // Computer type
    return ctypes(u().GetComputerType());
  case 'Z':                               // User's zip code
    return u().GetZipcode();
  default:
    return "";
  }
}

bool BbsMacroFiilter::write(char c) {
  if (in_macro_) {
    auto s = ctx_->interpret(c);
    for (const auto ch : s) {
      chain_->write(ch);
    }
    return true;
  } else if (in_pipe_) {
    if (c == '@') {
      in_macro_ = true;
      in_pipe_ = false;
      return true;
    } else {
      in_macro_ = false;
      in_pipe_ = false;
      chain_->write('|');
      return chain_->write(c);
    }
  } else if (c == '|') {
    in_pipe_ = true;
    in_macro_ = false;
    return true;
  } else {
    return chain_->write(c);
  }
}

bool BbsMacroFiilter::attr(uint8_t a) { 
  return chain_->attr(a);
}
