/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef INCLUDED_CORE_SOCKET_EXCEPTIONS_H
#define INCLUDED_CORE_SOCKET_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace wwiv::core {

struct socket_error : std::runtime_error {
  explicit socket_error(const std::string& message);
};

struct socket_closed_error : socket_error {
  explicit socket_closed_error(const std::string& message)
    : socket_error(message) {
  }
};

struct timeout_error : socket_error {
  explicit timeout_error(const std::string& message)
    : socket_error(message) {
  }
};

struct connection_error : socket_error {
  connection_error(const std::string& host, int port);
};

} // namespace

#endif
