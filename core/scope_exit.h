/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2019, WWIV Software Services           */
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
#ifndef __INCLUDED_SCOPE_EXIT_H__
#define __INCLUDED_SCOPE_EXIT_H__

#include <functional>

namespace wwiv {
namespace core {

class ScopeExit {
public:
  ScopeExit() noexcept {}
  explicit ScopeExit(std::function<void()> fn) : fn_(fn) {}
  ~ScopeExit() { if (fn_) { fn_(); } }
  // Apparently msvc always has a valid target here since this doesn't go boom:
  // std::function<void(void)> f;
  // if (f) { FAIL("boom"); }
  // bool empty() const { if (fn_) return true; return false; }
  void swap(std::function<void()> fn) { fn_.swap(fn); }
private:
  std::function<void()> fn_;
};

}  // namespace core
}  // namespace wwiv

#endif // __INCLUDED_SCOPE_EXIT_H__
