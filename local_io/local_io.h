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
#ifndef INCLUDED_LOCAL_IO_LOCALIO_H
#define INCLUDED_LOCAL_IO_LOCALIO_H

#include "fmt/printf.h"
#include "local_io/curatr_provider.h"

#include <string>

namespace wwiv::local::io {

enum class AllowedKeys { NUM_ONLY, UPPER_ONLY, ALL, SET };

struct LocalIOData {
  int instance_number{0};
  std::string system_name;
};

enum class EditlineResult { PREV, NEXT, DONE, ABORTED };


/**
 * Encompasses all Local Input/Output from The BBS.
 * You should use a routine in here instead of using printf, puts, etc.
 */
class LocalIO {
public:
  // Constructor/Destructor
  LocalIO();
  LocalIO(const LocalIO& copy) = delete;
  virtual ~LocalIO();

  static constexpr int cursorNone = 0;
  static constexpr int cursorNormal = 1;
  static constexpr int cursorSolid = 2;

  enum class topdata_t { none, system, user };

  [[nodiscard]] int GetTopLine() const noexcept { return topline_; }
  void SetTopLine(int nTopLine) { topline_ = nTopLine; }

  [[nodiscard]] int GetScreenBottom() const noexcept { return screen_bottom_; }
  void SetScreenBottom(int nScreenBottom) { screen_bottom_ = nScreenBottom; }

  virtual void GotoXY(int x, int y) = 0;
  [[nodiscard]] virtual int WhereX() const noexcept = 0;
  [[nodiscard]] virtual int WhereY() const noexcept = 0;
  virtual void Lf() = 0;
  virtual void Cr() = 0;
  virtual void Cls() = 0;
  virtual void ClrEol() = 0;
  virtual void Backspace() = 0;
  virtual void PutchRaw(unsigned char ch) = 0;
  // Overridden by TestLocalIO in tests.
  /** Writes a single character 'ch' at the current position */
  virtual void Putch(unsigned char ch) = 0;
  /** Writes text at the current position */
  virtual void Puts(const std::string& text) = 0;

  template <class... Args> void Printf(const char* format_str, Args&&... args) {
    // Process arguments
    return Puts(fmt::sprintf(format_str, std::forward<Args>(args)...));
  }

  template <class... Args> void Format(const char* format_str, Args&&... args) {
    // Process arguments
    return Puts(fmt::format(format_str, std::forward<Args>(args)...));
  }

  /**
   * Writes text at position (x, y) using the current color.
   *
   * Note that x and y are zero based and (0, 0) is the top left corner of the screen.
   */
  virtual void PutsXY(int x, int y, const std::string& text) = 0;
  /**
   * Writes text at position (x, y) using the color 'attr'.
   *
   * Note that x and y are zero based and (0, 0) is the top left corner of the screen.
   */
  virtual void PutsXYA(int x, int y, int attr, const std::string& text) = 0;
  virtual void set_protect(int l) = 0;
  virtual void savescreen() = 0;
  virtual void restorescreen() = 0;
  virtual bool KeyPressed() = 0;
  virtual unsigned char GetChar() = 0;
  /*
   * MakeLocalWindow makes a "shadowed" window with the upper-left hand corner at
   * (x,y), and the lower-right corner at (x+xlen,y+ylen).
   */
  virtual void MakeLocalWindow(int x, int y, int xlen, int ylen) = 0;
  virtual void SetCursor(int cursorStyle) = 0;
  virtual void WriteScreenBuffer(const char* buffer) = 0;
  [[nodiscard]] virtual int GetDefaultScreenBottom() const noexcept = 0;
  virtual void EditLine(char* s, int len, AllowedKeys allowed_keys, EditlineResult* returncode,
                        const char* allowed_set_chars) = 0;
  virtual EditlineResult EditLine(std::string& s, int len, AllowedKeys allowed_keys,
                       const std::string& allowed_set_chars);
  virtual EditlineResult EditLine(std::string& s, int len, AllowedKeys allowed_keys);
  virtual void UpdateNativeTitleBar(const std::string& system_name, int instance_number) = 0;

  [[nodiscard]] uint8_t GetTopScreenColor() const noexcept { return top_screen_color_; }
  void SetTopScreenColor(uint8_t n) { top_screen_color_ = n; }

  [[nodiscard]] uint8_t GetUserEditorColor() const noexcept { return user_editor_color_; }
  void SetUserEditorColor(uint8_t n) { user_editor_color_ = n; }

  [[nodiscard]] uint8_t GetEditLineColor() const noexcept { return edit_line_color_; }
  void SetEditLineColor(uint8_t n) { edit_line_color_ = n; }

  virtual void DisableLocalIO() {}
  virtual void ReenableLocalIO() {}

  [[nodiscard]] topdata_t topdata() const noexcept { return topdata_; }
  void topdata(topdata_t t) { topdata_ = t; }
  void increment_topdata();

  // curatr_provider interface
  virtual void set_curatr_provider(wwiv::local::io::curatr_provider* p);
  virtual wwiv::local::io::curatr_provider* curatr_provider();
  [[nodiscard]] virtual uint8_t curatr() const;
  virtual void curatr(int c);

private:
  virtual void FastPuts(const std::string& text) = 0;

  int topline_{0};
  int screen_bottom_{25}; // Just a default.
  uint8_t top_screen_color_{27};
  uint8_t user_editor_color_{9};
  uint8_t edit_line_color_{31};
  wwiv::local::io::curatr_provider* curatr_;
  topdata_t topdata_{topdata_t::none};
};

}

#endif