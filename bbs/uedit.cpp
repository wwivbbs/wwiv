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
/*                                                                        */
/**************************************************************************/
#include "bbs/uedit.h"

#include "bbs/bbs.h"
#include "sdk/config.h"
#include "sdk/user.h"

void auto_val(int n, wwiv::sdk::User *u) {
  if (u->sl() == 255) {
    return;
  }
  const auto& avr = a()->config()->auto_val(n);
  u->sl(avr.sl);
  u->dsl(avr.dsl);
  u->ar_int(avr.ar);
  u->dar_int(avr.dar);
  u->restriction(avr.restrict);
}
