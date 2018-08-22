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
#include <set>
#include <string>
#include <vector>
#include <type_traits>

#include "core/strings.h"
#include "core/wwivassert.h"

namespace wwiv {
namespace bbs {
// Text editing modes for input routines
enum class InputMode { UPPER, MIXED, PROPER, FILENAME, FULL_PATH_NAME, DATE, PHONE };

} // namespace bbs
} // namespace wwiv

void input(char* out_text, int max_length, bool auto_mpl = false);
std::string input(int max_length, bool auto_mpl = false);

/**
 * Inputs password up to length max_length after displaying prompt_text
 */
std::string input_password(const std::string& prompt_text, int max_length);

/**
 * Inputs filename up to length max_length.
 */
std::string input_filename(const std::string& orig_text, int max_length);

/**
 * Inputs full file path up to length max_length.
 */
std::string input_path(const std::string& orig_text, int max_length);

/**
 * Inputs commandline up to length max_length.
 */
std::string input_cmdline(const std::string& orig_text, int max_length);

/**
 * Inputs phonenumber up to length max_length.
 */
std::string input_phonenumber(const std::string& orig_text, int max_length);

/**
 * Inputs random text (upper and lower case) up to length max_length.
 */
std::string input_text(const std::string& orig_text, int max_length);

/**
 * Inputs random text (upper and lower case) up to length max_length.
 */
std::string input_text(int max_length);

/**
 * Inputs random text (upper case) up to length max_length.
 */
std::string input_upper(const std::string& orig_text, int max_length);

/**
 * Inputs random text (upper case) up to length max_length.
 */
std::string input_upper(int max_length);

/**
 * Inputs random text (In Proper Case) up to length max_length.
 */
std::string input_proper(const std::string& orig_text, int max_length);

/**
 * Inputs a date (10-digit) of MM/DD/YY.
 */
std::string input_date_mmddyy(const std::string& orig_text);

/**
 * Inputs a number of type T within the range of min_value and max_value.
 * If set_default_value is true (the default) then the input box will have
 * the current_value prepopulated.
 */
template <typename T>
typename std::enable_if<std::is_signed<T>::value, T>::type
input_number(T current_value, int min_value = std::numeric_limits<T>::min(),
             int max_value = std::numeric_limits<T>::max(), bool set_default_value = true) {
  auto len = std::max<int>(1, static_cast<T>(std::floor(std::log10(max_value))) + 1);
  std::string default_value = (set_default_value ? std::to_string(current_value) : "");
  const auto s = input_upper(default_value, len);
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
template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
input_number(T current_value, int min_value = std::numeric_limits<T>::min(),
             int max_value = std::numeric_limits<T>::max(), bool set_default_value = true) {
  WWIV_ASSERT(max_value >= 0);
  auto len = std::max<int>(1, static_cast<T>(std::floor(std::log10(max_value))) + 1);
  std::string default_value = (set_default_value ? std::to_string(current_value) : "");
  const auto s = input_upper(default_value, len);
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

struct input_result_t { 
  int64_t num{0};
  char key{0};
};

input_result_t input_number_or_key_raw(int64_t cur, int64_t minv, int64_t maxv, bool setdefault,
                                       std::set<char> hotkeys);

std::vector<std::set<char>> create_allowed_charmap(int64_t minv, int64_t maxv);

#endif // __INCLUDED_INPUT_H__
