/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2026, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_BASIC_BASIC_H
#define INCLUDED_BBS_BASIC_BASIC_H

#include <string>

namespace wwiv::bbs::basic {

// Runs the WWIVbasic script scriptdir/script_name on wwivbasic (see
// bbs/basic/next/next_engine.h), with the wwiv.* modules the sysop enabled.
// Returns false if it fails or isn't found, or if this build has no script
// engine (WWIV_WWIVBASIC is off, as on OS/2).
bool RunBasicScript(const std::string& script_name);

} // namespace wwiv::bbs::basic

#endif
