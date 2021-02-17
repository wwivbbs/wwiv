/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "bbs/confutil.h"
#include "bbs/utility.h"
#include "common/datetime.h"
#include "common/macro_context.h"
#include "common/pause.h"
#include "core/datetime.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"

using std::string;
using std::to_string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;


std::string BbsMacroContext::interpret_macro_char(char ch) const {
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
    case '~':                               // Total mails/feed backs sent
      return to_string(context_->u().email_sent() + context_->u().feedback_sent() +
                       context_->u().email_net());
    case '/':                               // Today's date
      return fulldate();
    case '%':                               // Time left today
      return to_string(static_cast<int>(nsl() / 60));
    case '#':                               // User's number
      return to_string(a()->sess().user_num());
    case '$':                               // File points (UNUSED)
      return "0"; //to_string(u().GetFilePoints());
    case '*':                               // User reg num
      return to_string(context_->u().wwiv_regnum());
    case '-':                               // Aggravation points
      return to_string(context_->u().ass_points());
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
      return to_string(context_->u().banktime_minutes());
    case 'C':                               // User's city
      return context_->u().city();
    case 'c':                               // User's country
      return context_->u().country();
    case 'D':                               // Files downloaded
      return to_string(context_->u().downloaded());
    case 'd':                               // User's DSL
      return to_string(context_->u().dsl());
    case 'E':                               // E-mails sent
      return to_string(context_->u().email_sent());
    case 'e':                               // Net E-mails sent
      return to_string(context_->u().email_net());
    case 'F':
      return to_string(context_->u().feedback_sent());
    case 'f':                               // First time user called
      return context_->u().firston();
    case 'G':                               // Messages read
      return to_string(context_->u().messages_read());
    case 'g':                               // Gold
      return to_string(context_->u().gold());
    case 'I':                               // User's call sIgn
      return context_->u().callsign();
    case 'i':                               // Illegal log-ons
      return to_string(context_->u().illegal_logons());
    case 'J': {                             // Message conference
      const int x = context_->session_context().current_user_sub_conf_num();
      if (x < 0 || x >= wwiv::stl::ssize(a()->uconfsub)) {
        return {};
      }
      return wwiv::stl::at(a()->uconfsub, x).conf_name;
    }
    case 'j': { // Transfer conference
      const int x = a()->sess().current_user_dir_conf_num();
      if (x < 0 || x >= wwiv::stl::ssize(a()->uconfdir)) {
        return {};
      }
      return wwiv::stl::at(a()->uconfdir, x).conf_name;
    }
    case 'K':                               // Kb uploaded
      return to_string(context_->u().uk());
    case 'k':                               // Kb downloaded
      return to_string(context_->u().dk());
    case 'L':                               // Last call
      return context_->u().laston();
    case 'l':                               // Number of logons
      return to_string(context_->u().logons());
    case 'M':                               // Mail waiting
      return to_string(context_->u().email_waiting());
    case 'm':                               // Messages posted
      return to_string(context_->u().messages_posted());
    case 'N':                               // User's name
      return context_->u().name();
    case 'n':                               // Sysop's note
      return context_->u().note();
    case 'O':                               // Times on today
      return to_string(context_->u().ontoday());
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
      return context_->u().data_phone();
    case 'R':                               // User's real name
      return context_->u().real_name();
    case 'r':                               // Last baud rate
      return to_string(context_->u().last_bps());
    case 'S':                               // User's SL
      return to_string(context_->u().sl());
    case 's':                               // User's street address
      return context_->u().street();
    case 'T':                               // User's sTate
      return context_->u().state();
    case 't':                               // Current time
      return times();
    case 'U':                               // Files uploaded
      return to_string(context_->u().uploaded());
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
      return context_->u().zip_code();
    default:
      return {};
    }
      
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught exception in interpret_macro_char(ch): '" << ch << "' :" << e.what();
  }
  return {}; 
}

wwiv::common::Interpreted BbsMacroContext::interpret_string(const std::string& s) const {
  if (s.length() < 2) {
    // We need at least 2 chars to be useful.
    return s;
  }
  const auto code = s.front();
  auto data = s.substr(1);
  switch (code) {
    case '@':
      return interpret_macro_char(data.front());
    case '[': {
      // movement
      const auto type = data.back();
      data.pop_back();
      wwiv::common::Interpreted res;
      res.cmd = wwiv::common::interpreted_cmd_t::movement;
      switch (type) {
        case 'A': res.up = data.empty() ? 1 : to_number<int>(data); break;
        case 'B': res.down = data.empty() ? 1 : to_number<int>(data); break;
        case 'C': res.right = data.empty() ? 1 : to_number<int>(data); break;
        case 'D': res.left = data.empty() ? 1 : to_number<int>(data); break;
        case 'H': {
          const auto semi = data.find(';');
          if (data.empty() || semi == std::string::npos) {
            return s;
          }
          const auto x = to_number<int>(data.substr(0, semi))-1;
          res.x = std::max<int>(0, x);
          const auto y = to_number<int>(data.substr(semi + 1)) - 1;
          res.y = std::max<int>(0,y);
          return res;
        }
        case 'J': {
          res.cls = true;
          return res;
        }
        case 'K': {
          const auto ct = data.empty() ? 0 : to_number<int>(data);
          if (ct == 0) {
            res.clreol = true;
          } else if (ct == 1) {
            res.clrbol = true;
          } else if (ct == 2) {
            res.clreol = true;
            res.clrbol = true;
          }
          return res;
        }
        case 'N': { // NL
          const auto ct = data.empty() ? 1 : to_number<int>(data);
          res.nl = ct;
          return res;
        }
        default:
          return s;
      }
      return res;
    }
  }
  return {};
}

wwiv::common::Interpreted BbsMacroContext::evaluate_expression(const std::string& expr) const {
  wwiv::common::Interpreted r{};
  r.text = eval_fn_(expr);
  return r;
}

bool BbsMacroFilter::write(char c) {
  if (pipe_ == pipe_type_t::macro) {
    auto s = ctx_->interpret_macro_char(c);
    for (const auto ch : s) {
      chain_->write(ch);
    }
    return true;
  }
  // todo - handle expr and movement
  if (pipe_ == pipe_type_t::pipe) {
    if (c == '@') {
      pipe_ = pipe_type_t::macro;
      return true;
    }
    if (c == '{') {
      pipe_ = pipe_type_t::expr;
      return true;
    }
    if (c == '[') {
      pipe_ = pipe_type_t::movement;
      return true;
    }
    pipe_ = pipe_type_t::none;
    chain_->write('|');
    return chain_->write(c);
  }
  if (c == '|') {
    pipe_ = pipe_type_t::pipe;
    return true;
  }
  return chain_->write(c);
}

bool BbsMacroFilter::attr(uint8_t a) { 
  return chain_->attr(a);
}
