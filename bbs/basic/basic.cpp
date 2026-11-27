/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2026, WWIV Software Services             */
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
#include "bbs/basic/basic.h"

#include "bbs/bbs.h"
#include "common/output.h"
#include "core/log.h"

#ifdef WWIV_WWIVBASIC
#include "bbs/application.h"
#include "bbs/basic/next/bbs_file_base.h"
#include "bbs/basic/next/bbs_message_base.h"
#include "bbs/basic/next/bbs_user_base.h"
#include "bbs/basic/next/next_engine.h"
#include "common/input.h"
#include <memory>
#endif

namespace wwiv::bbs::basic {

bool RunBasicScript(const std::string& script_name) {
#ifdef WWIV_WWIVBASIC
  NextEngine engine(bin, bout, *a()->config(), &a()->context());
  engine.set_message_base(std::make_unique<BbsMessageBase>(*a()));
  engine.set_user_base(std::make_unique<BbsUserBase>(*a()));
  engine.set_file_base(std::make_unique<BbsFileBase>(*a()));
  return engine.RunScript(script_name);
#else
  LOG(WARNING) << "Unable to run script '" << script_name
               << "': this build has no script engine (WWIV_WWIVBASIC).";
  bout.pl("|#6WWIVbasic scripts aren't available on this system.");
  return false;
#endif
}

} // namespace wwiv::bbs::basic
