/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_CORE_CLOCK_H__
#define __INCLUDED_CORE_CLOCK_H__

#include "core/datetime.h"

#include <chrono>
#include <ctime>
#include <string>

namespace wwiv {
namespace core {

class Clock {
public:
  Clock() {}
  virtual DateTime Now() noexcept = 0;
  virtual ~Clock() {}
};

class SystemClock : public Clock {
public:
  SystemClock() {}
  virtual ~SystemClock() {}
  virtual DateTime Now() noexcept override;
};

} // namespace core
} // namespace wwiv

#endif // __INCLUDED_CORE_CLOCK_H__
