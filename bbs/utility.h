/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2017, WWIV Software Services            */
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

#include <ctime>
#include <string>
#include <vector>

#include "sdk/net.h"
#include "sdk/vardec.h"

void remove_from_temp(const std::string& file_name, const std::string& directory_name, bool bPrintStatus);
bool sysop1();
void ToggleScrollLockKey();
bool okansi();
void reset_disable_conf();
void frequent_init();
double ratio();
double post_ratio();
long nsl();
void send_net(net_header_rec* nh, std::vector<uint16_t> list, const std::string& text, const std::string& byname);
void giveup_timeslice();
std::string stripfn(const std::string& file_name);
char* stripfn(const char* file_name);
void stripfn_inplace(char *file_name);
char *get_wildlist(char *file_mask);
int side_menu(int *menu_pos, bool redraw, const std::vector<std::string>& menu_items, int xpos, int ypos, struct side_menu_colors * smc);
slrec getslrec(int nSl);
bool okfsed();
int ansir_to_flags(uint8_t ansir);

#endif  // __INCLUDED_UTILITY_H__

