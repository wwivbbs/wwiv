/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
/**************************************************************************/
#include "bbs/context.h"

#include "bbs/application.h"
#include "core/log.h"
#include "core/strings.h"
#include "local_io/local_io.h"
#include "sdk/config.h"
#include <chrono>
#include <string>

namespace wwiv::bbs {

using namespace wwiv::strings;

SessionContext::SessionContext(LocalIO* io)
    : irt_{}, io_(io) {}

void SessionContext::InitalizeContext(const wwiv::sdk::Config& c) {
  const auto qscan_length = c.qscn_len() / sizeof(uint32_t);
  qscan_ = std::make_unique<uint32_t[]>(qscan_length);
  qsc = qscan_.get();
  qsc_n = qsc + 1;
  qsc_q = qsc_n + (c.max_dirs() + 31) / 32;
  qsc_p = qsc_q + (c.max_subs() + 31) / 32;
  ResetQScanPointers(c);
}

void SessionContext::ResetQScanPointers(const wwiv::sdk::Config& c) {
  memset(qsc, 0, c.qscn_len());
  *qsc = 999;
  memset(qsc_n, 0xff, ((c.max_dirs() + 31) / 32) * 4);
  memset(qsc_q, 0xff, ((c.max_subs() + 31) / 32) * 4);
}

void SessionContext::reset() {
  SetCurrentReadMessageArea(-1);
  set_current_user_sub_conf_num(0);
  set_current_user_dir_conf_num(0);
  SetUserOnline(false);
  SetTimeOnlineLimited(false);

  chat_reason("");
  num_screen_lines(io_->GetDefaultScreenBottom() + 1);
  hangup(false);
  chatting(chatting_t::none);
  using_modem(false);
  clear_irt();
  outcom(false);
  incom(false);
  okmacro(true);
}

void SessionContext::reset_local_io(LocalIO* io) { 
  DCHECK_NE(io, nullptr);
  io_ = io; 
}

void SessionContext::irt(const std::string& irt) { 
  to_char_array(irt_, irt); 
}

}
