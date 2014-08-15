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
#include <curses.h>
#include "curses_win.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

// Indicator mode for the header bar while editing text.
enum class IndicatorMode { INSERT, OVERWRITE, NONE };

// Color Scheme
enum class Scheme { 
  NORMAL=1, ERROR_TEXT, WARNING, HIGHLIGHT, HEADER, HEADER_COPYRIGHT, 
  FOOTER_KEY, FOOTER_TEXT, 
  PROMPT, EDITLINE, INFO, 
  DIALOG_PROMPT, DIALOG_BOX, DIALOG_TEXT,
  UNKNOWN
};

// Describes a color scheme.
class SchemeDescription {
public:
  SchemeDescription(Scheme scheme, int f, int b, bool bold) 
    : scheme_(scheme), f_(f), b_(b), bold_(bold) {}

  // Make the unknown scheme magenta on a red background to make it easy to spot.
  SchemeDescription(): scheme_(Scheme::UNKNOWN), f_(COLOR_MAGENTA), b_(COLOR_RED), bold_(true) {}

  // Don't provide a user defined destructor since that will block move semantics
  // virtual ~SchemeDescription() {}

  int color_pair() const { return static_cast<int>(scheme_); }
  int f() const { return f_; }
  int b() const { return b_; }
  bool bold() const { return bold_; }

private:
  Scheme scheme_;
  int f_;
  int b_;
  bool bold_;
};

// Curses implementation of screen display routines for Init.
class CursesIO {

 public:
  // Constructor/Destructor
  CursesIO();
  CursesIO(const CursesIO& copy);
  virtual ~CursesIO();

  virtual void GotoXY(int x, int y);
  virtual int  WhereX();
  virtual int  WhereY();
  virtual void Cls();
  virtual void Putch(unsigned char ch);
  virtual void Puts(const char *pszText);
  virtual void PutsXY(int x, int y, const char *pszText);
  virtual int GetChar();
  virtual void Refresh();
  virtual CursesWindow* window() const { return window_; }
  virtual CursesWindow* footer() const { return footer_; }
  virtual CursesWindow* header() const { return header_; }
  virtual void SetDefaultFooter();
  virtual void SetIndicatorMode(IndicatorMode mode);
  virtual attr_t GetAttributesForScheme(Scheme id);
  virtual void SetColor(CursesWindow* window, Scheme scheme);
  virtual void SetColor(Scheme scheme) { SetColor(window_, scheme); }

  static void Init();

 private:
  static std::map<Scheme, SchemeDescription> LoadColorSchemes();

  int max_x_;
  int max_y_;
  CursesWindow* window_;
  CursesWindow* footer_;
  CursesWindow* header_;
  std::map<Scheme, SchemeDescription> scheme_;
  IndicatorMode indicator_mode_;
};


extern CursesIO* out;


#endif // __INCLUDED_PLATFORM_CURSES_IO_H__
