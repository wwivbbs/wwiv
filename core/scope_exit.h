/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#ifndef INCLUDED_CORE_SCOPE_EXIT_H
#define INCLUDED_CORE_SCOPE_EXIT_H

#include <functional>
#include <utility>

namespace wwiv::core {

/**
 * \short 
 * A general-purpose scope guard to call the exit function <void()> when a scope
 * is exited, either by normal exit or an exception being thrown.
 * \note 
 * This class is not copyable, but is movable.
 *
 * Example use:
 * \code
 *  // We need to call do_cleanup after everything else is done.
 *  ScopeExit on_exit([=] { do_cleanup() });
 *  if (!foo()) {
 *    do_something()
 *    return false
 *  }
 *  do_something_else();
 *  return true;
 *  \endcode 
 */
class ScopeExit final {
public:
  ScopeExit() noexcept = default;
  ScopeExit(const ScopeExit&) = delete;
  ScopeExit(ScopeExit&& other) noexcept { fn_.swap(other.fn_); }

  ScopeExit& operator=(const ScopeExit&) = delete;
  ScopeExit& operator=(ScopeExit&& other) noexcept { fn_.swap(other.fn_); return *this; }

  /**
   * Construct a  scope guard with the exit function "<void()>".
   */
  [[nodiscard]] explicit ScopeExit(std::function<void()> fn)
    : fn_(std::move(fn)) {
  }

  ~ScopeExit() { if (fn_) { fn_(); } }
  void swap(std::function<void()> fn) { fn_.swap(fn); }
private:
  std::function<void()> fn_;
};

} // namespace

#endif
