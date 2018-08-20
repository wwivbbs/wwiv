/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_INPUT_H__
#define __INCLUDED_INPUT_H__

#include <cmath>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "core/wwivassert.h"

namespace wwiv {
namespace bbs {
// Text editing modes for input routines
enum class InputMode {
  UPPER,
  MIXED,
  PROPER,
  FILENAME,
  FULL_PATH_NAME,
  DATE,
  PHONE 
};

}  // namespace bbs
}  // namespace wwiv

void input(char *out_text, int max_length, bool auto_mpl = false);
std::string input(int max_length, bool auto_mpl = false);
void inputl(char *out_text, int max_length, bool auto_mpl = false);
std::string inputl(int max_length, bool auto_mpl = false);
void Input1(char *out_text, const std::string& orig_text, int max_length, bool bInsert, wwiv::bbs::InputMode mode);
std::string Input1(const std::string& orig_text, int max_length, bool bInsert, wwiv::bbs::InputMode mode);

/**
 * Inputs password up to length max_length after displaying prompt_text
 */
std::string input_password(const std::string& prompt_text, int max_length);

/**
 * Inputs filename up to length max_length.
 */
std::string input_filename(const std::string& orig_text, int max_length);

/**
 * Inputs commandline up to length max_length.
 */
std::string input_cmdline(const std::string& orig_text, int max_length);

/**
 * Inputs a number of type T within the range of min_value and max_value.
 * If set_default_value is true (the default) then the input box will have
 * the current_value prepopulated.
 */
template <typename T>
typename std::enable_if<std::is_signed<T>::value, T>::type
input_number(T current_value, int min_value = std::numeric_limits<T>::min(),
             int max_value = std::numeric_limits<T>::max(), bool set_default_value = true) {
  auto len = std::max<int>(1, static_cast<T>(std::ceil(std::log10(max_value))) + 1);
  std::string default_value = (set_default_value ? std::to_string(current_value) : "");
  const auto s = Input1(default_value, len, true, wwiv::bbs::InputMode::UPPER);
  try {
    auto value = wwiv::strings::to_number<T>(s);
    if (value < static_cast<T>(min_value) || value > static_cast<T>(max_value)) {
      return current_value;
    }
    return static_cast<T>(value);
  } catch (std::invalid_argument&) {
    return current_value;
  }
}

/**
 * Inputs a number of type T within the range of min_value and max_value.
 * If set_default_value is true (the default) then the input box will have
 * the current_value prepopulated.
 */
template<typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
input_number(T current_value, int min_value = std::numeric_limits<T>::min(),
             int max_value = std::numeric_limits<T>::max(), bool set_default_value = true) {
  WWIV_ASSERT(max_value >= 0); 
  auto len = std::max<int>(1, static_cast<T>(std::ceil(std::log10(max_value))) + 1);
  std::string default_value = (set_default_value ? std::to_string(current_value) : "");
  const auto s = Input1(default_value, len, true, wwiv::bbs::InputMode::UPPER);
  try {
    auto value = wwiv::strings::to_number<T>(s);
    if (value < static_cast<T>(min_value) || value > static_cast<T>(max_value)) {
      return current_value;
    }
    return static_cast<T>(value);
  } catch (std::invalid_argument&) {
    return current_value;
  }
}


#endif  // __INCLUDED_INPUT_H__
