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
#ifndef __INCLUDED_BBS_CONFUTIL_H__
#define __INCLUDED_BBS_CONFUTIL_H__

#include "bbs/conf.h"
#include "sdk/user.h"
#include <vector>

void setuconf(ConferenceType type, int num, int old_subnum);
void changedsl();

/*
 * Checks status of given userrec to see if conferencing is turned on.
 */
bool okconf(wwiv::sdk::User* pUser);

/*
 * Checks status of given userrec to see if conferencing is turned on 
 * *and* that multiple conferences are defined.
 */
bool ok_multiple_conf(wwiv::sdk::User* pUser, const std::vector<userconfrec>& uc);

/** Returns the real conference number for user conference at position uc */
int16_t userconf_to_subconf(int uc);

/** Returns the real conference number for user conference at position uc */
int16_t userconf_to_dirconf(int uc);

/** Returns true if a user conference exists at position uc */
bool has_userconf_to_subconf(int uc);

/** Returns true if a user conference exists at position uc */
bool has_userconf_to_dirconf(int uc);

#endif // __INCLUDED_BBS_CONFUTIL_H__