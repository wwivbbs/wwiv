/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_CONFUTIL_H
#define INCLUDED_BBS_CONFUTIL_H

#include "sdk/user.h"
#include "sdk/conf/conf.h"
#include <vector>

bool clear_usersubs(wwiv::sdk::Conference& conf, std::vector<usersubrec>& uc, int old_subnum);
void setuconf(wwiv::sdk::Conference& conf, char key, int old_subnum);
void setuconf(wwiv::sdk::ConferenceType type, int num, int old_subnum);
void changedsl();

/*
 * Checks status of given userrec to see if conferencing is turned on.
 */
bool okconf(wwiv::sdk::User* user);

/*
 * Checks status of given userrec to see if conferencing is turned on 
 * *and* that multiple conferences are defined.
 */
bool ok_multiple_conf(wwiv::sdk::User* user, const std::vector<wwiv::sdk::conference_t>& uc);

/** Returns true if a user conference exists at position uc */
bool has_userconf_to_dirconf(int uc);

#endif
