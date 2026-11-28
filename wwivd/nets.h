/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
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
#ifndef INCLUDED_WWIVD_NETS_H
#define INCLUDED_WWIVD_NETS_H
#include <chrono>
#include <memory>
#include <string>

#include "sdk/config.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/wwivd_config.h"

namespace wwiv::wwivd {
class NodeManager;
class Sessions;

/**
 * Decides when the callout thread runs the beginday command. It runs when
 * the date in status.dat isn't today. If the command fails, or doesn't
 * update status.dat, it's tried again no sooner than retry_interval later
 * instead of on every pass of the loop.
 */
class BeginDayRetry {
public:
  explicit BeginDayRetry(std::chrono::system_clock::duration retry_interval =
                             std::chrono::hours(1))
      : retry_interval_(retry_interval) {}

  /** today is date(); last_date is status.dat's last date. */
  [[nodiscard]] bool should_run(const std::string& today, const std::string& last_date,
                                std::chrono::system_clock::time_point now) const;
  /** Records that the command was run for today at now. */
  void attempted(const std::string& today, std::chrono::system_clock::time_point now);

private:
  std::chrono::system_clock::duration retry_interval_;
  std::string attempted_date_;
  std::chrono::system_clock::time_point attempted_at_{};
};

/**
 * Starts the network callout and beginday thread on sessions. config must
 * outlive sessions.
 */
void do_wwivd_callouts(const sdk::Config& config, const sdk::wwivd_config_t& c,
                       std::shared_ptr<NodeManager> nodes, Sessions& sessions);

} // namespace

#endif
