/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2026, WWIV Software Services              */
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
#include "bbs/wwivfoss_inband.h"

namespace wwiv::bbs {

std::string wwivfoss_escape(const char* data, size_t len) {
  std::string out;
  out.reserve(len + 8);
  for (size_t i = 0; i < len; i++) {
    const auto ch = static_cast<uint8_t>(data[i]);
    out.push_back(static_cast<char>(ch));
    if (ch == WWIVFOSS_INBAND_ESCAPE) {
      out.push_back(static_cast<char>(WWIVFOSS_INBAND_ESCAPE));
    }
  }
  return out;
}

std::string wwivfoss_line_state(uint8_t state) {
  std::string out;
  out.push_back(static_cast<char>(WWIVFOSS_INBAND_ESCAPE));
  out.push_back(static_cast<char>(state));
  return out;
}

std::string WWIVFossDecoder::decode(const char* data, size_t len) {
  std::string out;
  out.reserve(len);
  for (size_t i = 0; i < len; i++) {
    const auto ch = static_cast<uint8_t>(data[i]);
    if (!escape_) {
      if (ch == WWIVFOSS_INBAND_ESCAPE) {
        escape_ = true;
      } else {
        out.push_back(static_cast<char>(ch));
      }
      continue;
    }
    escape_ = false;
    if (ch == WWIVFOSS_INBAND_ESCAPE) {
      out.push_back(static_cast<char>(ch));
    } else {
      line_state_ = ch;
    }
  }
  return out;
}

} // namespace wwiv::bbs
