/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_WWIVD_IPS_H__
#define __INCLUDED_WWIVD_IPS_H__

#include "sdk/wwivd_config.h"
#include <ctime>
#include <filesystem>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wwiv {
namespace wwivd {

class GoodIp {
public:
  GoodIp(const std::filesystem::path& fn);
  GoodIp(const std::vector<std::string>& lines);
  bool IsAlwaysAllowed(const std::string& ip);

private:
  bool LoadLines(const std::vector<std::string>& ips);
  std::unordered_set<std::string> ips_;
};

class BadIp {
public:
  BadIp(const std::filesystem::path& fn);
  bool IsBlocked(const std::string& ip);
  bool Block(const std::string& ip);

private:
  const std::filesystem::path fn_;
  std::unordered_set<std::string> ips_;
};

class AutoBlocker {
public:
  AutoBlocker(std::shared_ptr<BadIp> bip, const wwiv::sdk::wwivd_blocking_t& b);
  virtual ~AutoBlocker();
  bool Connection(const std::string& ip);

private:
  std::shared_ptr<BadIp> bip_;
  wwiv::sdk::wwivd_blocking_t b_;
  std::unordered_map<std::string, std::set<time_t>> sessions_;
};

} // namespace wwivd
} // namespace wwiv

#endif // __INCLUDED_WWIVD_IPS_H__
