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
/**************************************************************************/
#ifndef __INCLUDED_BBS_MMKEY_H__
#define __INCLUDED_BBS_MMKEY_H__

#include <string>
#include <set>

extern char
    mmkey_odc[81],
    mmkey_dc[81],
    mmkey_dcd[81],
    mmkey_dtc[81],
    mmkey_tc[81];


// todo(rush): make this a C++11 enum
std::string mmkey(std::set<char>& x, std::set<char>& xx, bool bListOption);
std::string mmkey(std::set<char>& x);

char *mmkey(int dl, bool bListOption = false);


#endif  // __INCLUDED_BBS_MMKEY_H__