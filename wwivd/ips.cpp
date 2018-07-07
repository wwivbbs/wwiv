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

#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/vector.hpp>

#include "core/file.h"
#include "core/http_server.h"
#include "core/inifile.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "core/datetime.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd.h"

namespace wwiv {
namespace wwivd {

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
    auto space = line.find(' ');
    if (space != line.npos) {
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

GoodIp::GoodIp(const std::string& fn) {
  TextFile f(fn, "r");
  if (f) {
    auto lines = f.ReadFileIntoVector();
    LoadLines(lines);
  }
}

bool GoodIp::IsAlwaysAllowed(const std::string& ip) {
  if (ips_.empty()) {
    return false;
  }
  return ips_.find(ip) != ips_.end();
}

BadIp::BadIp(const std::string& fn) : fn_(fn) {
  TextFile f(fn, "r");
  if (f) {
    auto lines = f.ReadFileIntoVector();
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
  auto now = DateTime::now();
  auto written =
      appender.WriteLine(StrCat(ip, " # AutoBlocked by wwivd on: ", now.to_string("%FT%T")));
  return written > 0;
}

AutoBlocker::AutoBlocker(std::shared_ptr<BadIp> bip, const wwiv::sdk::wwivd_blocking_t& b)
    : bip_(bip), b_(b) {}

AutoBlocker::~AutoBlocker() {}

static string to_string(std::set<time_t> times) {
  std::stringstream ss;
  ss << "{";
  for (const auto& t : times) {
    ss << t << ", ";
  }
  ss << "}";
  return ss.str();
}

bool AutoBlocker::Connection(const std::string& ip) {
  if (!b_.auto_blacklist) {
    return true;
  }

  const auto auto_bl_sessions = b_.auto_bl_sessions;
  const auto auto_bl_seconds = b_.auto_bl_seconds;
  auto now = DateTime::now();
  auto oldest_in_window = now.to_time_t() - auto_bl_seconds;
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

} // namespace wwivd
} // namespace wwiv
