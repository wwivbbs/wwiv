/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#ifndef INCLUDED_WWIV_CORE_NUMBERS_H
#define INCLUDED_WWIV_CORE_NUMBERS_H

#include "core/strings.h"
#include <string>

template<typename T>
T bytes_to_k(T b) {
  if (b <= 0) {
    return static_cast<T>(0);
  }
  return static_cast<T>((b + 1023) / 1024);
}

template<typename T>
std::string humanize(T num) {
  static constexpr T KB = 1024;
  static constexpr T MB = KB * 1024;
  static constexpr T GB = MB * 1024;
  if (num >= GB) {
    return wwiv::strings::StrCat(std::to_string(num / GB), "G");
  }
  if (num >= MB) {
    return wwiv::strings::StrCat(std::to_string(num / MB), "M");
  }
  if (num >= KB) {
    return wwiv::strings::StrCat(std::to_string(num / KB), "k");
  }
  return wwiv::strings::StrCat(std::to_string(bytes_to_k(num)), "k");
}

#endif