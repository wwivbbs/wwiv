/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2016 WWIV Software Services                  */
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
#ifndef __INCLUDED_NETWORKB_FIDO_UTIL_H__
#define __INCLUDED_NETWORKB_FIDO_UTIL_H__

#include <set>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/networks.h"
#include "sdk/net.h"

namespace wwiv {
namespace net {

std::string packet_name();
std::string bundle_name(std::string address);

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_FIDO_UTIL_H__
