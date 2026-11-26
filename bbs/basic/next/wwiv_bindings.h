/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#ifndef INCLUDED_BBS_BASIC_NEXT_WWIV_BINDINGS_H
#define INCLUDED_BBS_BASIC_NEXT_WWIV_BINDINGS_H

#include <string>

namespace wwiv::common {
class Context;
class Input;
class Output;
} // namespace wwiv::common

namespace wwiv::sdk {
class Config;
}

namespace wwivbasic {
class Console;
class Interpreter;
} // namespace wwivbasic

namespace wwiv::bbs::basic {

class MessageBase;
class UserBase;

// What the wwiv.* modules for the new engine use. Everything here must outlive
// the interpreter the modules are added to.
struct WwivBindings {
  common::Input& in;
  common::Output& out;
  // The script's console (a BbsConsole over in and out).
  wwivbasic::Console& console;
  const sdk::Config& config;
  common::Context* ctx;
  // The script's module name in lower case: its file name without ".bas", or
  // the name given to ScriptEngine::RunScript(module, text). wwiv.data names
  // its files after it, as the legacy engine does.
  std::string module;
  // What wwiv.msg reads, or nullptr for no wwiv.msg module.
  MessageBase* msgs{nullptr};
  // What wwiv.user reads, or nullptr for no wwiv.user module.
  UserBase* users{nullptr};
};

// The wwiv.* modules, with the same functions, arguments, return values and
// side effects as the legacy (my_basic) ones in bbs/basic/wwiv*.cpp.
void RegisterWwiv(wwivbasic::Interpreter& interp, const WwivBindings& b);
void RegisterWwivIo(wwivbasic::Interpreter& interp, const WwivBindings& b);
void RegisterWwivData(wwivbasic::Interpreter& interp, const WwivBindings& b);
// Registered only when the sysop enables them (see NextEngine).
void RegisterWwivIoFile(wwivbasic::Interpreter& interp, const WwivBindings& b);
void RegisterWwivOs(wwivbasic::Interpreter& interp, const WwivBindings& b);
void RegisterWwivTime(wwivbasic::Interpreter& interp, const WwivBindings& b);
// New in the new engine: read-only message areas. Registered when b.msgs is
// set and the sysop hasn't turned it off.
void RegisterWwivMsg(wwivbasic::Interpreter& interp, const WwivBindings& b);
// New in the new engine: the caller, and other users' public details.
// Registered when b.users is set and the sysop hasn't turned it off.
void RegisterWwivUser(wwivbasic::Interpreter& interp, const WwivBindings& b);
// New in the new engine: full-screen layouts and lightbar lists. Always on.
void RegisterWwivScreen(wwivbasic::Interpreter& interp, const WwivBindings& b);

} // namespace wwiv::bbs::basic

#endif
