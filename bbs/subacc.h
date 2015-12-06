/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#ifndef __INCLUDED_BBS_SUBACC_H__
#define __INCLUDED_BBS_SUBACC_H__

#include <cstdint>
#include "sdk/vardec.h"

void close_sub();
bool open_sub(bool wr);
uint32_t WWIVReadLastRead(int sub_number);
bool iscan1(int si);
int iscan(int b);
postrec *get_post(int mn);
void delete_message(int mn);
void write_post(int mn, postrec * pp);
void add_post(postrec * pp);
void resynch(int *msgnum, postrec * pp);
void pack_all_subs();
void pack_sub(int si);

#endif  // __INCLUDED_BBS_SUBACC_H__