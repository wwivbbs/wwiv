/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
/**************************************************************************/
#ifndef __INCLUDED_BBS_BBSOVL3_H__
#define __INCLUDED_BBS_BBSOVL3_H__

#include <string>

int  get_kb_event(int nNumLockMode);
char onek_ncr(const char *pszAllowableChars);
bool do_sysop_command(int command);
bool copyfile(const std::string& sourceFileName, const std::string& destFileName, bool stats);
bool movefile(const std::string& sourceFileName, const std::string& destFileName, bool stats);
void ListAllColors();


#endif  // __INCLUDED_BBS_BBSOVL3_H__
