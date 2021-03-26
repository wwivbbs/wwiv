/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_EXCEPTIONS_H
#define INCLUDED_COMMON_EXCEPTIONS_H

#include "core/strings.h"
#include <stdexcept>

namespace wwiv::common {

enum class hangup_type_t { user_logged_off, user_disconnected, sysop_forced };

class hangup_error final : public std::runtime_error {
public:
  hangup_error(hangup_type_t t, const std::string& username)
   : std::runtime_error(wwiv::strings::StrCat(username, " hung up.")), t_(t) {}

  [[nodiscard]] hangup_type_t hangup_type() const noexcept { return t_; }

  hangup_type_t t_;
};

}  // namespace wwiv::common


#endif
