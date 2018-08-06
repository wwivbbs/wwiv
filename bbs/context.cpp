/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2018, WWIV Software Services             */
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

#include <chrono>
#include <string>

namespace wwiv {
namespace bbs {

SessionContext::SessionContext(Application* a) : a_(a) { reset(); }

void SessionContext::reset() {
  a_->SetCurrentReadMessageArea(-1);
  a_->SetCurrentConferenceMessageArea(0);
  a_->SetCurrentConferenceFileArea(0);
  a_->charbufferpointer_ = 0;
  a_->localIO()->SetTopLine(0);
  a_->screenlinest = a_->defscreenbottom + 1;
  a_->hangup_ = false;
  a_->chatcall_ = false;
  a_->SetChatReason("");
  a_->SetUserOnline(false);
  a_->chatting_ = 0;
  a_->ReadCurrentUser(1);
  a_->received_short_message_ = false;
  a_->set_extratimecall(std::chrono::seconds(0));
  a_->using_modem = 0;
  a_->SetTimeOnlineLimited(false);
}


}
}