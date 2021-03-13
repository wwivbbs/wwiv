/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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
#ifndef INCLUDED_WWIVCONFIG_SUBACC_H
#define INCLUDED_WWIVCONFIG_SUBACC_H

#include "sdk/subxtr.h"
#include <optional>

namespace wwiv::sdk {
class Config;
}

struct postrec;
void close_sub();
bool open_sub(bool wr);
bool iscan1(int si, const wwiv::sdk::Subs&, const wwiv::sdk::Config&);
std::optional<postrec> get_post(int mn);
void write_post(int mn, const postrec& pp);

int GetNumMessagesInCurrentMessageArea();

#endif  // INCLUDED_WWIVCONFIG_SUBACC_H