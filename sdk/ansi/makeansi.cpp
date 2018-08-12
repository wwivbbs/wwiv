/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*               Copyright (C)2018, WWIV Software Services                */
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
/**************************************************************************/
#include "sdk/ansi/makeansi.h"

#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
namespace ansi {


static void addto(std::string* ansi_str, int num) {
  if (ansi_str->empty()) {
    ansi_str->append("\x1b[");
  } else {
    ansi_str->append(";");
  }
  ansi_str->append(std::to_string(num));
}

std::string makeansi(int attr, int current_attr) {
  static const std::vector<int> kAnsiColorMap = {'0', '4', '2', '6', '1', '5', '3', '7'};

  int catr = current_attr;
  std::string out;
  //  if ((catr & 0x88) ^ (attr & 0x88)) {
  addto(&out, 0);
  addto(&out, 30 + kAnsiColorMap[attr & 0x07] - '0');
  addto(&out, 40 + kAnsiColorMap[(attr & 0x70) >> 4] - '0');
  catr = (attr & 0x77);
  //  }
  if ((catr & 0x07) != (attr & 0x07)) {
    addto(&out, 30 + kAnsiColorMap[attr & 0x07] - '0');
  }
  if ((catr & 0x70) != (attr & 0x70)) {
    addto(&out, 40 + kAnsiColorMap[(attr & 0x70) >> 4] - '0');
  }
  if ((catr & 0x08) != (attr & 0x08)) {
    addto(&out, 1);
  }
  if ((catr & 0x80) != (attr & 0x80)) {
    // Italics will be generated
    addto(&out, 3);
  }
  if (!out.empty()) {
    out += "m";
  }
  return out;
}


} // namespace ansi
} // namespace sdk
} // namespace wwiv
