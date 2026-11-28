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
#ifndef INCLUDED_WWIVD_SIGNALS_H
#define INCLUDED_WWIVD_SIGNALS_H

#include <atomic>

// What the signal and console handlers ask of wwivd. All of these are safe
// to call from a signal handler.
namespace wwiv::wwivd {

/** Asks wwivd to shut down. Returns true if that had already been asked. */
bool request_exit() noexcept;
/** The flag request_exit sets, for SocketSet::Run. */
std::atomic<bool>& exit_flag() noexcept;

/** Asks wwivd to reload its configuration (SIGHUP). */
void request_reload() noexcept;
/** Returns whether a reload was asked for since the last call. */
[[nodiscard]] bool take_reload_request() noexcept;

} // namespace wwiv::wwivd

#endif
