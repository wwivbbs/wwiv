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
#ifndef __INCLUDED_BBS_READ_MESSAGE_H__
#define __INCLUDED_BBS_READ_MESSAGE_H__

#include "sdk/vardec.h"

void read_type2_message(messagerec * pMessageRecord, char an, bool readit,
                        bool *next, const char *file_name, int nFromSystem,
                        int nFromUser);

void read_post(int n, bool *next, int *val);

#endif  // __INCLUDED_BBS_READ_MESSAGE_H__