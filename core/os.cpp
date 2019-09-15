/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2019, WWIV Software Services           */
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
#include "core/os.h"

#include <chrono>
#include <functional>
#include <random>

#include "core/strings.h"

using std::function;
using std::string;
using namespace std::chrono;
using namespace wwiv::strings;

namespace wwiv::os {

bool wait_for(function<bool()> predicate, duration<double> d) {
  auto now = std::chrono::steady_clock::now();
  const auto end = now + d;
  while (!predicate() && now < end) {
    now = steady_clock::now();
    sleep_for(milliseconds(100));
  }
  return predicate();
}

void yield() {
  // TODO(rushfan): use this_thread::yield once it's been
  // tested on MSVC and GCC (Maybe with MSVC 2015).
  // Then we'll call this:
  // std::this_thread::yield();

  // Also TODO is to investigate if we should use sleep(1) vs. sleep(0) so that we'll
  // yield to threads on other processors.
  // See http://stackoverflow.com/questions/1413630/switchtothread-thread-yield-vs-thread-sleep0-vs-thead-sleep1
  sleep_for(milliseconds(0));
}

int random_number(int max_value) {
  static std::random_device rdev;
  static std::default_random_engine re(rdev());

  std::uniform_int_distribution<int> dist(0, max_value - 1);
  return dist(re);
}

} // namespace wwiv
