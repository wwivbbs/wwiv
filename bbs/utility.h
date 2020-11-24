/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2020, WWIV Software Services            */
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
#ifndef __INCLUDED_UTILITY_H__
#define __INCLUDED_UTILITY_H__

#include "sdk/net/net.h"
#include <string>
#include <vector>

void remove_from_temp(const std::string& file_name, const std::filesystem::path& directory_name,
                      bool bPrintStatus);
float ratio();
float post_ratio();
void reset_disable_conf();
bool okansi();
long nsl();
void send_net(net_header_rec* nh, std::vector<uint16_t> list, const std::string& text, const std::string& byname);
std::string stripfn(const std::string& file_name);
std::string get_wildlist(const std::string& file_mask);
int side_menu(int *menu_pos, bool redraw, const std::vector<std::string>& menu_items, int xpos, int ypos, struct side_menu_colors * smc);
/** True if an external fsed is defined and user has ansi */
bool ok_external_fsed();
/** True if an external or internal fsed is allowed */
bool okfsed();
/** True if an internal fsed is enabled and user has ansi */
bool ok_internal_fsed();
int ansir_to_flags(uint16_t ansir);

#endif  // __INCLUDED_UT`ILITY_H__

