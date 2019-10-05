/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*               Copyright (C)2018, WWIV Software Services                */
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
#include "wwivd/ips.h"

#include "core/datetime.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "wwivd/connection_data.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace wwiv::wwivd {

using std::map;
using std::string;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

static bool LoadLinesIntoSet(std::unordered_set<string>& s, const std::vector<std::string>& lines) {
  for (auto line : lines) {
    const auto space = line.find(' ');
    if (space != std::string::npos) {
      line = line.substr(0, space);
    }
    s.emplace(StringTrim(line));
  }
  return true;
}

GoodIp::GoodIp(const std::vector<std::string>& lines) { LoadLines(lines); }

bool GoodIp::LoadLines(const std::vector<std::string>& lines) {
  return LoadLinesIntoSet(ips_, lines);
}

GoodIp::GoodIp(const std::filesystem::path& fn) {
  TextFile f(fn, "r");
  if (f) {
    const auto lines = f.ReadFileIntoVector();
    LoadLines(lines);
  }
}

bool GoodIp::IsAlwaysAllowed(const std::string& ip) {
  if (ips_.empty()) {
    return false;
  }
  return ips_.find(ip) != ips_.end();
}

BadIp::BadIp(const std::filesystem::path& fn) : fn_(fn) {
  TextFile f(fn, "r");
  if (f) {
    const auto lines = f.ReadFileIntoVector();
    LoadLinesIntoSet(ips_, lines);
  }
}
bool BadIp::IsBlocked(const std::string& ip) {
  if (ips_.empty()) {
    return false;
  }
  return ips_.find(ip) != ips_.end();
}
bool BadIp::Block(const std::string& ip) {
  ips_.emplace(ip);
  TextFile appender(fn_, "at");
  const auto now = DateTime::now();
  const auto written =
      appender.WriteLine(StrCat(ip, " # AutoBlocked by wwivd on: ", now.to_string("%FT%T")));
  return written > 0;
}

AutoBlocker::AutoBlocker(std::shared_ptr<BadIp> bip, const wwiv::sdk::wwivd_blocking_t& b)
    : bip_(bip), b_(b) {}

AutoBlocker::~AutoBlocker() = default;

bool AutoBlocker::Connection(const std::string& ip) {
  VLOG(1) << "AutoBlocker::Connection: " << ip;
  if (!b_.auto_blacklist) {
    return true;
  }

  const auto auto_bl_sessions = b_.auto_bl_sessions;
  const auto auto_bl_seconds = b_.auto_bl_seconds;
  const auto now = DateTime::now();
  const auto oldest_in_window = now.to_time_t() - auto_bl_seconds;
  auto& s = sessions_[ip];
  s.emplace(now.to_time_t());
  if (s.size() == 1) {
    return true;
  }

  for (auto iter = s.begin(); iter != s.end();) {
    if (*iter < oldest_in_window) {
      iter = s.erase(iter);
    } else {
      ++iter;
    }
  }

  if (size_int(s) > auto_bl_sessions) {
    LOG(INFO) << "Blocking since we have " << s.size() << " sessions within "
              << auto_bl_seconds << " seconds.";
    bip_->Block(ip);
    return false;
  }
  return true;
}

} // namespace wwiv
