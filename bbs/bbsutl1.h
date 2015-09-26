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
#ifndef __INCLUDED_BBS_BBSUTL1_H__
#define __INCLUDED_BBS_BBSUTL1_H__

#include <string>

bool AllowLocalSysop();
void parse_email_info(const std::string& emailAddress, int *pUserNumber, int *pSystemNumber);
bool ValidateSysopPassword();
void hang_it_up();
bool play_sdf(const std::string& soundFileName, bool abortable);
std::string describe_area_code(int nAreaCode);
std::string describe_area_code_prefix(int nAreaCode, int town);


#endif  // __INCLUDED_BBS_BBSUTL1_H__
