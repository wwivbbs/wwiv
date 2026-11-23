/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2022, WWIV Software Services             */
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
#ifndef INCLUDED_CORE_FAKE_CLOCK_H
#define INCLUDED_CORE_FAKE_CLOCK_H

#include "core/clock.h"
#include "core/datetime.h"

#include <chrono>

namespace wwiv::core {

class FakeClock final : public Clock {
public:
  explicit FakeClock(const DateTime& dt)
    : date_time_(dt) {
  }

  [[nodiscard]] DateTime Now() const noexcept override;
  void tick(std::chrono::duration<double> inc);
  void SleepFor(std::chrono::duration<double>);

private:
  DateTime date_time_;
};

} // namespace

#endif
