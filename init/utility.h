/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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

#include "bbs/vardec.h"

int number_userrecs();
int exist(const char *s);
void save_status();
bool read_status();
int save_config();
void read_user(unsigned int un, userrec *u);
void write_user(unsigned int un, userrec *u);
void exit_init(int level);
void trimstr(char *s);
void trimstrpath(char *s);

#endif // __INCLUDED_UTILITY_H__
