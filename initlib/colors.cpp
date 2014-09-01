/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "colors.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "core/strings.h"

ColorScheme::ColorScheme() : scheme_(LoadColorSchemes()) { }

attr_t ColorScheme::GetAttributesForScheme(SchemeId id) const {
  const SchemeDescription& s = scheme_.at(id);
  attr_t attr = COLOR_PAIR(s.color_pair());
  if (s.bold()) {
    attr |= A_BOLD;
  }
  return attr;
}

// static 
std::map<SchemeId, SchemeDescription> ColorScheme::LoadColorSchemes() {
  std::map<SchemeId, SchemeDescription> scheme;

  // Create a default color scheme, ideally this would be loaded from an INI file or some other
  // configuration file, but this is a sufficient start.
  scheme[SchemeId::NORMAL] = SchemeDescription(SchemeId::NORMAL, COLOR_CYAN, COLOR_BLACK, false);
  scheme[SchemeId::ERROR_TEXT] = SchemeDescription(SchemeId::ERROR_TEXT, COLOR_RED, COLOR_BLACK, false);
  scheme[SchemeId::WARNING] = SchemeDescription(SchemeId::WARNING, COLOR_MAGENTA, COLOR_BLACK, true);
  scheme[SchemeId::HIGHLIGHT] = SchemeDescription(SchemeId::HIGHLIGHT, COLOR_CYAN, COLOR_BLACK, true);
  scheme[SchemeId::HEADER] = SchemeDescription(SchemeId::HEADER, COLOR_YELLOW, COLOR_BLUE, true);
  scheme[SchemeId::HEADER_COPYRIGHT] = SchemeDescription(SchemeId::HEADER_COPYRIGHT, COLOR_WHITE, COLOR_BLUE, true);
  scheme[SchemeId::FOOTER_KEY] = SchemeDescription(SchemeId::FOOTER_KEY, COLOR_YELLOW, COLOR_BLUE, true);
  scheme[SchemeId::FOOTER_TEXT] = SchemeDescription(SchemeId::FOOTER_TEXT, COLOR_CYAN, COLOR_BLUE, false);
  scheme[SchemeId::PROMPT] = SchemeDescription(SchemeId::PROMPT, COLOR_YELLOW, COLOR_BLACK, true);
  scheme[SchemeId::EDITLINE] = SchemeDescription(SchemeId::EDITLINE, COLOR_WHITE, COLOR_BLUE, true);
  scheme[SchemeId::INFO] = SchemeDescription(SchemeId::INFO, COLOR_CYAN, COLOR_BLACK, false);
  scheme[SchemeId::DIALOG_PROMPT] = SchemeDescription(SchemeId::DIALOG_PROMPT, COLOR_YELLOW, COLOR_BLUE, true);
  scheme[SchemeId::DIALOG_BOX] = SchemeDescription(SchemeId::DIALOG_BOX, COLOR_GREEN, COLOR_BLUE, true);
  scheme[SchemeId::DIALOG_TEXT] = SchemeDescription(SchemeId::DIALOG_TEXT, COLOR_CYAN, COLOR_BLUE, true);
  scheme[SchemeId::DIALOG_SELECTION] = SchemeDescription(SchemeId::DIALOG_SELECTION, COLOR_CYAN, COLOR_BLUE, true);
  scheme[SchemeId::WINDOW_BOX] = SchemeDescription(SchemeId::WINDOW_BOX, COLOR_CYAN, COLOR_BLACK, true);
  scheme[SchemeId::WINDOW_TEXT] = SchemeDescription(SchemeId::WINDOW_TEXT, COLOR_CYAN, COLOR_BLACK, false);
  scheme[SchemeId::WINDOW_SELECTED] = SchemeDescription(SchemeId::WINDOW_SELECTED, COLOR_CYAN , COLOR_BLACK, true);
  scheme[SchemeId::WINDOW_PROMPT] = SchemeDescription(SchemeId::WINDOW_PROMPT, COLOR_YELLOW, COLOR_BLACK, true);
  scheme[SchemeId::DESKTOP] = SchemeDescription(SchemeId::DESKTOP, COLOR_CYAN, COLOR_BLACK, false);
  scheme[SchemeId::WINDOW_DATA] = SchemeDescription(SchemeId::WINDOW_SELECTED, COLOR_YELLOW, COLOR_BLACK, true);
  scheme[SchemeId::WINDOW_TITLE] = SchemeDescription(SchemeId::WINDOW_TITLE, COLOR_YELLOW, COLOR_BLUE, true);

  // Create the color pairs for each of the colors defined in the color scheme.
  for (const auto& kv : scheme) {
    init_pair(kv.second.color_pair(), kv.second.f(), kv.second.b());
  }
  return scheme;
}
