/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
 *  auto on_exit = finally([=] { do_cleanup() });
 *  if (!foo()) {
 *    do_something()
 *    return false
 *  }
 *  do_something_else();
 *  return true;
 *  \endcode 
 */
template <class F = std::function<void()>> class ScopeExit final {
public:
  ScopeExit() noexcept = default;
  ScopeExit(const ScopeExit&) = delete;
  ScopeExit(ScopeExit&& other) noexcept {
    fn_.swap(other.fn_);
    invoke_ = other.invoke_;
  }

  ScopeExit& operator=(const ScopeExit&) = delete;
  ScopeExit& operator=(ScopeExit&& other) noexcept {
    fn_(std::move(other.fn_));
    invoke_ = other.invoke_;
    return *this;
  }

  /**
   * Construct a  scope guard with the exit function "<void()>".
   */
// GCC seems to display a warning on this, at least in 8.3.  MSVC can handle this in recent
// builds of the compiler.
#if defined(_MSC_VER)
  [[nodiscard]]
#endif // _MSC_VER
  explicit ScopeExit(F fn)
    : fn_(std::move(fn)), invoke_(true) {
  }

  ~ScopeExit() { if (invoke_) { fn_(); } }
  void swap(std::function<void()> fn) {
    invoke_ = false;
    fn_.swap(fn);
    if (fn_) {
      invoke_ = true;
    }
  }

private:
  F fn_;
  bool invoke_{false};
};

template <class F> [[nodiscard]] auto finally(F&& f) noexcept {
  return ScopeExit<std::decay_t<F>>{std::forward<F>(f)};
}
} // namespace

#endif
