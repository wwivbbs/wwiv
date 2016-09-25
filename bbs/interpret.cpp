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
#include "bbs/fcns.h"
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
    return session()->directories[session()->current_user_dir().subnum].name;
  case '~':                               // Total mails/feedbacks sent
    return to_string(session()->user()->GetNumEmailSent() +
             session()->user()->GetNumFeedbackSent() + session()->user()->GetNumNetEmailSent());
  case '/':                               // Today's date
    return fulldate();
  case '%':                               // Time left today
    return to_string(static_cast<int>(nsl() / 60));
    break;
  case '#':                               // User's number
    return to_string(session()->usernum);
  case '$':                               // File points
    return to_string(session()->user()->GetFilePoints());
  case '*':                               // User reg num
    return to_string(session()->user()->GetWWIVRegNumber());
  case '-':                               // Aggravation points
    return to_string(session()->user()->GetAssPoints());
  case ':':                               // Sub number
    return session()->current_user_sub().keys;
  case ';':                               // Directory number
    return session()->current_user_dir().keys;
  case '!':                               // Built-in pause
    pausescr();
    return "";
  case '&':
    return session()->user()->HasAnsi() ? "ANSI" : "ASCII";
  case 'A':                               // User's age
    return to_string(session()->user()->GetAge());
  case 'a':                               // User's language
    return session()->cur_lang_name;
  case 'B':                               // User's birthday
    return StringPrintf("%d/%d/%d", session()->user()->GetBirthdayMonth(),
             session()->user()->GetBirthdayDay(), session()->user()->GetBirthdayYear());
  case 'b':                               // Minutes in bank
    return to_string(session()->user()->GetTimeBankMinutes());
  case 'C':                               // User's city
    return session()->user()->GetCity();
  case 'c':                               // User's country
    return session()->user()->GetCountry();
  case 'D':                               // Files downloaded
    return to_string(session()->user()->GetFilesDownloaded());
  case 'd':                               // User's DSL
    return to_string(session()->user()->GetDsl());
  case 'E':                               // E-mails sent
    return to_string(session()->user()->GetNumEmailSent());
  case 'e':                               // Net E-mails sent
    return to_string(session()->user()->GetNumNetEmailSent());
  case 'F':
    return to_string(session()->user()->GetNumFeedbackSent());
  case 'f':                               // First time user called
    return session()->user()->GetFirstOn();
  case 'G':                               // MessaGes read
    return to_string(session()->user()->GetNumMessagesRead());
  case 'g':                               // Gold
    return to_string(session()->user()->GetGold());
  case 'I':                               // User's call sIgn
    return session()->user()->GetCallsign();
  case 'i':                               // Illegal log-ons
    return to_string(session()->user()->GetNumIllegalLogons());
  case 'J':                               // Message conference
    return reinterpret_cast<char*>(subconfs[uconfsub[session()->GetCurrentConferenceMessageArea()].confnum].name);
  case 'j':                               // Transfer conference
    return reinterpret_cast<char*>(dirconfs[uconfdir[session()->GetCurrentConferenceFileArea()].confnum].name);
  case 'K':                               // Kb uploaded
    return to_string(session()->user()->GetUploadK());
  case 'k':                               // Kb downloaded
    return to_string(session()->user()->GetDownloadK());
  case 'L':                               // Last call
    return session()->user()->GetLastOn();
  case 'l':                               // Number of logons
    return to_string(session()->user()->GetNumLogons());
  case 'M':                               // Mail waiting
    return to_string(session()->user()->GetNumMailWaiting());
  case 'm':                               // Messages posted
    return to_string(session()->user()->GetNumMessagesPosted());
  case 'N':                               // User's name
    return session()->user()->GetName();
  case 'n':                               // Sysop's note
    return session()->user()->GetNote();
  case 'O':                               // Times on today
    return to_string(session()->user()->GetTimesOnToday());
  case 'o':                               // Time on today
    return to_string(static_cast<long>(
      (session()->user()->GetTimeOn() + timer() - timeon) / SECONDS_PER_MINUTE));
  case 'P':                               // BBS phone
    return reinterpret_cast<char*>(syscfg.systemphone);
  case 'p':                               // User's phone
    return session()->user()->GetDataPhoneNumber();
  case 'R':                               // User's real name
    return session()->user()->GetRealName();
  case 'r':                               // Last baud rate
    return to_string(session()->user()->GetLastBaudRate());
  case 'S':                               // User's SL
    return to_string(session()->user()->GetSl());
  case 's':                               // User's street address
    return session()->user()->GetStreet();
  case 'T':                               // User's sTate
    return session()->user()->GetState();
  case 't':                               // Current time
    return times();
  case 'U':                               // Files uploaded
    return to_string(session()->user()->GetFilesUploaded());
  case 'u':                               // Current sub
    return session()->subs().sub(session()->current_user_sub().subnum).name;
  case 'W':                               // Total # of messages in sub
    return to_string(session()->GetNumMessagesInCurrentMessageArea());
  case 'X':                               // User's sex
    return StringPrintf("%c", session()->user()->GetGender());
  case 'Y':                               // Your BBS name
    return syscfg.systemname;
  case 'y':                               // Computer type
    return ctypes(session()->user()->GetComputerType());
  case 'Z':                               // User's zip code
    return session()->user()->GetZipcode();
  default:
    return "";
  }
}

