/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_NEWUSER_H
#define INCLUDED_BBS_NEWUSER_H

#include "sdk/user.h"

#include <string>

void input_phone();
void input_dataphone();
void input_name();
void input_realname();
bool valid_phone(const std::string& phoneNumber);
void input_street();
void input_city();
void input_state();
void input_country();
void input_zipcode();
void input_sex();
void input_age(wwiv::sdk::User* u);
void input_comptype();
void input_screensize();
void input_pw(wwiv::sdk::User* u);
void input_ansistat();
void input_callsign();
void newuser();




#endif
