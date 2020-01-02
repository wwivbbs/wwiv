/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#ifndef __INCLUDED_PLATFORM_CURSES_WIN_H__
#define __INCLUDED_PLATFORM_CURSES_WIN_H__

#include "localui/colors.h"
#include "localui/ui_win.h"
#include <any>
#include <string>

// Curses implementation of screen display routines for wwivconfig.
class CursesWindow final : public UIWindow {
public:
  // Constructor/Destructor
  CursesWindow(CursesWindow* parent, ColorScheme* color_scheme, int nlines, int ncols,
               int begin_y = -1, int begin_x = -1);
  CursesWindow(const CursesWindow& copy) = delete;
  virtual ~CursesWindow();

  void SetTitle(const std::string& title) override;

  void Bkgd(uint32_t ch) override;
  int RedrawWin() override;
  int TouchWin() override;
  int Refresh() override;
  int Move(int y, int x) override;
  int GetcurX() const override;
  int GetcurY() const override;
  int Clear() override;
  int Erase() override;
  int AttrSet(uint32_t attrs) override;
  int Keypad(bool b) override;
  int GetMaxX() const override;
  int GetMaxY() const override;
  int ClrtoEol() override;
  int AttrGet(uint32_t* a, short* c) const override;
  int Box(uint32_t vert_ch, uint32_t horiz_ch) override;

  int GetChar() const override;
  void GotoXY(int x, int y) override;
  void Putch(uint32_t ch) override;
  void Puts(const std::string& text) override;
  void PutsXY(int x, int y, const std::string& text) override;

  void SetColor(SchemeId id) override;

  std::any window() const;
  CursesWindow* parent() const;

  bool IsGUI() const override;

private:
  std::any window_;
  CursesWindow* parent_;
  ColorScheme* color_scheme_;
};

#endif // __INCLUDED_PLATFORM_CURSES_WIN_H__
