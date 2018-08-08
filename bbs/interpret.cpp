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
#include "bbs/wconstants.h"
#include "core/strings.h"
#include "core/datetime.h"

using std::string;
using std::to_string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

std::string interpret(char ch, const MacroContext& context) {
  if (!context.mci_enabled()) {
    return "";
  }

  switch (ch) {
  case '@':                               // Dir name
    return context.dir().name;
  case '~':                               // Total mails/feedbacks sent
    return to_string(context.u().GetNumEmailSent() +
             context.u().GetNumFeedbackSent() + context.u().GetNumNetEmailSent());
  case '/':                               // Today's date
    return fulldate();
  case '%':                               // Time left today
    return to_string(static_cast<int>(nsl() / 60));
    break;
  case '#':                               // User's number
    return to_string(a()->usernum);
  case '$':                               // File points
    return to_string(context.u().GetFilePoints());
  case '*':                               // User reg num
    return to_string(context.u().GetWWIVRegNumber());
  case '-':                               // Aggravation points
    return to_string(context.u().GetAssPoints());
  case ':':                               // Sub number
    return a()->current_user_sub().keys;
  case ';':                               // Directory number
    return a()->current_user_dir().keys;
  case '!':                               // Built-in pause
    pausescr();
    return "";
  case '&':
    return context.u().HasAnsi() ? "ANSI" : "ASCII";
  case 'A':                               // User's age
    return to_string(context.u().GetAge());
  case 'a':                               // User's language
    return a()->cur_lang_name;
  case 'B':                               // User's birthday
    return StringPrintf("%d/%d/%d", context.u().GetBirthdayMonth(),
             context.u().GetBirthdayDay(), context.u().GetBirthdayYear());
  case 'b':                               // Minutes in bank
    return to_string(context.u().GetTimeBankMinutes());
  case 'C':                               // User's city
    return context.u().GetCity();
  case 'c':                               // User's country
    return context.u().GetCountry();
  case 'D':                               // Files downloaded
    return to_string(context.u().GetFilesDownloaded());
  case 'd':                               // User's DSL
    return to_string(context.u().GetDsl());
  case 'E':                               // E-mails sent
    return to_string(context.u().GetNumEmailSent());
  case 'e':                               // Net E-mails sent
    return to_string(context.u().GetNumNetEmailSent());
  case 'F':
    return to_string(context.u().GetNumFeedbackSent());
  case 'f':                               // First time user called
    return context.u().GetFirstOn();
  case 'G':                               // MessaGes read
    return to_string(context.u().GetNumMessagesRead());
  case 'g':                               // Gold
    return to_string(context.u().GetGold());
  case 'I':                               // User's call sIgn
    return context.u().GetCallsign();
  case 'i':                               // Illegal log-ons
    return to_string(context.u().GetNumIllegalLogons());
  case 'J': {                             // Message conference
    auto x = a()->GetCurrentConferenceMessageArea();
    auto cnum = a()->uconfsub[x].confnum;
    return a()->subconfs[cnum].conf_name;
  }
  case 'j':                               // Transfer conference
    return a()->dirconfs[a()->uconfdir[a()->GetCurrentConferenceFileArea()].confnum].conf_name;
  case 'K':                               // Kb uploaded
    return to_string(context.u().GetUploadK());
  case 'k':                               // Kb downloaded
    return to_string(context.u().GetDownloadK());
  case 'L':                               // Last call
    return context.u().GetLastOn();
  case 'l':                               // Number of logons
    return to_string(context.u().GetNumLogons());
  case 'M':                               // Mail waiting
    return to_string(context.u().GetNumMailWaiting());
  case 'm':                               // Messages posted
    return to_string(context.u().GetNumMessagesPosted());
  case 'N':                               // User's name
    return context.u().GetName();
  case 'n':                               // Sysop's note
    return context.u().GetNote();
  case 'O':                               // Times on today
    return to_string(context.u().GetTimesOnToday());
  case 'o': {
    // Time on today
    auto used_this_session = (std::chrono::system_clock::now() - a()->system_logon_time());
    auto min_used = context.u().timeon() + used_this_session;
    return to_string(std::chrono::duration_cast<std::chrono::minutes>(min_used).count());
  }
  case 'P':                               // BBS phone
    return a()->config()->system_phone();
  case 'p':                               // User's phone
    return context.u().GetDataPhoneNumber();
  case 'R':                               // User's real name
    return context.u().GetRealName();
  case 'r':                               // Last baud rate
    return to_string(context.u().GetLastBaudRate());
  case 'S':                               // User's SL
    return to_string(context.u().GetSl());
  case 's':                               // User's street address
    return context.u().GetStreet();
  case 'T':                               // User's sTate
    return context.u().GetState();
  case 't':                               // Current time
    return times();
  case 'U':                               // Files uploaded
    return to_string(context.u().GetFilesUploaded());
  case 'u':                               // Current sub
    return a()->subs().sub(a()->current_user_sub().subnum).name;
  case 'W':                               // Total # of messages in sub
    return to_string(a()->GetNumMessagesInCurrentMessageArea());
  case 'X':                               // User's sex
    return StringPrintf("%c", context.u().GetGender());
  case 'Y':                               // Your BBS name
    return a()->config()->system_name();
  case 'y':                               // Computer type
    return ctypes(context.u().GetComputerType());
  case 'Z':                               // User's zip code
    return context.u().GetZipcode();
  default:
    return "";
  }
}

