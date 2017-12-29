/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "core/wwivport.h"
#include "sdk/wwivcolors.h"

// Defines for listplus
constexpr int LP_LIST_DIR = 0;
constexpr int LP_SEARCH_ALL = 1;
constexpr int LP_NSCAN_DIR = 2;
constexpr int LP_NSCAN_NSCAN = 3;

constexpr int ALL_DIRS = 0;
constexpr int THIS_DIR = 1;
constexpr int NSCAN_DIRS = 2;

constexpr int WWIV_LISTPLUS_NORMAL_HIGHLIGHT = 
  (static_cast<uint8_t>(wwiv::sdk::Color::YELLOW) + (static_cast<uint8_t>(wwiv::sdk::Color::BLACK) << 4));
constexpr int WWIV_LISTPLUS_NORMAL_MENU_ITEM = 
  (static_cast<uint8_t>(wwiv::sdk::Color::CYAN) + (static_cast<uint8_t>(wwiv::sdk::Color::BLACK) << 4));
constexpr int WWIV_LISTPLUS_CURRENT_HIGHLIGHT = 
  (static_cast<uint8_t>(wwiv::sdk::Color::RED) + (static_cast<uint8_t>(wwiv::sdk::Color::LIGHTGRAY) << 4));
constexpr int WWIV_LISTPLUS_CURRENT_MENU_ITEM = 
  (static_cast<uint8_t>(wwiv::sdk::Color::BLACK) + (static_cast<uint8_t>(wwiv::sdk::Color::LIGHTGRAY) << 4));


struct side_menu_colors {
  int normal_highlight = WWIV_LISTPLUS_NORMAL_HIGHLIGHT;
  int normal_menu_item = WWIV_LISTPLUS_NORMAL_MENU_ITEM;
  int current_highlight = WWIV_LISTPLUS_CURRENT_HIGHLIGHT;
  int current_menu_item = WWIV_LISTPLUS_CURRENT_MENU_ITEM;
};

struct search_record {
  std::string filemask;
  daten_t nscandate = 0;
  std::string search;

  int alldirs = 0;
  bool search_extended = false;
};

#endif // __INCLUDED_COMMON_H__
