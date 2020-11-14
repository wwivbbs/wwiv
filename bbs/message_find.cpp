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
#include "bbs/message_find.h"

#include "bbs/bbs.h"
#include "bbs/message_file.h"
#include "bbs/subacc.h"
#include "common/com.h"
#include "common/full_screen.h"
#include "common/input.h"
#include "core/strings.h"
#include "sdk/subxtr.h"

namespace wwiv::bbs {

static std::string last_search_string;
static bool last_search_forward{true};

using namespace wwiv::core;
using namespace wwiv::strings;


find_message_result_t FindNextMessageAgain(int msgno) {
  const auto search_string = last_search_string;
  const auto msgnum_limit = last_search_forward ? a()->GetNumMessagesInCurrentMessageArea() : 1;
  auto tmp_msgnum = msgno;
  auto fnd = false;
  while (tmp_msgnum != msgnum_limit && !fnd) {
    if (last_search_forward) {
      tmp_msgnum++;
    } else {
      tmp_msgnum--;
    }
    if (bin.checka()) {
      break;
    }
    if (!(tmp_msgnum % 5)) {
      bout.bprintf("%5.5d", tmp_msgnum);
      for (auto i1 = 0; i1 < 5; i1++) {
        bout << "\b";
      }
      if (!(tmp_msgnum % 100)) {
        a()->tleft(true);
        a()->CheckForHangup();
      }
    }
    if (auto o = readfile(&(get_post(tmp_msgnum)->msg), a()->current_sub().filename)) {
      auto b = ToStringUpperCase(o.value());
      const std::string title = stripcolors(get_post(tmp_msgnum)->title);
      const auto ft = title.find(search_string) != std::string::npos;
      const auto fb = b.find(search_string) != std::string::npos;
      fnd = ft || fb;
    }
  }
  if (fnd) {
    return {true, tmp_msgnum};
  }
  return { false, -1};
}

find_message_result_t FindNextMessage(int msgno) {
  bout.nl();
  bout << "|#7Find what? (CR=\"" << last_search_string << "\")|#1: ";
  auto search_string = ToStringUpperCase(bin.input(20));
  if (search_string.empty()) {
    search_string = last_search_string;
  }
  bout.nl();
  bout << "|#1Backwards or Forwards? ";
  const auto ch = onek("QBF\r");
  if (ch == 'Q') {
    return {false, -1};
  }
  bout.nl();
  bout << "|#1Searching -> |#2";

  // Store search direction and limit
  const auto search_forward = ch != '-' && ch != 'B';
  last_search_forward = search_forward;
  last_search_string = search_string;

  return FindNextMessageAgain(msgno);
}

find_message_result_t FindNextMessageFS(common::FullScreenView& fs, int msgno) {
  fs.ClearCommandLine();
  fs.PutsCommandLine(fmt::format("|#7Find what? (CR=\"{}\")|#1: ", last_search_string));
  auto search_string = fs.in().input(20);
  if (search_string.empty()) {
    search_string = last_search_string;
  }
  fs.ClearCommandLine();
  fs.PutsCommandLine("|#1Backwards or Forwards? ");
  const auto ch = onek("QBF\r");
  if (ch == 'Q') {
    return {false, -1};
  }
  fs.ClearCommandLine();
  fs.PutsCommandLine("|#1Searching -> |#2 ");

  // Store search direction and limit
  const auto search_forward = ch != '-' && ch != 'B';
  last_search_forward = search_forward;
  last_search_string = search_string;

  return FindNextMessageAgain(msgno);
}

}
