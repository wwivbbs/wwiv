/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2017-2022, WWIV Software Services             */
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

#include <ctime>
#include <mutex>
#include <regex>
#include <string>
#include <vector>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/string.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/vector.hpp>
#include <nlohmann/json.hpp>

#include "core/datetime.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fmt/format.h"
#include "httplib.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/instance.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/usermanager.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"

namespace wwiv::wwivd {

static const char MIME_TYPE_JSON[] = "application/json";

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

struct status_reponse_v0_t {
  int num_instances;
  int used_instances;
  std::vector<std::string> lines;
};

struct status_line_t {
  // Name of BBS or binkp
  std::string name;
  // BBS node number 
  int node;
  // PID of bbs binary
  int pid;
  // is this node connected
  bool connected{ false };
  // Textual status of node.
  std::string status;
  // Remote address.
  std::string remote;
  // Connection time in ISO8601
  time_t connect_time;
  // User number on this node
  int user_number{ 0 };
  // User handle/name
  std::string user_handle;
};

struct status_reponse_t {
  int num_instances;
  int used_instances;
  std::vector<status_line_t> lines;
};

// v0
void to_json(nlohmann::json& j, const status_reponse_v0_t& v) {
  j = nlohmann::json{
      {"num_instances", v.num_instances}, {"used_instances", v.used_instances}, {"lines", v.lines}};
}

void from_json(const nlohmann::json& j, status_reponse_v0_t& v) {
  j.at("num_instances").get_to(v.num_instances);
  j.at("used_instances").get_to(v.used_instances);
  j.at("lines").get_to(v.lines);
}

// v1
void to_json(nlohmann::json& j, const status_line_t& v) {
  j = nlohmann::json{{"name", v.name},
                     {"node", v.node},
                     {"pid", v.pid},
                     {"remote", v.remote},
                     {"connected", v.connected},
                     {"status", v.status},
                     {"connect_time", v.connect_time},
                     {"user_number", v.user_number},
                     {"user_handle", v.user_handle}};
}

void from_json(const nlohmann::json& j, status_line_t& v) {
  j.at("name").get_to(v.name);
  j.at("node").get_to(v.node);
  j.at("pid").get_to(v.pid);
  j.at("remote").get_to(v.remote);
  j.at("connected").get_to(v.connected);
  j.at("status").get_to(v.status);
  j.at("connect_time").get_to(v.connect_time);
  if (j.contains("user_number")) {
    j.at("user_number").get_to(v.user_number);
  }
  if (j.contains("user_handle")) {
    j.at("user_handle").get_to(v.user_handle);
  }
}

void to_json(nlohmann::json& j, const status_reponse_t& v) {
  j = nlohmann::json{
      {"num_instances", v.num_instances}, {"used_instances", v.used_instances}, {"lines", v.lines}};
}

void from_json(const nlohmann::json& j, status_reponse_t& v) {
  j.at("num_instances").get_to(v.num_instances);
  j.at("used_instances").get_to(v.used_instances);
  j.at("lines").get_to(v.lines);
}

std::string ToJson(status_reponse_t r) {
  using json = nlohmann::json;
  json j;
  j["status"] = r;
  return j.dump(4);
}

std::string ToJson(status_reponse_v0_t r) {
  using json = nlohmann::json;
  json j;
  j["status"] = r;
  return j.dump(4);
}

// JSON
void StatusHandler(std::map<const std::string, std::shared_ptr<NodeManager>>* nodes,
                   const wwiv::sdk::Config* config,
                   const httplib::Request& req, httplib::Response& res) {
  static std::mutex mu;
  std::lock_guard<std::mutex> lock(mu);

  int version = 0;
  // Determine version from path
  const auto& path = req.path;
  if (path == "/nodes") {
    version = 1;
  } else if (path == "/status") {
    version = 0;
  }

  // We only handle status
  switch (version) {
  case 1: {
    status_reponse_t r{};
    // Only use config for version 1 (/nodes endpoint)
    std::unique_ptr<Names> names;
    if (config) {
      names = std::make_unique<Names>(*config);
    }
    for (const auto& n : *nodes) {
      const auto& bbs_name = n.first;
      const auto& nm = n.second;
      r.num_instances += nm->total_nodes();
      r.used_instances += nm->nodes_used();
      // Read the latest state from the instance.dat.
      (void) nm->update_nodes();
      for (const auto& node : nm->nodes()) {
        status_line_t l{};
        l.name = bbs_name;
        l.node = node.node;
        l.status = node.description;
        l.connected = node.connected;
        l.connect_time = node.connection_time;
        l.remote = node.peer;
        l.pid = node.pid;
        l.user_number = node.user_number;
        if (l.user_number > 0 && names) {
          l.user_handle = names->UserName(l.user_number);
        }
        r.lines.push_back(l);
      }
    }

    const auto source = ToJson(r);
    res.set_content(source, MIME_TYPE_JSON);
    return;
  } break;
  case 0:
  default: {
    status_reponse_v0_t r{};
    for (const auto& n : *nodes) {
      const auto v = n.second->status_lines();
      r.num_instances += n.second->total_nodes();
      r.used_instances += n.second->nodes_used();
      for (const auto& l : v) {
        r.lines.push_back(l);
      }
    }
    const auto source = ToJson(r);
    res.set_content(source, MIME_TYPE_JSON);
    return;
  } break;
  }
}

struct ip_entry_t {
  std::string ip;
  std::string added_date;
  time_t added_timestamp{0};
  std::string expire_date;
  time_t expire_timestamp{0};
  int block_count{0};
  bool permanent{false};
};

void to_json(nlohmann::json& j, const ip_entry_t& e) {
  j = nlohmann::json{
      {"ip", e.ip},
      {"added_date", e.added_date},
      {"added_timestamp", e.added_timestamp},
      {"expire_date", e.expire_date},
      {"expire_timestamp", e.expire_timestamp},
      {"block_count", e.block_count},
      {"permanent", e.permanent}};
}

static std::vector<ip_entry_t> ParseIpFile(const std::filesystem::path& filename) {
  std::vector<ip_entry_t> entries;
  if (!File::Exists(filename)) {
    return entries;
  }

  TextFile file(filename, "rt");
  if (!file.IsOpen()) {
    return entries;
  }

  std::string line;
  while (file.ReadLine(&line)) {
    StringTrim(&line);
    if (line.empty() || line.front() == '#') {
      continue;
    }

    ip_entry_t entry;
    // Extract IP (everything before space or #)
    const auto space_pos = line.find(' ');
    const auto hash_pos = line.find('#');
    const auto end_pos = (space_pos != std::string::npos && hash_pos != std::string::npos)
                             ? std::min(space_pos, hash_pos)
                             : (space_pos != std::string::npos ? space_pos : hash_pos);
    
    if (end_pos != std::string::npos) {
      entry.ip = StringTrim(line.substr(0, end_pos));
      // Try to parse timestamp from comment
      if (hash_pos != std::string::npos) {
        const auto comment = StringTrim(line.substr(hash_pos + 1));
        // Look for ISO8601 date format: YYYY-MM-DDTHH:MM:SS
        std::regex date_regex(R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}))");
        std::smatch match;
        if (std::regex_search(comment, match, date_regex)) {
          entry.added_date = match.str(1);
          // Try to parse timestamp manually (YYYY-MM-DDTHH:MM:SS)
          try {
            struct tm tm = {};
            if (sscanf(entry.added_date.c_str(), "%d-%d-%dT%d:%d:%d",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
              tm.tm_year -= 1900;
              tm.tm_mon -= 1;
              tm.tm_isdst = -1;
              entry.added_timestamp = mktime(&tm);
            }
          } catch (...) {
            // Ignore parse errors
          }
        } else {
          entry.added_date = comment;
        }
      }
    } else {
      entry.ip = StringTrim(line);
    }

    if (!entry.ip.empty()) {
      entry.permanent = true; // IPs in files are permanent unless in auto-blocker
      entries.push_back(entry);
    }
  }

  return entries;
}

void BlockingHandler(ConnectionData* data, const httplib::Request&, httplib::Response& res) {
  static std::mutex mu;
  std::lock_guard<std::mutex> lock(mu);

  nlohmann::json response;

  // Parse whitelist (goodip.txt)
  std::vector<ip_entry_t> whitelist;
  if (data->good_ips_) {
    const auto goodip_file = FilePath(data->config->datadir(), "goodip.txt");
    whitelist = ParseIpFile(goodip_file);
  }
  response["whitelist"] = whitelist;
  response["whitelist_count"] = static_cast<int>(whitelist.size());

  // Parse blacklist (badip.txt)
  std::vector<ip_entry_t> blacklist;
  if (data->bad_ips_) {
    const auto badip_file = FilePath(data->config->datadir(), "badip.txt");
    blacklist = ParseIpFile(badip_file);
  }

  // Add auto-blocked IPs
  std::vector<ip_entry_t> auto_blocked;
  if (data->auto_blocker_) {
    const auto& auto_blocked_map = data->auto_blocker_->auto_blocked();
    const auto now = std::chrono::system_clock::now();
    const auto now_time_t = std::chrono::system_clock::to_time_t(now);

    for (const auto& [ip, entry] : auto_blocked_map) {
      ip_entry_t ip_entry;
      ip_entry.ip = ip;
      ip_entry.block_count = entry.count;
      ip_entry.expire_timestamp = entry.expiration;
      
      if (entry.expiration > 0) {
        const auto expire_dt = DateTime::from_time_t(entry.expiration);
        ip_entry.expire_date = expire_dt.to_string("%FT%T");
        ip_entry.permanent = false;
      } else {
        ip_entry.permanent = true;
      }

      // Check if expired
      if (entry.expiration > 0 && entry.expiration <= now_time_t) {
        continue; // Skip expired entries
      }

      auto_blocked.push_back(ip_entry);
    }
  }

  // Merge auto-blocked into blacklist (they're also blocked)
  blacklist.insert(blacklist.end(), auto_blocked.begin(), auto_blocked.end());
  response["blacklist"] = blacklist;
  response["blacklist_count"] = static_cast<int>(blacklist.size());
  response["auto_blocked"] = auto_blocked;
  response["auto_blocked_count"] = static_cast<int>(auto_blocked.size());

  res.set_content(response.dump(4), MIME_TYPE_JSON);
}

void SysopHandler(ConnectionData* data, const httplib::Request&, httplib::Response& res) {
  static std::mutex mu;
  std::lock_guard<std::mutex> lock(mu);

  nlohmann::json response;

  // Get status information
  StatusMgr status_mgr(data->config->datadir());
  const auto status = status_mgr.get_status();
  if (!status) {
    res.status = 500;
    res.set_content(R"({"error": "Unable to read status.dat"})", MIME_TYPE_JSON);
    return;
  }

  // Get sysop user for feedback_waiting
  int feedback_waiting = 0;
  UserManager user_mgr(*data->config);
  if (const auto sysop = user_mgr.readuser(1)) {
    feedback_waiting = sysop->email_waiting();
  }

  // Get last user from instances
  std::string last_user = "Nobody";
  Instances instances(*data->config);
  if (instances) {
    // Find the instance with the highest user_number that's valid
    int max_user_num = 0;
    for (const auto& inst : instances) {
      const auto user_num = inst.user_number();
      if (user_num > 0 && user_num < data->config->max_users() && user_num > max_user_num) {
        max_user_num = user_num;
      }
    }
    if (max_user_num > 0) {
      Names names(*data->config);
      last_user = names.UserName(max_user_num);
    }
  }

  // Get chat status (sysop available) - check if sysop (user 1) is online
  std::string chat_status = "Not Available";
  if (instances) {
    for (const auto& inst : instances) {
      if (inst.online() && inst.user_number() == 1) {
        // Sysop is online, check if in chat location
        const auto loc = inst.loc_code();
        if (loc == INST_LOC_CHAT || loc == INST_LOC_CHAT2 || loc == INST_LOC_CHATROOM) {
          chat_status = "Available";
        }
        break;
      }
    }
  }

  // Calculate call/day ratio
  std::string call_day_ratio = "N/A";
  if (status->days_active() > 0) {
    const auto ratio = static_cast<float>(status->caller_num()) / static_cast<float>(status->days_active());
    call_day_ratio = fmt::format("{:.2f}", ratio);
  }

  // Get current date and time
  const auto now = DateTime::now();
  const auto date_str = now.to_string("%m/%d/%y");
  const auto time_str = now.to_string("%H:%M:%S");

  // Calculate minutes percentage
  const auto percent = static_cast<double>(status->active_today_minutes()) / 1440.0;

  // Build response
  response["calls_today"] = status->calls_today();
  response["feedback_waiting"] = feedback_waiting;
  response["uploads_today"] = status->uploads_today();
  response["messages_today"] = status->msgs_today();
  response["email_today"] = status->email_today();
  response["feedback_today"] = status->feedback_today();
  response["mins_used_today"] = status->active_today_minutes();
  response["mins_used_today_percent"] = fmt::format("{:.2f}", 100.0 * percent);
  response["wwiv_version"] = full_version();
  response["net_version"] = status->status_net_version();
  response["total_users"] = status->num_users();
  response["total_calls"] = status->caller_num();
  response["call_day_ratio"] = call_day_ratio;
  response["chat_status"] = chat_status;
  response["last_user"] = last_user;
  response["os"] = os::os_version_string();
  response["date"] = date_str;
  response["time"] = time_str;

  res.set_content(response.dump(4), MIME_TYPE_JSON);
}

} // namespace wwiv::wwivd
