/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2018-2022, WWIV Software Services             */
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
#include "sdk/net/callouts.h"

#include "core/log.h"
#include "core/numbers.h"

#include <algorithm>
#include <chrono>

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::sdk;

namespace wwiv::sdk::net {

network_callout_config_t to_network_callout_config_t(const net_call_out_rec& con) {
  network_callout_config_t c{};
  c.auto_callouts = !(con.options & options_no_call);
  c.call_every_x_minutes = con.call_every_x_minutes;
  c.min_k = con.min_k;
  return c;
}

bool allowed_to_call(const network_callout_config_t& con) { 
  return con.auto_callouts;
}

bool allowed_to_call(const net_call_out_rec& con, const DateTime& dt) {

  if (con.options & options_no_call) {
    VLOG(3) << "Not Calling: options_no_call";
    return false;
  }

  const auto l = con.min_hr;
  const auto h = con.max_hr;
  const auto n = dt.hour();
  if (l < 0 || h <= 0 || l == h) {
    return true;
  }
  if (l < h) {
    // Case where we want to allow it from say 0200-2000
    return n >= l && n < h;
  }
  // Case where we want to allow it from say 2000-0200
  return n >= h && n < l;
}

/**
 * Checks the net_contact_rec and net_call_out_rec to ensure the node specified
 * is ok to call and does not violate any constraints.
 */
bool should_call(const NetworkContact& ncn, const net_call_out_rec& con, const DateTime& dt) {
  const auto callout = to_network_callout_config_t(con);
  return should_call(ncn, callout, dt);
}

bool should_call(const NetworkContact& ncn, const network_callout_config_t& callout,
                 const DateTime& dt) {

  if (!allowed_to_call(callout)) {
    VLOG(2) << "!allowed_to_call; skipping";
    return false;
  }

  const auto now = dt.to_system_clock();
  if (ncn.bytes_waiting() == 0L && !callout.call_every_x_minutes) {
    VLOG(2) << "Skipping: No bytes waiting and !call_every_x_minutes";
    return false;
  }
  const auto min_minutes = std::max<int>(callout.call_every_x_minutes, 1);
  const auto last_contact = DateTime::from_time_t(ncn.lastcontact()).to_system_clock();
  const auto next_contact_time = last_contact + minutes(min_minutes);

  if (callout.call_every_x_minutes && now >= next_contact_time) {
    VLOG(1) << "Calling anyway since it's been time: ";
    VLOG(1) << "Last Try: " << DateTime::from_time_t(ncn.lastcontact()).to_string();
    return true;
  }
  if (callout.min_k > 0) {
    // We won't use min_k if it's 0, so only test if it's >0
    if (bytes_to_k<int>(ncn.bytes_waiting()) >= callout.min_k) {
      VLOG(1) << "Calling: min_k: " << humanize(ncn.bytes_waiting()) << " > " << callout.min_k;
      return true;
    }
  }
  VLOG(2) << "Skipping; No reason to call.";
  return false;
}


} // namespace wwiv
