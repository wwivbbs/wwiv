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
#include "wwivd/ips.h"

#include "core/clock.h"
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
#include <utility>
#include <vector>

namespace wwiv::wwivd {

using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

static bool LoadLinesIntoSet(std::unordered_set<std::string>& s, const std::vector<std::string>& lines) {
  for (auto line : lines) {
    const auto space = line.find(' ');
    if (space != std::string::npos) {
      line = line.substr(0, space);
    }
    s.emplace(StringTrim(line));
  }
  return true;
}

GoodIp::GoodIp(const std::vector<std::string>& lines) { (void)LoadLines(lines); }

bool GoodIp::LoadLines(const std::vector<std::string>& lines) {
  return LoadLinesIntoSet(ips_, lines);
}

GoodIp::GoodIp(const std::filesystem::path& fn) {
  TextFile f(fn, "r");
  if (f) {
    const auto lines = f.ReadFileIntoVector();
    (void)LoadLines(lines);
  }
}

bool GoodIp::IsAlwaysAllowed(const std::string& ip) {
  if (ips_.empty()) {
    return false;
  }
  return ips_.find(ip) != ips_.end();
}

BadIp::BadIp(const std::filesystem::path& fn, Clock& clock) : fn_(fn), clock_(clock) {
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
  const auto now = clock_.Now();
  const auto written =
      appender.WriteLine(StrCat(ip, " # AutoBlocked by wwivd on: ", now.to_string("%FT%T")));
  return written > 0;
}

AutoBlocker::AutoBlocker(std::shared_ptr<BadIp> bip, wwivd_blocking_t b, std::filesystem::path datadir, Clock& clock)
    : bip_(std::move(bip)), b_(std::move(b)), datadir_(std::move(datadir)), clock_(clock) {
  if (b_.block_duration.empty()) {
    b_.block_duration.emplace_back("15m");
  }
  if (b_.block_duration.size() < 2) {
    b_.block_duration.emplace_back("1h");
  }
  if (b_.block_duration.size() < 3) {
    b_.block_duration.emplace_back("1d");
  }
  if (b_.block_duration.size() < 4) {
    b_.block_duration.emplace_back("30d");
  }
  auto modified = !Load();

  // Cleanup the autoblock list.
  const auto now = clock_.Now().to_time_t();
  for (auto iter = std::begin(auto_blocked_); iter != std::end(auto_blocked_); ) {
    if (iter->second.expiration < now) {
      LOG(INFO) << "Removing Entry from autoblock list: " << iter->first;
      iter = auto_blocked_.erase(iter);
      modified = true;
    } else {
      ++iter;
    }
  }
  if (modified) {
    Save();
  }
}

AutoBlocker::~AutoBlocker() = default;


void AutoBlocker::escalate_block(const std::string& ip) {
  LOG(INFO) << "escalate_block: " << ip;
  auto& item = auto_blocked_[ip];
  const auto now = clock_.Now();
  ++item.count;
  if (item.count <= size_int(b_.block_duration)) {
    const auto bt = parse_time_span(at(b_.block_duration, item.count - 1)).value_or(std::chrono::minutes(10));
    LOG(INFO) << "escalate_block: count: " << item.count << "; new blocked time: " << wwiv::core::to_string(bt);
    const auto exp = now + bt;
    item.expiration = exp.to_time_t();
  } else {
    LOG(INFO) << "escalate_block: permanent block; count: " << item.count;
    // We've run out of auto-blocks.  Replace this with the permanent one.
    auto_blocked_.erase(ip);
    bip_->Block(ip);
  }
  Save();
}

bool AutoBlocker::Connection(const std::string& ip) {
  VLOG(1) << "AutoBlocker::Connection: Checking status for: " << ip;
  if (!b_.auto_blocklist) {
    return true;
  }

  const auto now = clock_.Now();

  //
  // Synchronized from here on down
  //
  std::lock_guard<std::mutex> lock(mu_);

  if (blocked(ip)) {
    // We have an auto-block and we're still blocked.
    LOG(INFO) << "Still in auto-block for ip: " << ip;
    return false;
  }

  const auto auto_bl_sessions = b_.auto_bl_sessions;
  const auto auto_bl_seconds = b_.auto_bl_seconds;
  const auto oldest_in_window = now.to_time_t() - auto_bl_seconds;

  auto& s = sessions_[ip];
  s.emplace(now.to_time_t());
  if (s.size() == 1) {
    VLOG(1) << "OK: num sessions: " << ssize(s);
    return true;
  }

  for (auto iter = s.begin(); iter != s.end();) {
    if (*iter < oldest_in_window) {
      VLOG(1) << "Erasing old session for IP: " << ip;
      iter = s.erase(iter);
    } else {
      ++iter;
    }
  }

  if (ssize(s) >= auto_bl_sessions) {
    LOG(INFO) << "Blocking since we have " << s.size() << " sessions within " << auto_bl_seconds
              << " seconds.";
    // Don't do the hard block here, just escalate.
    escalate_block(ip);
    return false;
  }
  VLOG(1) << "OK: num sessions: " << ssize(s);
  return true;
}

template <class Archive>
void serialize(Archive & ar, auto_blocked_entry_t &a) {
  SERIALIZE(a, count);
  SERIALIZE(a, expiration);
}

bool AutoBlocker::Save() {
  LOG(INFO) << "AutoBlocker: Save";
  JsonFile<std::map<std::string, auto_blocked_entry_t>> file(FilePath(datadir_, "wwivd.autoblock.json"), "autoblock", auto_blocked_);
  return file.Save();
}

bool AutoBlocker::blocked(const std::string& ip) const {
  const auto now = clock_.Now();
  if (const auto iter = auto_blocked_.find(ip); iter != std::end(auto_blocked_)) {
    if (iter->second.expiration > now.to_time_t()) {
      // We have an auto-block and we're still blocked.
      LOG(INFO) << "Still in auto-block for ip: " << ip;
      return true;
    }
    // TODO: We're good.  Remove this entry once it ages out, what'd be an appropriate age? 1d? 1week? 1 month?
    // auto_blocked_.erase(iter);
    // Save();
  }
  return false;
}

bool AutoBlocker::Load() {
  VLOG(1) << "AutoBlocker: Load";
  JsonFile<std::map<std::string, auto_blocked_entry_t>> file(FilePath(datadir_, "wwivd.autoblock.json"), "autoblock", auto_blocked_);
  return file.Load();
}

} // namespace wwiv::wwivd
