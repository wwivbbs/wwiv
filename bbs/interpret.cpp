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
#include "bbs/interpret.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl2.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/confutil.h"
#include "common/macro_context.h"
#include "common/pause.h"
#include "common/datetime.h"
#include "fmt/printf.h"
#include "core/strings.h"
#include "core/datetime.h"
#include "sdk/config.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"

using std::string;
using std::to_string;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;


std::string BbsMacroContext::interpret(char ch) const {
  if (!context_) {
    return "";
  }
  if (!context_->mci_enabled()) {
    return "";
  }
  try {
    switch (ch) {
    case '@':                               // Dir name
      return context_->session_context().current_dir().name;
    case '~':                               // Total mails/feedbacks sent
      return to_string(context_->u().GetNumEmailSent() + context_->u().GetNumFeedbackSent() +
                       context_->u().GetNumNetEmailSent());
    case '/':                               // Today's date
      return fulldate();
    case '%':                               // Time left today
      return to_string(static_cast<int>(nsl() / 60));
      break;
    case '#':                               // User's number
      return to_string(a()->usernum);
    case '$':                               // File points (UNUSED)
      return "0"; //to_string(u().GetFilePoints());
    case '*':                               // User reg num
      return to_string(context_->u().GetWWIVRegNumber());
    case '-':                               // Aggravation points
      return to_string(context_->u().GetAssPoints());
    case ':':                               // Sub number
      return a()->current_user_sub().keys;
    case ';':                               // Directory number
      return a()->current_user_dir().keys;
    case '!':                               // Built-in pause
      bout.pausescr();
      return "";
    case '&':
      return context_->u().HasAnsi() ? "ANSI" : "ASCII";
    case 'A':                               // User's age
      return to_string(context_->u().age());
    case 'a':                               // User's language
      return context_->session_context().current_language();
    case 'B':                               // User's birthday
      return context_->u().birthday_mmddyy();
    case 'b':                               // Minutes in bank
      return to_string(context_->u().GetTimeBankMinutes());
    case 'C':                               // User's city
      return context_->u().GetCity();
    case 'c':                               // User's country
      return context_->u().GetCountry();
    case 'D':                               // Files downloaded
      return to_string(context_->u().GetFilesDownloaded());
    case 'd':                               // User's DSL
      return to_string(context_->u().GetDsl());
    case 'E':                               // E-mails sent
      return to_string(context_->u().GetNumEmailSent());
    case 'e':                               // Net E-mails sent
      return to_string(context_->u().GetNumNetEmailSent());
    case 'F':
      return to_string(context_->u().GetNumFeedbackSent());
    case 'f':                               // First time user called
      return context_->u().GetFirstOn();
    case 'G':                               // MessaGes read
      return to_string(context_->u().GetNumMessagesRead());
    case 'g':                               // Gold
      return to_string(context_->u().gold());
    case 'I':                               // User's call sIgn
      return context_->u().GetCallsign();
    case 'i':                               // Illegal log-ons
      return to_string(context_->u().GetNumIllegalLogons());
    case 'J': {                             // Message conference
      const int x = context_->session_context().current_user_sub_conf_num();
      if (!has_userconf_to_subconf(x)) {
        return {};
      }
      const int cnum = userconf_to_subconf(x);
      if (cnum < 0 || cnum >= wwiv::stl::ssize(a()->uconfsub)) {
        return {};
      }
      return a()->subconfs[cnum].conf_name;
    }
    case 'j': { // Transfer conference
      const int x = a()->sess().current_user_dir_conf_num();
      if (!has_userconf_to_dirconf(x)) {
        return {};
      }
      const auto cn = userconf_to_dirconf(x);
      return a()->dirconfs[cn].conf_name;
    } break;
    case 'K':                               // Kb uploaded
      return to_string(context_->u().uk());
    case 'k':                               // Kb downloaded
      return to_string(context_->u().dk());
    case 'L':                               // Last call
      return context_->u().GetLastOn();
    case 'l':                               // Number of logons
      return to_string(context_->u().GetNumLogons());
    case 'M':                               // Mail waiting
      return to_string(context_->u().GetNumMailWaiting());
    case 'm':                               // Messages posted
      return to_string(context_->u().GetNumMessagesPosted());
    case 'N':                               // User's name
      return context_->u().GetName();
    case 'n':                               // Sysop's note
      return context_->u().GetNote();
    case 'O':                               // Times on today
      return to_string(context_->u().GetTimesOnToday());
    case 'o': {
      // Time on today
      const auto used_this_session =
          (std::chrono::system_clock::now() - context_->session_context().system_logon_time());
      const auto min_used = context_->u().timeon() + used_this_session;
      return to_string(std::chrono::duration_cast<std::chrono::minutes>(min_used).count());
    }
    case 'P':                               // BBS phone
      return (config_ == nullptr) ? "" : config_->system_phone();
    case 'p':                               // User's phone
      return context_->u().GetDataPhoneNumber();
    case 'R':                               // User's real name
      return context_->u().GetRealName();
    case 'r':                               // Last baud rate
      return to_string(context_->u().GetLastBaudRate());
    case 'S':                               // User's SL
      return to_string(context_->u().GetSl());
    case 's':                               // User's street address
      return context_->u().GetStreet();
    case 'T':                               // User's sTate
      return context_->u().GetState();
    case 't':                               // Current time
      return times();
    case 'U':                               // Files uploaded
      return to_string(context_->u().GetFilesUploaded());
    case 'u':  {                             // Current sub
      const auto subnum = a()->current_user_sub().subnum;
      if (subnum < 0 || subnum >= wwiv::stl::ssize(a()->subs())) {
        return {};
      }
      return a()->subs().sub(a()->current_user_sub().subnum).name;
    }
    case 'W':                               // Total # of messages in sub
      return to_string(a()->GetNumMessagesInCurrentMessageArea());
    case 'X':                               // User's sex
      return fmt::sprintf("%c", context_->u().GetGender());
    case 'Y':                               // Your BBS name
      return (config_ == nullptr) ? "" : config_->system_name();
    case 'y':                               // Computer type
      return ctypes(context_->u().GetComputerType());
    case 'Z':                               // User's zip code
      return context_->u().GetZipcode();
    default:
      return {};
    }
      
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught exception in interpret(ch): '" << ch << "' :" << e.what();
  }
  return {}; 
}

bool BbsMacroFilter::write(char c) {
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

bool BbsMacroFilter::attr(uint8_t a) { 
  return chain_->attr(a);
}
