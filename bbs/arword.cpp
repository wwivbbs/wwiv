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
#include "bbs/arword.h"

#include <algorithm>
#include <vector>
#include <string>
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/input.h"
#include "core/strings.h"
#include "core/wwivassert.h"

using std::string;
using namespace wwiv::strings;


/*
 * Returns bitmapped word representing an AR or DAR string.
 */
uint16_t str_to_arword(const std::string& arstr) {
  uint16_t rar = 0;
  auto s = ToStringUpperCase(arstr);

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
std::string word_to_arstr(int ar) {

  if (!ar) {
    return "-";
  }

  std::string arstr;
  for (int i = 0; i < 16; i++) {
    if ((1 << i) & ar) {
      arstr.push_back(static_cast<char>('A' + i));
    }
  }
  return (arstr.empty()) ? "-" : arstr;
}

