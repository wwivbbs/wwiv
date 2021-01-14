/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2021, WWIV Software Services            */
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
#ifndef INCLUDED_BBS_ACS_H
#define INCLUDED_BBS_ACS_H

#include "common/input.h"
#include "common/output.h"
#include "sdk/acs/acs.h"

#include <string>

namespace wwiv::bbs {

bool check_acs(const std::string& expression, sdk::acs::acs_debug_t debug = sdk::acs::acs_debug_t::none);
bool validate_acs(const std::string& expression, sdk::acs::acs_debug_t debug = sdk::acs::acs_debug_t::none);
std::string input_acs(common::Input& in, common::Output& out, const std::string& prompt, 
                      const std::string& orig_text, int max_length);


}

#endif
