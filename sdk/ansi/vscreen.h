/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2022, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_VSCREEN_H__
#define __INCLUDED_SDK_VSCREEN_H__

#include <cstdint>
#include <vector>

namespace wwiv {
namespace sdk {
namespace ansi {

static constexpr int VSCREEN_DEFAULT_ATTRIBUTE = 0x07;

class VScreen {
public:
  explicit VScreen();
  virtual ~VScreen() = default;

  /** Writes c using a, handles \r and \n */
  virtual bool write(char c, uint8_t a) = 0;
  /** Moves the cursor to x,y */
  virtual bool gotoxy(int x, int y) = 0;
  // Like CLS, clears everything viewable.
  virtual bool clear() = 0;
  // clears from current position to the end of line.
  virtual bool clear_eol() = 0;
  // Finalizes this frambuffer and shrinks the size to fit, etc.
  virtual void close() = 0;

  /**
   * Puts c using a at cursor position pos.
   * Does not handle movement chars like \r or \n
   */
  virtual bool put(int pos, char c, uint8_t a) = 0;

  virtual bool write(char c) = 0;
  virtual void curatr(uint8_t a) = 0;

  [[nodiscard]] virtual int cols() const noexcept = 0;
  [[nodiscard]] virtual int pos() const noexcept = 0;
  [[nodiscard]] virtual uint8_t curatr() const noexcept = 0;
  [[nodiscard]] virtual int x() const noexcept = 0;
  [[nodiscard]] virtual int y() const noexcept = 0;

};

} // namespace ansi
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_VSCREEN_H__
