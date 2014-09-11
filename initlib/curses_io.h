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
#ifndef __INCLUDED_PLATFORM_CURSES_IO_H__
#define __INCLUDED_PLATFORM_CURSES_IO_H__

#include <map>
#include <memory>
#include <utility>
#include <curses.h>
#include "curses_win.h"
#include "colors.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

// Indicator mode for the header bar while editing text.
enum class IndicatorMode { INSERT, OVERWRITE, NONE };

struct HelpItem {
  std::string key;
  std::string description;
};

class CursesFooter {
public:
  CursesFooter(CursesWindow* window, ColorScheme* color_scheme) 
    : window_(window), color_scheme_(color_scheme) {}
  virtual ~CursesFooter() {}
  virtual void ShowHelpItems(int line, const std::vector<HelpItem>& help_items) const;
  virtual void ShowContextHelp(const std::string& help_text) const;
  virtual void SetDefaultFooter() const;
  virtual CursesWindow* window() const { return window_.get(); }

 private:
   std::unique_ptr<CursesWindow> window_;
   ColorScheme* color_scheme_;
};

// Curses implementation of screen display routines for Init.
class CursesIO {
 public:
  // Constructor/Destructor
  CursesIO();
  CursesIO(const CursesIO& copy);
  virtual ~CursesIO();

  virtual void Cls(chtype background_char = ' ');
  virtual CursesWindow* window() const { return window_; }
  virtual CursesFooter* footer() const { return footer_; }
  virtual CursesWindow* header() const { return header_; }
  virtual void SetIndicatorMode(IndicatorMode mode);

  virtual CursesWindow* CreateBoxedWindow(const std::string& title, int nlines, int ncols);

  ColorScheme* color_scheme() { return color_scheme_.get(); }
  static void Init();

 private:
  int max_x_;
  int max_y_;
  CursesWindow* window_;
  CursesFooter* footer_;
  CursesWindow* header_;
  IndicatorMode indicator_mode_;
  std::unique_ptr<ColorScheme> color_scheme_;
};

extern CursesIO* out;

#endif // __INCLUDED_PLATFORM_CURSES_IO_H__
