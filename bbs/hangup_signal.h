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
#ifndef INCLUDED_BBS_HANGUP_SIGNAL_H
#define INCLUDED_BBS_HANGUP_SIGNAL_H

namespace wwiv::bbs {

/**
 * Makes SIGHUP and SIGTERM hang up the caller on sock: the signal shuts the
 * socket down, so the BBS sees the caller go away and logs them off as it
 * always does. wwivd sends SIGHUP to each node when it shuts down, and
 * systemd sends SIGTERM. Does nothing on Windows and OS/2.
 */
void HangupOnSignal(int sock);

/**
 * Forgets the socket, so a later SIGHUP or SIGTERM does nothing. Called
 * once the caller has been hung up, before the socket is closed and its
 * number can be reused. The signals stay caught rather than going back to
 * their defaults: wwivd and systemd can each send one while the BBS is
 * still logging the caller off. (Programs the BBS starts get the defaults
 * back, since caught signals are reset by exec.)
 */
void StopHangupOnSignal();

} // namespace wwiv::bbs

#endif
