/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
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
#include "wwivd/signals.h"

#include <atomic>

namespace wwiv::wwivd {

// Lock-free atomics, so the handlers may touch them.
static std::atomic<bool> exit_requested{false};
static std::atomic<bool> reload_requested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

bool request_exit() noexcept { return exit_requested.exchange(true); }

std::atomic<bool>& exit_flag() noexcept { return exit_requested; }

void request_reload() noexcept { reload_requested.store(true); }

bool take_reload_request() noexcept { return reload_requested.exchange(false); }

} // namespace wwiv::wwivd
