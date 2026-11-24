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
#ifndef INCLUDED_BBS_BASIC_NEXT_TYPE_AHEAD_TEST_UTIL_H
#define INCLUDED_BBS_BASIC_NEXT_TYPE_AHEAD_TEST_UTIL_H

#include <string>

namespace wwiv::common {
class SessionContext;
}

// For tests that run scripts which read keys: the keys are typed ahead into
// bin, and once they run out the caller "hangs up" instead of the test
// waiting forever for real input.
namespace wwiv::bbs::basic::type_ahead {

// Hangs up both sessions once the typed-ahead keys run out. The script engine
// checks the context's session; Input checks the BBS's (a()->sess()).
void HangUpWhenKeysRunOut(common::SessionContext& a, common::SessionContext& b);

// Call from TearDown: stops hanging up, and throws away any keys left.
void StopTypingAhead();

// Types keys ahead, as if the caller had typed them.
void TypeAhead(const std::string& keys);

} // namespace wwiv::bbs::basic::type_ahead

#endif
