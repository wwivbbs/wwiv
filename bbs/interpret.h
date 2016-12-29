/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_INTERPRET_H__
#define __INCLUDED_BBS_INTERPRET_H__

#include <string>

#include "sdk/user.h"
#include "sdk/vardec.h"

// for a()
#include "bbs/bbs.h"

class MacroContext {
public:
  virtual const wwiv::sdk::User& u() const = 0;
  virtual const directoryrec& dir() const = 0;
};

class BbsMacroContext : public MacroContext {
public:
  BbsMacroContext(wwiv::sdk::User* u) : u_(u) {}
  const wwiv::sdk::User& u() const override { return *u_; }
  const directoryrec& dir() const { return a()->current_dir(); }

private:
  wwiv::sdk::User* u_ = nullptr;
};

std::string interpret(char chKey, const MacroContext& context);


#endif  // __INCLUDED_BBS_INTERPRET_H__
