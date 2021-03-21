/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include "common/datetime.h"
#include "core/datetime.h"
#include "core/file.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include <string>

namespace wwiv::common {

using namespace std::chrono;
using namespace wwiv::core;

//
// This kludge will get us through 2019 and should not interfere anywhere
// else.
//

void ToggleScrollLockKey() {
#if defined(_WIN32)
  // Simulate a key press
  keybd_event(VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
  // Simulate a key release
  keybd_event(VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
#endif // _WIN32
}

/* This function returns the status of scoll lock.  If scroll lock is active
 * (ie, the user has hit scroll lock + the light is lit if there is a
 * scroll lock LED), the sysop is assumed to be available.
 */
bool sysop1() {
#if defined(_WIN32)
  return (GetKeyState(VK_SCROLL) & 0x1);
#else
  return false;
#endif
}

bool isleap(int year) { return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0); }

/*
 * Returns current time as string formatted like HH:MM:SS (01:13:00).
 */
std::string ctim(long d) {
  if (d < 0) {
    d += SECONDS_PER_DAY;
  }
  const auto hour = (d / SECONDS_PER_HOUR);
  d -= (hour * SECONDS_PER_HOUR);
  const auto minute = static_cast<long>(d / MINUTES_PER_HOUR);
  d -= (minute * MINUTES_PER_HOUR);
  const auto second = static_cast<long>(d);
  return fmt::sprintf("%2.2ld:%2.2ld:%2.2ld", hour, minute, second);
}

std::string ctim(std::chrono::duration<double> d) {
  return ctim(static_cast<long>(std::chrono::duration_cast<seconds>(d).count()));
}

system_clock::duration duration_since_midnight(system_clock::time_point now) {
  auto tnow = system_clock::to_time_t(now);
  tm* date = std::localtime(&tnow);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  const auto midnight = system_clock::from_time_t(std::mktime(date));

  return now - midnight;
}

system_clock::time_point minutes_after_midnight(int minutes) {
  const auto tnow = time_t_now();
  auto* date = std::localtime(&tnow);
  date->tm_hour = minutes / 60;
  date->tm_min = minutes % 60;
  date->tm_sec = 0;

  return system_clock::from_time_t(std::mktime(date));
}

int minutes_since_midnight() {
  const auto d = duration_since_midnight(system_clock::now());
  return duration_cast<minutes>(d).count();
}

} // namespace wwiv::common