/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
std::string input_password(const std::string& prompt_text, int max_length);


template<typename T>
typename std::enable_if<std::is_signed<T>::value, T>::type
input_number(T current_value, T min_value, T max_value, bool set_default_value = true) {
  int len = std::max<int>(1, static_cast<T>(ceil(log10(max_value))));
  std::string default_value = (set_default_value ? std::to_string(current_value) : "");
  std::string s = Input1(default_value, len, true, wwiv::bbs::InputMode::UPPER);
  try {
    long value = static_cast<T>(std::stoul(s));
    if (value < min_value || value > max_value) {
      return current_value;
    }
    return static_cast<T>(value);
  } catch (std::invalid_argument&) {
    return current_value;
  }
}

template<typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
input_number(T current_value, T min_value, T max_value, bool set_default_value = true) {
  int len = std::max<int>(1, static_cast<T>(ceil(log10(max_value))));
  std::string default_value = (set_default_value ? std::to_string(current_value) : "");
  std::string s = Input1(default_value, len, true, wwiv::bbs::InputMode::UPPER);
  try {
    unsigned long value = std::stoul(s);
    if (value < min_value || value > max_value) {
      return current_value;
    }
    return static_cast<T>(value);
  } catch (std::invalid_argument&) {
    return current_value;
  }
}


#endif  // __INCLUDED_INPUT_H__
