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
#include "bbs/qwk/qwk_text.h"

#include "core/strings.h"

namespace wwiv::bbs::qwk {

extern const char* QWKFrom;

std::optional<std::string> get_qwk_from_message(const std::string& text) {
  const auto* qwk_from_start = strstr(text.c_str(), QWKFrom + 2);

  if (qwk_from_start == nullptr) {
    return std::nullopt;
  }

  qwk_from_start += strlen(QWKFrom + 2); // Get past 'QWKFrom:'
  const auto* qwk_from_end = strchr(qwk_from_start, '\r');

  if (qwk_from_end && qwk_from_end > qwk_from_start) {
    std::string temp(qwk_from_start, qwk_from_end - qwk_from_start);

    strings::StringTrim(&temp);
    strings::StringUpperCase(&temp);
    return temp;
  }
  return std::nullopt;
}

// Takes reply packet and converts '227' (ã) to '13' and removes QWK style
// space padding at the end.
std::string make_text_ready(const std::string& text) {
  std::string temp;
  for (const auto c : text) {
    if (c == '\xE3') {
      temp.push_back(13);
      temp.push_back(10);
    } else {
      temp.push_back(c);
    }
  }

  // Remove trailing space character only. wwiv::strings::StringTrimEnd also removes other
  // whitespace characters like \t and \r\n
  const auto pos = temp.find_last_not_of(' ');
  temp.erase(pos + 1);
  return temp;
}


}
