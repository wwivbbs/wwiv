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
#ifndef __INCLUDED_SDK_ANSI_H__
#define __INCLUDED_SDK_ANSI_H__

#include "sdk/ansi/framebuffer.h"
#include "sdk/ansi/vscreen.h"

#include <cstdint>
#include <vector>

namespace wwiv {
namespace sdk {
namespace ansi {

enum class AnsiMode { in_sequence, not_in_sequence };

class Ansi {
public:
  Ansi(VScreen* b, uint8_t default_attr);
  virtual ~Ansi() = default;

  bool write(char c);
  bool write(const std::string& s) {
    bool result = true;
    for (const auto c : s) {
      if (!write(c)) {
        result = false;
      }
    }
    return result;
  }
  bool attr(uint8_t a);
  bool reset();

private:
  bool write_in_sequence(char c);
  bool write_not_in_sequence(char c);
  bool write_not_in_sequence(std::string& s) {
    for (const auto c : s) {
      write_not_in_sequence(c);
    }
    return true;
  }
  bool ansi_sequence_error(char c);
  bool ansi_sequence_done();

  VScreen* b_;
  uint8_t default_attr_;
  AnsiMode state_{AnsiMode::not_in_sequence};
  std::string ansi_sequence_;

  int saved_x_{0};
  int saved_y_{0};
};

} // namespace ansi
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_ANSI_H__
