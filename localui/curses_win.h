/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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

#include <map>
#include <string>
#include <curses.h>
#include "localui/colors.h"
#include "localui/ui_win.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

// Curses implementation of screen display routines for Init.
class CursesWindow : public UIWindow {
 public:
  // Constructor/Destructor
  CursesWindow(CursesWindow* parent, ColorScheme* color_scheme, int nlines, int ncols,
               int begin_y = -1, int begin_x = -1);
  CursesWindow(const CursesWindow& copy) = delete;
  virtual ~CursesWindow();

  void SetTitle(const std::string& title);

  int AddCh(chtype ch) override { return waddch(window_, ch); }
  int AddStr(const std::string s) override { return waddstr(window_, s.c_str()); }
  int MvAddStr(int y, int x, const std::string s) override { return mvwaddstr(window_, y, x, s.c_str()); }
  void Bkgd(chtype ch) override { wbkgd(window_, ch); }
  int RedrawWin() override { return redrawwin(window_); }
  int TouchWin() override { return touchwin(window_); }
  int Refresh() override { return wrefresh(window_); }
  int Move(int y, int x) override { return wmove(window_, y, x); }
  int GetcurX() const override { return getcurx(window_); }
  int GetcurY() const override { return getcury(window_); }
  int Clear() override { return wclear(window_); }
  int Erase() override { return werase(window_); }
  int GetChar() const override;
  int AttrSet(chtype attrs) override { return wattrset(window_, attrs); }
  int Keypad(bool b) override { return keypad(window_, b); }
  int GetMaxX() const override { return getmaxx(window_); }
  int GetMaxY() const override { return getmaxy(window_); }
  int ClrtoEol() override { return wclrtoeol(window_); }
  int AttrGet(attr_t* a, short* c) const override { return wattr_get(window_, a, c, nullptr); }
  int Box(chtype vert_ch, chtype horiz_ch) override { return box(window_, vert_ch, horiz_ch); }

  void GotoXY(int x, int y) override;
  void Putch(unsigned char ch) override;
  void Puts(const std::string& text) override;
  void PutsXY(int x, int y, const std::string& text) override;
  void Printf(const char* format, ...) override;
  void PrintfXY(int x, int y, const char* format, ...) override;

  void SetColor(SchemeId id) override;

  WINDOW* window() const { return window_; }
  CursesWindow* parent() const { return parent_; }
  ColorScheme* color_scheme() const { return color_scheme_; }
  SchemeId current_scheme_id() const { return current_scheme_id_; }

  virtual bool IsGUI() const override { return true; }


private:
  WINDOW* window_;
  CursesWindow* parent_;
  ColorScheme* color_scheme_;
  SchemeId current_scheme_id_;
};

#endif // __INCLUDED_PLATFORM_CURSES_WIN_H__
