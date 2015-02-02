/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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
#ifndef __INCLUDED_PLATFORM_COLORS_H__
#define __INCLUDED_PLATFORM_COLORS_H__

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <curses.h>

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT


// Color Scheme
enum class SchemeId { 
  NORMAL=1, ERROR_TEXT, WARNING, HIGHLIGHT, HEADER, HEADER_COPYRIGHT, 
  FOOTER_KEY, FOOTER_TEXT, 
  PROMPT, EDITLINE, INFO, 
  DIALOG_PROMPT, DIALOG_BOX, DIALOG_TEXT, DIALOG_SELECTION,
  WINDOW_BOX, WINDOW_TEXT, WINDOW_SELECTED, WINDOW_PROMPT,
  WINDOW_DATA, WINDOW_TITLE,
  DESKTOP,
  UNKNOWN
};

// Describes a color scheme.
class SchemeDescription {
public:
  SchemeDescription(SchemeId scheme, int f, int b, bool bold) 
    : scheme_(scheme), f_(f), b_(b), bold_(bold) {}

  // Make the unknown scheme magenta on a red background to make it easy to spot.
  SchemeDescription(): scheme_(SchemeId::UNKNOWN), f_(COLOR_MAGENTA), b_(COLOR_RED), bold_(true) {}

  // Don't provide a user defined destructor since that will block move semantics

  int color_pair() const { return static_cast<int>(scheme_); }
  int f() const { return f_; }
  int b() const { return b_; }
  bool bold() const { return bold_; }

private:
  SchemeId scheme_;
  int f_;
  int b_;
  bool bold_;
};


// Curses implementation of a list box.
class ColorScheme {
 public:
  ColorScheme();
  virtual ~ColorScheme() {}
  virtual attr_t GetAttributesForScheme(SchemeId id) const;
  ColorScheme& operator=(const ColorScheme&) = delete;
private:
  static std::map<SchemeId, SchemeDescription> LoadColorSchemes();
  const std::map<SchemeId, SchemeDescription> scheme_;
};
#endif // __INCLUDED_PLATFORM_COLORS_H__
