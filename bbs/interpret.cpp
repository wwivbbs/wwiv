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
#include "bbs/interpret.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl2.h"
#include "bbs/fcns.h"
#include "bbs/pause.h"
#include "bbs/vars.h"
#include "bbs/datetime.h"
#include "bbs/wconstants.h"
#include "core/strings.h"

using namespace wwiv::strings;
using std::string;
using std::to_string;

std::string interpret(char ch) {
  if (g_flags & g_flag_disable_mci) {
    return "";
  }

  switch (ch) {
  case '@':                               // Dir name
    return a()->directories[a()->current_user_dir().subnum].name;
  case '~':                               // Total mails/feedbacks sent
    return to_string(a()->user()->GetNumEmailSent() +
             a()->user()->GetNumFeedbackSent() + a()->user()->GetNumNetEmailSent());
  case '/':                               // Today's date
    return fulldate();
  case '%':                               // Time left today
    return to_string(static_cast<int>(nsl() / 60));
    break;
  case '#':                               // User's number
    return to_string(a()->usernum);
  case '$':                               // File points
    return to_string(a()->user()->GetFilePoints());
  case '*':                               // User reg num
    return to_string(a()->user()->GetWWIVRegNumber());
  case '-':                               // Aggravation points
    return to_string(a()->user()->GetAssPoints());
  case ':':                               // Sub number
    return a()->current_user_sub().keys;
  case ';':                               // Directory number
    return a()->current_user_dir().keys;
  case '!':                               // Built-in pause
    pausescr();
    return "";
  case '&':
    return a()->user()->HasAnsi() ? "ANSI" : "ASCII";
  case 'A':                               // User's age
    return to_string(a()->user()->GetAge());
  case 'a':                               // User's language
    return a()->cur_lang_name;
  case 'B':                               // User's birthday
    return StringPrintf("%d/%d/%d", a()->user()->GetBirthdayMonth(),
             a()->user()->GetBirthdayDay(), a()->user()->GetBirthdayYear());
  case 'b':                               // Minutes in bank
    return to_string(a()->user()->GetTimeBankMinutes());
  case 'C':                               // User's city
    return a()->user()->GetCity();
  case 'c':                               // User's country
    return a()->user()->GetCountry();
  case 'D':                               // Files downloaded
    return to_string(a()->user()->GetFilesDownloaded());
  case 'd':                               // User's DSL
    return to_string(a()->user()->GetDsl());
  case 'E':                               // E-mails sent
    return to_string(a()->user()->GetNumEmailSent());
  case 'e':                               // Net E-mails sent
    return to_string(a()->user()->GetNumNetEmailSent());
  case 'F':
    return to_string(a()->user()->GetNumFeedbackSent());
  case 'f':                               // First time user called
    return a()->user()->GetFirstOn();
  case 'G':                               // MessaGes read
    return to_string(a()->user()->GetNumMessagesRead());
  case 'g':                               // Gold
    return to_string(a()->user()->GetGold());
  case 'I':                               // User's call sIgn
    return a()->user()->GetCallsign();
  case 'i':                               // Illegal log-ons
    return to_string(a()->user()->GetNumIllegalLogons());
  case 'J':                               // Message conference
    return reinterpret_cast<char*>(subconfs[uconfsub[a()->GetCurrentConferenceMessageArea()].confnum].name);
  case 'j':                               // Transfer conference
    return reinterpret_cast<char*>(dirconfs[uconfdir[a()->GetCurrentConferenceFileArea()].confnum].name);
  case 'K':                               // Kb uploaded
    return to_string(a()->user()->GetUploadK());
  case 'k':                               // Kb downloaded
    return to_string(a()->user()->GetDownloadK());
  case 'L':                               // Last call
    return a()->user()->GetLastOn();
  case 'l':                               // Number of logons
    return to_string(a()->user()->GetNumLogons());
  case 'M':                               // Mail waiting
    return to_string(a()->user()->GetNumMailWaiting());
  case 'm':                               // Messages posted
    return to_string(a()->user()->GetNumMessagesPosted());
  case 'N':                               // User's name
    return a()->user()->GetName();
  case 'n':                               // Sysop's note
    return a()->user()->GetNote();
  case 'O':                               // Times on today
    return to_string(a()->user()->GetTimesOnToday());
  case 'o': {
    // Time on today
    auto used_this_session = (std::chrono::system_clock::now() - a()->system_logon_time());
    auto min_used = a()->user()->timeon() + used_this_session;
    return to_string(std::chrono::duration_cast<std::chrono::minutes>(min_used).count());
  }
  case 'P':                               // BBS phone
    return reinterpret_cast<char*>(syscfg.systemphone);
  case 'p':                               // User's phone
    return a()->user()->GetDataPhoneNumber();
  case 'R':                               // User's real name
    return a()->user()->GetRealName();
  case 'r':                               // Last baud rate
    return to_string(a()->user()->GetLastBaudRate());
  case 'S':                               // User's SL
    return to_string(a()->user()->GetSl());
  case 's':                               // User's street address
    return a()->user()->GetStreet();
  case 'T':                               // User's sTate
    return a()->user()->GetState();
  case 't':                               // Current time
    return times();
  case 'U':                               // Files uploaded
    return to_string(a()->user()->GetFilesUploaded());
  case 'u':                               // Current sub
    return a()->subs().sub(a()->current_user_sub().subnum).name;
  case 'W':                               // Total # of messages in sub
    return to_string(a()->GetNumMessagesInCurrentMessageArea());
  case 'X':                               // User's sex
    return StringPrintf("%c", a()->user()->GetGender());
  case 'Y':                               // Your BBS name
    return syscfg.systemname;
  case 'y':                               // Computer type
    return ctypes(a()->user()->GetComputerType());
  case 'Z':                               // User's zip code
    return a()->user()->GetZipcode();
  default:
    return "";
  }
}

