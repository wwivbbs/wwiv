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
#include "bbs/hangup_signal.h"

#ifdef __unix__
#include "core/log.h"
#include <csignal>
#include <cstring>
#include <sys/socket.h>
#endif

namespace wwiv::bbs {

#ifdef __unix__

// Read in the signal handler, so a lock-free atomic or sig_atomic_t only.
static volatile sig_atomic_t hangup_sock = -1;

static void on_hangup_signal(int) {
  // shutdown is async-signal-safe. The reader thread then sees end of
  // file, and the BBS hangs up.
  if (const int s = hangup_sock; s >= 0) {
    shutdown(s, SHUT_RDWR);
  }
}

void HangupOnSignal(int sock) {
  hangup_sock = sock;
  struct sigaction sa {};
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = on_hangup_signal;
  sa.sa_flags = SA_RESTART;
  for (const auto sig : {SIGHUP, SIGTERM}) {
    if (sigaction(sig, &sa, nullptr) == -1) {
      LOG(WARNING) << "Unable to install a hangup handler for " << strsignal(sig);
    }
  }
}

void StopHangupOnSignal() { hangup_sock = -1; }

#else

void HangupOnSignal(int) {}
void StopHangupOnSignal() {}

#endif

} // namespace wwiv::bbs
