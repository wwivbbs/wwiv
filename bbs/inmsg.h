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
#ifndef __INCLUDED_BBS_INMSG_H__
#define __INCLUDED_BBS_INMSG_H__

#include <string>

#include "sdk/vardec.h"

void inmsg(messagerec * pMessageRecord, std::string* title, int *anony, bool needtitle, const char *aux, int fsed,
           const char *pszDestination, int flags, bool force_title = false);
void AddLineToMessageBuffer(char *pszMessageBuffer, const std::string& line_to_add, long *plBufferLength);

#endif  // __INCLUDED_BBS_INMSG_H__