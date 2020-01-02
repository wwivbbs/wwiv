/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_BBSUTL_H__
#define __INCLUDED_BBS_BBSUTL_H__

#include <string>
#include "sdk/user.h"

bool inli(std::string* outBuffer, std::string* rollover, std::string::size_type maxlen, bool add_crlf = true,
  bool bAllowPrevious = false, bool two_color_chatmode = false, bool clear_previous_line = false);
bool inli(char *buffer, char *rollover, std::string::size_type maxlen, bool add_crlf = true,
  bool allow_previous = false, bool two_color = false, bool clear_previous_line = false);
bool so();
bool cs();
bool lcs();
bool checka();
bool checka(bool *abort);
bool checka(bool *abort, bool *next);
bool sysop2();
int  check_ansi();
bool set_language_1(int n);
bool set_language(int n);
const char *YesNoString(bool bYesNo);
bool okconf(wwiv::sdk::User *pUser);
void* BbsAllocA(size_t num_bytes);

#endif  // __INCLUDED_BBS_BBSUTL_H__