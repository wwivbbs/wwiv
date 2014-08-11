/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                Copyright (C)2014, WWIV Software Services               */
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

#include "net.h"
#include "vardec.h"

void remove_from_temp(const char *pszFileName, const char *pszDirectoryName, bool bPrintStatus);
bool sysop1();
void ToggleScrollLockKey();
bool okansi();
void reset_disable_conf();
void frequent_init();
double ratio();
double post_ratio();
double nsl();
void Wait(double d);
double freek1(const char *pszPathName);
void send_net(net_header_rec * nh, unsigned short int *list, const char *text, const char *byname);
void giveup_timeslice();
char *stripfn(const char *pszFileName);
void stripfn_inplace(char *pszFileName);
void preload_subs();
char *get_wildlist(char *pszFileMask);
int side_menu(int *menu_pos, bool redraw, char *menu_items[], int xpos, int ypos, struct side_menu_colors * smc);
slrec getslrec(int nSl);
void WWIV_SetFileTime(const char* pszFileName, const time_t tTime);
bool okfsed();
std::string W_DateString(time_t tDateTime, const std::string& origMode , const std::string& timeDelim);

#endif  // __INCLUDED_UTILITY_H__

