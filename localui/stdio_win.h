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
#ifndef INCLUDED_LOCALUI_STDIO_WIN_H
#define INCLUDED_LOCALUI_STDIO_WIN_H

#include "localui/colors.h"
#include "localui/ui_win.h"
#include <string>

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif // INSERT

// Generic implementation of screen display routines for wwivconfig.
class StdioWindow : public UIWindow {
public:
  // Constructor/Destructor
  StdioWindow(UIWindow* parent, ColorScheme* color_scheme) : UIWindow(parent, color_scheme) {}
  StdioWindow(const UIWindow&) = delete;
  StdioWindow() = delete;
  StdioWindow(UIWindow&&) = delete;

  ~StdioWindow() override = default;

  void GotoXY(int, int) override {}

  // Still used by some curses code

  [[nodiscard]] int GetChar() const override;
  void Putch(uint32_t ch) override;
  void Puts(const std::string& text) override;
  void PutsXY(int x, int y, const std::string& text) override;

  /**
   * Returns true if this is a GUI mode UI vs. stdio based UI.
   */
  [[nodiscard]] bool IsGUI() const override { return false; }
};

#endif
