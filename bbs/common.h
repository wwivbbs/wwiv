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

#pragma pack(push, 1)


struct side_menu_colors {
  int normal_highlight;   // used to all be unsigned.
  int normal_menu_item;
  int current_highlight;
  int current_menu_item;
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

struct user_config {
  char name[31];          // verify against a user

  unsigned long unused_status;

  unsigned long lp_options;
  unsigned char lp_colors[32];

  char szMenuSet[9];   // Selected AMENU set to use
  char cHotKeys;       // Use hot keys in AMENU

  char junk[119];   // AMENU took 11 bytes from here
};

#pragma pack(pop)

#endif // __INCLUDED_COMMON_H__
