/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_LOCALIO_SCREEN_H__
#define __INCLUDED_SDK_LOCALIO_SCREEN_H__

#include "local_io/curatr_provider.h"
#include "local_io/local_io.h"
#include "sdk/ansi/vscreen.h"
#include <cstdint>
#include <memory>

namespace wwiv {
namespace sdk {
namespace ansi {

class LocalIOScreen : public VScreen {
public:
  explicit LocalIOScreen(LocalIO* io, int cols);
  virtual ~LocalIOScreen() = default;

  /** Writes c using a, handles \r and \n */
  bool write(char c, uint8_t a) override {
    return write(c);
  }
  /** Moves the cursor to x,y */
  bool gotoxy(int x, int y) override {
    io_->GotoXY(x, y);
    return true;
  }
  // Like CLS, clears everything viewable.
  bool clear() override {
    io_->Cls();
    return true;
  }
  // clears from current position to the end of line.
  bool clear_eol() override {
    io_->ClrEol();
    return true;
  }
  // Finalizes this frambuffer and shrinks the size to fit, etc.
  void close() override {}

  /**
   * Puts c using a at cursor position pos.
   * Does not handle movement chars like \r or \n
   */
  bool put(int pos, char c, uint8_t a) override {
    const auto x = pos % cols_;
    const auto y = pos / cols_;
    io_->PutsXYA(x, y, a, std::string(1, c));
    return true;
  }

  bool write(char c) override {
    io_->Putch(c);
    return true;
  }

  void curatr(uint8_t a) override { curatr_provider_->curatr(a); }

  [[nodiscard]] int cols() const noexcept override { return cols_; }
  [[nodiscard]] int pos() const noexcept override { return (y() * cols()) + x(); }
  [[nodiscard]] uint8_t curatr() const noexcept override { return curatr_provider_->curatr(); }
  [[nodiscard]] int x() const noexcept override { return io_->WhereX(); }
  [[nodiscard]] int y() const noexcept override { return io_->WhereY(); }

private:
  LocalIO* io_;
  int cols_;
  std::unique_ptr<wwiv::local_io::curatr_provider> curatr_provider_;
};

} // namespace ansi
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_LOCALIO_SCREEN_H__
