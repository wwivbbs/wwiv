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
#ifndef __INCLUDED_PLATFORM_CURSESIO_H__
#define __INCLUDED_PLATFORM_CURSESIO_H__

#include <curses.h>

// Indicator mode for the header bar while editing text.
enum class IndicatorMode { INSERT, OVERWRITE, NONE };

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
  virtual void ClrEol();
  virtual void Putch(unsigned char ch);
  virtual void Puts(const char *pszText);
  virtual void PutsXY(int x, int y, const char *pszText);
  virtual int GetChar();
  virtual void Refresh();
  virtual WINDOW* window() const { return window_; }
  virtual WINDOW* footer() const { return footer_; }
  virtual WINDOW* header() const { return header_; }
  virtual void SetDefaultFooter();
  virtual void SetIndicatorMode(IndicatorMode mode);

 private:
  void SetCursesAttribute();
  int max_x_;
  int max_y_;
  WINDOW* window_;
  WINDOW* footer_;
  WINDOW* header_;
};

#endif // __INCLUDED_PLATFORM_CURSESIO_H__
