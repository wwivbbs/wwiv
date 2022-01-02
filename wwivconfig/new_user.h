/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2022, WWIV Software Services           */
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
#ifndef INCLUDED_WWIVCONFIG_NEW_USER_H
#define INCLUDED_WWIVCONFIG_NEW_USER_H

// Minimal New User is:
// Country first then:
// Alias, Birthday, Gender, Zip, (city/state filled in or prompted), then email.
// Example:
/*  Mystic Rhythms BBS New User Registration

[A] Name (real or alias)    : RNEW
[B] Birth Date (MM/DD/YYYY) : January 01, 1980 (41 years old)
[C] Sex (Gender)            : Male
[D] Country                 : USA
[E] ZIP or Postal Code      : 94105
[F] City/State/Province     : San Francisco, CA
[G] Internet Mail Address   : rushfan@me.com

Item to change or [Q] to Quit :
*/

// Full Example:
/*
Enter your country (i.e. USA).
Hit Enter for "USA"
:USA

Enter your full name, or your alias.
RNEW2

Enter your FULL real name.
Rob Cole

Enter your VOICE phone no. in the form:
 ###-###-####
:415-555-1212

 Enter your amateur radio callsign, or just hit <ENTER> if none.
:*/

namespace wwiv {
namespace local {
namespace ui {
class CursesWindow;
}
}

namespace sdk {
class Config;
struct newuser_config_t;
}
}

void newuser_settings(wwiv::sdk::Config& config, wwiv::sdk::newuser_config_t& nc,
                      wwiv::local::ui::CursesWindow*);

#endif // INCLUDED_WWIVCONFIG_NEW_USER_H