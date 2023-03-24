/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_DATETIME_H
#define INCLUDED_COMMON_DATETIME_H

#include "core/datetime.h"
#include <chrono>
#include <ctime>
#include <string>

namespace wwiv::common {

bool isleap(int nYear);
std::string ctim(long d);
std::string ctim(std::chrono::duration<double> d);

/**
 * Returns a duration representing the duration since midnight of the current day.
 */
std::chrono::system_clock::duration
duration_since_midnight(std::chrono::system_clock::time_point now);

/**
 * Returns a time_point representing minutes past midnight in the current day.
 */
std::chrono::system_clock::time_point minutes_after_midnight(int minutes);
/**
 * Returns an integer for the number of minutes since midnight.
 * N.B. This is the same as duration_cast<minutes>(duration_since_midnight(now())).count();
 */
int minutes_since_midnight();

// This really shouldn't be here.
bool sysop_available();
void ToggleScrollLockKey();

} // namespace wwiv::common 

#endif
