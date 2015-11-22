/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015, WWIV Software Services                */
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
#pragma once
#ifndef __INCLUDED_NETWORKB_SOCKET_EXCEPTIONS_H__
#define __INCLUDED_NETWORKB_SOCKET_EXCEPTIONS_H__

#include <exception>
#include <stdexcept>
#include <string>

namespace wwiv {
namespace net {

struct socket_error : public std::runtime_error {
socket_error(const std::string& message) : std::runtime_error(message) {}
};

struct socket_closed_error : public socket_error {
socket_closed_error(const std::string& message) : socket_error(message) {}
};

struct timeout_error : public socket_error {
  timeout_error(const std::string& message) : socket_error(message) {}
};

struct connection_error : public socket_error {
  connection_error(const std::string& host, int port);
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_SOCKET_EXCEPTIONS_H__
