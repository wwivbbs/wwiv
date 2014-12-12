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
#include <cstdint>
#include <functional>
#include <string>

namespace wwiv {
namespace os {

// Sleeps for a duration of time d, or until predicate returns true.
// returns the value of predicate.
bool wait_for(std::function<bool()> predicate, std::chrono::milliseconds d);

// Sleeps for a duration of time d
void sleep_for(std::chrono::milliseconds d);

// Yields the CPU to other threads or processes.
void yield();

// Gets the OS Version Number.
std::string os_version_string();

// plays a sound at frequency for duration
void sound(uint32_t frequency, std::chrono::milliseconds d);

// returns a random number.
int random_number(int max_value);

std::string environment_variable(const std::string& variable_name);


}  // namespace os
}  // namespace wwiv

#endif  // __INCLUDED_WWIV_CORE_OS_H__