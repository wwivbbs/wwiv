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

#include "core/jsonfile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "httplib.h"
#include "sdk/config.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"

#include <string>

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
  if (req.has_param("version")) {
    const auto v = req.get_param_value("version");
    version = wwiv::stl::get_or_default(version_map, v, 0);
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

} // namespace wwiv::wwivd
