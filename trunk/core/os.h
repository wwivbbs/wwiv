/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#ifndef __INCLUDED_WWIV_CORE_OS_H__
#define __INCLUDED_WWIV_CORE_OS_H__
#pragma once

#include <chrono>
#include <functional>

namespace wwiv {
namespace os {

bool wait_for(std::function<bool()> predicate, std::chrono::milliseconds d);
void sleep_for(std::chrono::milliseconds d);

}  // namespace os
}  // namespace wwiv

#endif  // __INCLUDED_WWIV_CORE_OS_H__