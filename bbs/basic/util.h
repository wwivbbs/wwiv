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
#ifndef __INCLUDED_BBS_BASIC_UTIL_H__
#define __INCLUDED_BBS_BASIC_UTIL_H__

#include "bbs/output.h"
#include <string>

struct mb_interpreter_t;
// From interpret.h
class MacroContext;

namespace wwiv::bbs::basic {

char* BasicStrDup(const std::string& s);
void set_script_out(Output* o);

Output& script_out();

struct wwiv_script_userdata_t {
  std::string datadir;
  std::string script_dir;
  std::string module;
  const MacroContext* ctx;
};

/**
 * Gets the WWIV script "userdata" (wwiv_script_userdata_t) that was applied at the
 * script level for the interpreter session.
 */
wwiv_script_userdata_t* get_wwiv_script_userdata(struct mb_interpreter_t* bas);

}

#endif