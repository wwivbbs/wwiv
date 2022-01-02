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
#ifndef INCLUDED_WWIVD_IPS_H
#define INCLUDED_WWIVD_IPS_H

#include "core/clock.h"
#include "sdk/wwivd_config.h"
#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wwiv::wwivd {

using namespace wwiv::core;

class GoodIp {
public:
  explicit GoodIp(const std::filesystem::path& fn);
  explicit GoodIp(const std::vector<std::string>& lines);
  [[nodiscard]] bool IsAlwaysAllowed(const std::string& ip);

private:
  [[nodiscard]] bool LoadLines(const std::vector<std::string>& ips);
  std::unordered_set<std::string> ips_;
};

class BadIp {
public:
  BadIp(const std::filesystem::path& fn, Clock& clock);
  [[nodiscard]] bool IsBlocked(const std::string& ip);
  bool Block(const std::string& ip);

private:
  const std::filesystem::path fn_;
  std::unordered_set<std::string> ips_;
  Clock& clock_;
};

struct auto_blocked_entry_t {
  int count{0};
  time_t expiration{0};
};

class AutoBlocker final {
public:
  AutoBlocker(std::shared_ptr<BadIp> bip, sdk::wwivd_blocking_t b, std::filesystem::path datadir, Clock& clock);
  ~AutoBlocker();
  void escalate_block(const std::string& ip);
  bool Connection(const std::string& ip);
  bool Save();

  // Used for testing
  const std::map<std::string, std::set<time_t>>& recent_sessions() const { return sessions_; }
  // Used for testing
  const std::map<std::string, auto_blocked_entry_t>& auto_blocked() const { return auto_blocked_; }

  [[nodiscard]] bool blocked(const std::string& ip) const;

private:
  bool Load();
  std::shared_ptr<BadIp> bip_;
  sdk::wwivd_blocking_t b_;
  std::filesystem::path datadir_;
  std::map<std::string, std::set<time_t>> sessions_;
  std::map<std::string, auto_blocked_entry_t> auto_blocked_;
  Clock& clock_;
  std::mutex mu_;
};

} // namespace

#endif
