/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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

#include "sdk/wwivd_config.h"
#include <ctime>
#include <filesystem>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wwiv::wwivd {

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
  explicit BadIp(const std::filesystem::path& fn);
  [[nodiscard]] bool IsBlocked(const std::string& ip);
  bool Block(const std::string& ip);

private:
  const std::filesystem::path fn_;
  std::unordered_set<std::string> ips_;
};

class AutoBlocker final {
public:
  AutoBlocker(std::shared_ptr<BadIp> bip, sdk::wwivd_blocking_t b);
  ~AutoBlocker();
  bool Connection(const std::string& ip);

private:
  std::shared_ptr<BadIp> bip_;
  sdk::wwivd_blocking_t b_;
  std::unordered_map<std::string, std::set<time_t>> sessions_;
};

} // namespace

#endif
