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
#ifndef __INCLUDED_LOCALUI_STDIO_WIN_H__
#define __INCLUDED_LOCALUI_STDIO_WIN_H__

#include <map>
#include <string>
#include <curses.h>
#include "localui/colors.h"
#include "localui/ui_win.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

// Generic implementation of screen display routines for Init.
class StdioWindow : public UIWindow {
 public:
  // Constructor/Destructor
  StdioWindow(UIWindow* parent,
              ColorScheme* color_scheme) : UIWindow(parent, color_scheme) {}
  StdioWindow(const UIWindow& copy) = delete;
  virtual ~StdioWindow() {}

  virtual void GotoXY(int, int) {}

  // Still used by some curses code
  virtual int AddCh(chtype) override;
  virtual int AddStr(const std::string) override;
  virtual int MvAddStr(int, int, const std::string) override;

  virtual int GetChar() const;
  virtual void Putch(unsigned char ch);
  virtual void Puts(const std::string& text);
  virtual void PutsXY(int x, int y, const std::string& text);
  virtual void Printf(const char* format, ...);
  virtual void PrintfXY(int x, int y, const char* format, ...);

  /**
   * Returns true if this is a GUI mode UI vs. stdio based UI.
   */
  virtual bool IsGUI() const { return false; }
};

#endif // __INCLUDED_LOCALUI_STDIO_WIN_H__
