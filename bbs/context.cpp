/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2019, WWIV Software Services             */
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
#include "sdk/config.h"

#include <chrono>
#include <string>

namespace wwiv {
namespace bbs {

using namespace wwiv::strings;

SessionContext::SessionContext(Application* a) : a_(a) {}

void SessionContext::InitalizeContext() {
  const auto c = a_->config();
  const auto qscan_length = c->qscn_len() / sizeof(uint32_t);
  qscan_ = std::make_unique<uint32_t[]>(qscan_length);
  qsc = qscan_.get();
  qsc_n = qsc + 1;
  qsc_q = qsc_n + (c->max_dirs() + 31) / 32;
  qsc_p = qsc_q + (c->max_subs() + 31) / 32;
  ResetQScanPointers();
}

void SessionContext::ResetQScanPointers() {
  const auto& c = a_->config();
  memset(qsc, 0, c->qscn_len());
  *qsc = 999;
  memset(qsc_n, 0xff, ((c->max_dirs() + 31) / 32) * 4);
  memset(qsc_q, 0xff, ((c->max_subs() + 31) / 32) * 4);
}

void SessionContext::reset() {
  a_->SetCurrentReadMessageArea(-1);
  a_->SetCurrentConferenceMessageArea(0);
  a_->SetCurrentConferenceFileArea(0);
  a_->localIO()->SetTopLine(0);
  a_->screenlinest = a_->defscreenbottom + 1;
  a_->hangup_ = false;
  a_->SetChatReason("");
  a_->SetUserOnline(false);
  a_->chatting_ = 0;
  a_->ReadCurrentUser(1);
  a_->received_short_message_ = false;
  a_->set_extratimecall(std::chrono::seconds(0));
  a_->using_modem = 0;
  a_->SetTimeOnlineLimited(false);

  bout.charbufferpointer_ = 0;
  clear_irt();
  outcom(false);
  incom(false);
  okmacro(true);
}

void SessionContext::irt(const std::string& irt) { 
  to_char_array(irt_, irt); 
}

}
}
