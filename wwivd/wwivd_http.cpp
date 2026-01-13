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
#include "httplib.h"
#include "sdk/config.h"
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
                     {"connect_time", v.connect_time}};
}

void from_json(const nlohmann::json& j, status_line_t& v) {
  j.at("name").get_to(v.name);
  j.at("node").get_to(v.node);
  j.at("pid").get_to(v.pid);
  j.at("remote").get_to(v.remote);
  j.at("connected").get_to(v.connected);
  j.at("status").get_to(v.status);
  j.at("connect_time").get_to(v.connect_time);
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
static std::map<std::string, int> version_map = {{"2023-01", 0}, {"2023-05", 1}};

void StatusHandler(std::map<const std::string, std::shared_ptr<NodeManager>>* nodes,
                   const httplib::Request& req, httplib::Response& res) {
  static std::mutex mu;
  std::lock_guard<std::mutex> lock(mu);

  int version = 0;
  // Determine version from path
  const auto& path = req.path;
  if (path == "/status_v1") {
    version = 1;
  } else if (path == "/status_v0") {
    version = 0;
  } else if (path == "/status") {
    // Default to version 0, but allow query parameter override for backward compatibility
    version = 0;
    if (req.has_param("version")) {
      const auto v = req.get_param_value("version");
      version = wwiv::stl::get_or_default(version_map, v, 0);
    }
  }

  // We only handle status
  switch (version) {
  case 1: {
    status_reponse_t r{};
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
  response["auto_blocked"] = auto_blocked;

  res.set_content(response.dump(4), MIME_TYPE_JSON);
}

} // namespace wwiv::wwivd
