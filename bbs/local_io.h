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
#ifndef __INCLUDED_PLATFORM_LOCALIO_H__
#define __INCLUDED_PLATFORM_LOCALIO_H__

#include <string>

#include "bbs/keycodes.h"
#include "core/file.h"

// This C++ class should encompass all Local Input/Output from The BBS.
// You should use a routine in here instead of using printf, puts, etc.

class Application;

#define NUM_ONLY            1
#define UPPER_ONLY          2
#define ALL                 4
#define SET                 8

class LocalIO {
 public:
  // Constructor/Destructor
  LocalIO();
  LocalIO(const LocalIO& copy) = delete;
  virtual ~LocalIO();

  static constexpr int cursorNone = 0;
  static constexpr int cursorNormal = 1;
  static constexpr int cursorSolid = 2;

  static constexpr int topdataNone = 0;
  static constexpr int topdataSystem = 1;
  static constexpr int topdataUser = 2;

  const size_t GetTopLine() const { return topline_; }
  void SetTopLine(int nTopLine) { topline_ = nTopLine; }

  const size_t GetScreenBottom() const { return screen_bottom_; }
  void SetScreenBottom(int nScreenBottom) { screen_bottom_ = nScreenBottom; }

  virtual void GotoXY(int x, int y) = 0;
  virtual size_t WhereX() = 0;
  virtual size_t WhereY() = 0;
  virtual void Lf() = 0;
  virtual void Cr() = 0;
  virtual void Cls() = 0;
  virtual void ClrEol() = 0;
  virtual void Backspace() = 0;
  virtual void PutchRaw(unsigned char ch) = 0;
  // Overridden by TestLocalIO in tests.
  virtual void Putch(unsigned char ch) = 0;
  virtual void Puts(const std::string& text) = 0;
  virtual void PutsXY(int x, int y, const std::string& text) = 0;
  virtual int  Printf(const char *formatted_text, ...) = 0;
  virtual int  PrintfXY(int x, int y, const char *formatted_text, ...) = 0;
  virtual int  PrintfXYA(int x, int y, int nAttribute, const char *formatted_text, ...) = 0;
  virtual void set_protect(Application* session, int l) = 0;
  virtual void savescreen() = 0;
  virtual void restorescreen() = 0;
  virtual bool KeyPressed() = 0;
  virtual unsigned char GetChar() = 0;
  /*
   * MakeLocalWindow makes a "shadowized" window with the upper-left hand corner at
   * (x,y), and the lower-right corner at (x+xlen,y+ylen).
   */
  virtual void MakeLocalWindow(int x, int y, int xlen, int ylen) = 0;
  virtual void SetCursor(int cursorStyle) = 0;
  virtual void WriteScreenBuffer(const char *buffer) = 0;
  virtual size_t GetDefaultScreenBottom() = 0;
  virtual void EditLine(char *s, int len, int status, int *returncode, const char *ss) = 0;
  virtual void UpdateNativeTitleBar(Application* session) = 0;

  int  GetTopScreenColor() const { return top_screen_color_; }
  void SetTopScreenColor(int n) { top_screen_color_ = n; }

  int  GetUserEditorColor() const { return user_editor_color_; }
  void SetUserEditorColor(int n) { user_editor_color_ = n; }

  int  GetEditLineColor() const { return edit_line_color_; }
  void SetEditLineColor(int n) { edit_line_color_ = n; }

  virtual void DisableLocalIO() {}
  virtual void ReenableLocalIO() {}

private:
  virtual void FastPuts(const std::string& text) = 0;

private:
  int topline_ = 0;
  int screen_bottom_ = 0;
  int top_screen_color_ = 27;
  int user_editor_color_ = 9;
  int edit_line_color_ = 31;
};


#endif // __INCLUDED_PLATFORM_LOCALIO_H__
