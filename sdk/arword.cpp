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
#include "sdk/arword.h"

#include <string>
#include "core/strings.h"

using namespace wwiv::strings;

/*
 * Returns bitmapped word representing an AR or DAR string.
 */
uint16_t str_to_arword(const std::string& arstr) {
  uint16_t rar = 0;
  const auto s = ToStringUpperCase(arstr);

  for (int i = 0; i < 16; i++) {
    if (s.find(static_cast<char>(i + 'A')) != std::string::npos) {
      rar |= (1 << i);
    }
  }
  return rar;
}

/*
 * Converts an int to a string representing those ARs (DARs).
 * or '-' on an empty string.
 */
std::string word_to_arstr(int ar, const std::string& empty_ar_str) {

  if (!ar) {
    return empty_ar_str;
  }

  std::string arstr;
  for (auto i = 0; i < 16; i++) {
    if ((1 << i) & ar) {
      arstr.push_back(static_cast<char>('A' + i));
    } else {
      arstr.push_back(' ');
    }
  }
  return arstr.empty() ? empty_ar_str : arstr;
}

/*
 * Converts an int to a string representing those ARs (DARs).
 * or '-' on an empty string.
 */
std::string word_to_arstr_nopadding(int ar, const std::string& empty_ar_str) {

  if (!ar) {
    return empty_ar_str;
  }

  std::string arstr;
  for (auto i = 0; i < 16; i++) {
    if ((1 << i) & ar) {
      arstr.push_back(static_cast<char>('A' + i));
    }
  }
  return arstr.empty() ? empty_ar_str : arstr;
}

