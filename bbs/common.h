/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#ifndef __INCLUDED_COMMON_H__
#define __INCLUDED_COMMON_H__

#include <string>
#include "bbs/wwivcolors.h"

constexpr int WWIV_LISTPLUS_NORMAL_HIGHLIGHT = (YELLOW + (BLACK << 4));
constexpr int WWIV_LISTPLUS_NORMAL_MENU_ITEM = (CYAN + (BLACK << 4));
constexpr int WWIV_LISTPLUS_CURRENT_HIGHLIGHT = (RED + (LIGHTGRAY << 4));
constexpr int WWIV_LISTPLUS_CURRENT_MENU_ITEM = (BLACK + (LIGHTGRAY << 4));


struct side_menu_colors {
  int normal_highlight = WWIV_LISTPLUS_NORMAL_HIGHLIGHT;
  int normal_menu_item = WWIV_LISTPLUS_NORMAL_MENU_ITEM;
  int current_highlight = WWIV_LISTPLUS_CURRENT_HIGHLIGHT;
  int current_menu_item = WWIV_LISTPLUS_CURRENT_MENU_ITEM;
};

struct search_record {
  std::string filemask;
  uint32_t nscandate = 0;
  std::string search;

  int alldirs = 0;
  int search_extended = 0;
};

constexpr int HOTKEYS_ON = 0;
constexpr int HOTKEYS_OFF = 1;

#endif // __INCLUDED_COMMON_H__
