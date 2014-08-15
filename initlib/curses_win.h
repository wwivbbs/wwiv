/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

// Curses implementation of screen display routines for Init.
class CursesWindow {

 public:
  // Constructor/Destructor
  CursesWindow(int nlines, int ncols, int begin_y, int begin_x);
  CursesWindow(const CursesWindow& copy) = delete;
  virtual ~CursesWindow();

  int addch(chtype ch) { return ::waddch(window_, ch); }
  int addstr(const std::string s) { return ::waddstr(s.c_str()); }
  int mvaddstr(int y, int x, const std::string s) { return ::mvwaddstr(window_, y, x, s.c_str(); }
  void bkgd(chtype ch) { ::wbkgd(window_, ch); }
  int redrawwin() { return ::redrawwin(window_); } 
  int touchwin() { return ::touchwin(window_); }
  int refresh() { return ::wrefresh(window_); }
  int move(int y, int x) { return ::wmove(window_, y, x); }
  int getcurx() { return ::getcurx(window_); }
  int getcury() { return ::getcury(window_); }
  int clear() { return ::wclear(window_); }
  int getchar() { return ::wgetch(window_); }
  int attrset(chtype attrs) { return ::wattrset(window_, attrs); }

 private:
  int max_x_;
  int max_y_;
  WINDOW* window_;
};

#endif // __INCLUDED_PLATFORM_CURSES_WIN_H__
