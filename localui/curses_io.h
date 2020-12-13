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
#ifndef INCLUDED_LOCALUI_CURSES_IO_H
#define INCLUDED_LOCALUI_CURSES_IO_H

#include "localui/curses_win.h"
#include "localui/colors.h"
#include <memory>
#include <vector>

// Indicator mode for the header bar while editing text.
enum class IndicatorMode { insert, overwrite, none };

struct HelpItem {
  std::string key;
  std::string description;
};

class CursesFooter final {
public:
  CursesFooter(CursesWindow* window, ColorScheme* color_scheme) 
    : window_(window), color_scheme_(color_scheme) {}
  ~CursesFooter() = default;
  void ShowHelpItems(int line, const std::vector<HelpItem>& help_items) const;
  void ShowContextHelp(const std::string& help_text) const;
  void SetDefaultFooter() const;
  [[nodiscard]] CursesWindow* window() const { return window_.get(); }

 private:
   std::unique_ptr<CursesWindow> window_;
   ColorScheme* color_scheme_;
};

// Curses implementation of screen display routines for wwivconfig.
class CursesIO final {
 public:
  // Constructor/Destructor
  explicit CursesIO(const std::string& title);
  CursesIO(const CursesIO& copy) = delete;
  ~CursesIO();
  // Initializes the DOS colors starting at 'start'.
  void InitDosColorPairs(short start = 100);
  void DisableLocalIO();

  void ReenableLocalIO();
  void Cls(uint32_t background_char = ' ');
  [[nodiscard]] CursesWindow* window() const { return window_.get(); }
  [[nodiscard]] CursesFooter* footer() const { return footer_.get(); }
  [[nodiscard]] CursesWindow* header() const { return header_.get(); }
  void SetIndicatorMode(IndicatorMode mode);

  [[nodiscard]] CursesWindow* CreateBoxedWindow(const std::string& title, int nlines, int ncols);

  [[nodiscard]] ColorScheme* color_scheme() const { return color_scheme_.get(); }
  static void Init(const std::string& title);
  [[nodiscard]] static CursesIO* Get();
  [[nodiscard]] int GetMaxX() const;
  [[nodiscard]] int GetMaxY() const;

 private:
  int max_x_;
  int max_y_;
  std::unique_ptr<CursesWindow> window_;
  std::unique_ptr<CursesFooter> footer_;
  std::unique_ptr<CursesWindow> header_;
  IndicatorMode indicator_mode_;
  std::unique_ptr<ColorScheme> color_scheme_;
};

extern CursesIO* curses_out;

#endif
