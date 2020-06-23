/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "core/strings.h"
#include <set>
#include <string>

namespace wwiv {
namespace bbs {

  // Text editing modes for input routines
  enum class InputMode { UPPER, MIXED, PROPER, FILENAME, FULL_PATH_NAME, CMDLINE, DATE, PHONE };

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
std::string input_filename(int max_length);

/**
 * Inputs filename up to length max_length.
 */
std::string input_filename(const std::string& orig_text, int max_length);

/**
 * Inputs full file path up to length max_length.
 */
std::string input_path(int max_length);

/**
 * Inputs full file path up to length max_length.
 */
std::string input_path(const std::string& orig_text, int max_length);

/**
 * Inputs commandline up to length max_length.
 */
std::string input_cmdline(const std::string& orig_text, int max_length);

/**
 * Inputs phone number up to length max_length.
 */
std::string input_phonenumber(const std::string& orig_text, int max_length);

/**
 * Inputs random text (upper and lower case) up to length max_length.
 */
std::string input_text(const std::string& orig_text, int max_length);

/**
 * Inputs random text (upper and lower case) up to length max_length
 * with the ability to turn on or off the MPL.
 */
std::string input_text(const std::string& orig_text, bool mpl, int max_length);

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
 * Inputs a date (10-digit) of MM/DD/YYYY.
 */
std::string input_date_mmddyyyy(const std::string& orig_text);

template <typename T> 
struct input_result_t { 
  T num{0};
  char key{0};
};

/**
 * Raw input_number method. Clients should use input_number or  input_number_hotkey instead.
 */
input_result_t<int64_t> input_number_or_key_raw(int64_t cur, int64_t minv, int64_t maxv,
                                                bool setdefault, const std::set<char>& hotkeys);


//TODO(rushfan): Using an int64_t for the min/max both here and in input_number_or_key_raw
//               is a bit wonky, but works for probably all case in WWIV since it won't be
//               using int64_t user input most likely ever.  Otherwise need to wait till compilers
//               are happy enough with auto everywhere to pass through caller types.
/**
 * Inputs a number of type T within the range of min_value and max_value.
 * If set_default_value is true (the default) then the input box will have
 * the current_value populated.
 */
template <typename T>
T input_number(T current_value, int64_t min_value = std::numeric_limits<T>::min(),
               int64_t max_value = std::numeric_limits<T>::max(), bool set_default_value = true) {
  auto r = input_number_or_key_raw(current_value, min_value, max_value, set_default_value, {'Q'});
  return (r.key != 0) ? current_value : static_cast<T>(r.num);
}

template <typename T>
input_result_t<T> input_number_hotkey(T current_value, const std::set<char>& keys,
                                      int min_value = std::numeric_limits<T>::min(),
                                      int max_value = std::numeric_limits<T>::max(),
                                      bool set_default_value = false) {
  auto orig = input_number_or_key_raw(current_value, min_value, max_value, set_default_value, keys);
  input_result_t<T> r{static_cast<T>(orig.num), orig.key};
  return r;
}

#endif // __INCLUDED_INPUT_H__
