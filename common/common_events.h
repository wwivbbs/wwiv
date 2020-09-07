/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2020, WWIV Software Services             */
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
#ifndef __INCLUDED_COMMON_EVENTS_H__
#define __INCLUDED_COMMON_EVENTS_H__

#include <chrono>
#include <cstdint>

namespace wwiv::common {

// The BBS Should attempt to process inter-instance messages.
struct ProcessInstanceMessages {};

// The BBS Should attempt to process inter-instance messages.
struct ResetProcessingInstanceMessages {};

// The BBS Should attempt to process inter-instance messages.
struct PauseProcessingInstanceMessages {};

// The BBS Should check if the user hungup.
struct CheckForHangupEvent {};

// Notify the BBS that the user has hungup, forcefully if needed.
struct HangupEvent {};

// Needs to update the topscreen on local IO.
struct UpdateTopScreenEvent {};

// Needs to update the time left in the BBS and optionally
// check for a timeout.
struct UpdateTimeLeft {
  bool check_for_timeout{false};
};

// Needs to handle a sysop key pressed locally. This is the second key
// of the two key sequence returned from Win32 for extended input.
struct HandleSysopKey {
  uint8_t key{0};
};

// Should giveup timeslices to other processes
// The BBS Should attempt to process inter-instance messages.
struct GiveupTimeslices {};

// Display the time on and time left.
struct DisplayTimeLeft {};

// Display the instance status in a multi-instance system.
struct DisplayMultiInstanceStatus {};

// Display the invisible status for multi-node chat
struct ToggleInvisble {};

// Display the available status for multi-node chat
struct ToggleAvailable {};

} // namespace wwiv::common

#endif