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
#ifndef __INCLUDED_BBS_QUOTE_H__
#define __INCLUDED_BBS_QUOTE_H__

#include <string>
#include <deque>

#include "sdk/vardec.h"

void grab_quotes(messagerec* m, const std::string& aux, const std::string& to_name);
void clear_quotes();
void auto_quote(char *org, const std::string& to_name, long len, int type, time_t tDateTime);
std::deque<std::string> get_quote(const std::string& reply_to_name);

// [[ VisibleForTesting ]]
std::string GetQuoteInitials(const std::string& reply_to_name);


#endif  // __INCLUDED_BBS_QUOTE_H__