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
#ifndef __INCLUDED_LOCALUI_UI_WIN_H__
#define __INCLUDED_LOCALUI_UI_WIN_H__

#include <map>
#include <string>
#include "colors.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

// Generic implementation of screen display routines for wwivconfig.
class UIWindow {
 public:
  // Constructor/Destructor
  UIWindow(UIWindow* parent,
           ColorScheme* color_scheme) : parent_(parent), color_scheme_(color_scheme) {}
  UIWindow(const UIWindow& copy) = delete;
  virtual ~UIWindow() {}

  virtual void SetColor(SchemeId) {}
  virtual void SetTitle(const std::string&) {}
  virtual void Bkgd(uint32_t) {}
  virtual int RedrawWin() { return 0; }
  virtual int TouchWin() { return 0; }
  virtual int Refresh() { return 0; }
  virtual int Move(int, int) { return 0; }
  virtual int GetcurX() const { return 0; }
  virtual int GetcurY() const { return 0; }
  virtual int Clear() { return 0; }
  virtual int Erase() { return 0; }
  virtual int AttrSet(uint32_t) { return 0; }
  virtual int Keypad(bool) { return 0; }
  virtual int GetMaxX() const { return 0; }
  virtual int GetMaxY() const { return 0; }
  virtual int ClrtoEol() { return 0; }
  virtual int AttrGet(uint32_t*, short*) const { return 0; }
  virtual int Box(uint32_t, uint32_t) { return 0; }

  virtual int GetChar() const = 0;
  virtual void GotoXY(int x, int y) = 0;
  virtual void Putch(uint32_t ch) = 0;
  virtual void Puts(const std::string& text) = 0;
  virtual void PutsXY(int x, int y, const std::string& text) = 0;
  
  /**
   * Returns true if this is a GUI mode UI vs. stdio based UI.
   */
  virtual bool IsGUI() const { return false; }

  UIWindow* parent() const { return parent_; }
  SchemeId current_scheme_id() const { return current_scheme_id_; }

protected:
  void set_current_scheme_id(SchemeId id) { current_scheme_id_ = id; }

private:
  UIWindow* parent_;
  ColorScheme* color_scheme_;
  SchemeId current_scheme_id_ {SchemeId::UNKNOWN};
};

#endif // __INCLUDED_LOCALUI_UI_WIN_H__
