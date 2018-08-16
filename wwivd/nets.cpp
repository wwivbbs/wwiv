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

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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

#include "core/datetime.h"
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
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/net/callouts.h"
#include "sdk/networks.h"
#include "sdk/status.h"
#include "wwivd/connection_data.h"
#include "wwivd/wwivd.h"
#include "wwivd/wwivd_non_http.h"

namespace wwiv {
namespace wwivd {

using std::map;
using std::string;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

std::atomic<bool> need_to_exit;
std::atomic<bool> need_to_reload_config;

// TODO(rushfan): Add tests for new stuff in here.

static NetworkContact network_contact_from_last_time(const std::string& address, time_t t) {
  network_contact_record ncr{};
  ncr.address = address;
  ncr.ncr.lastcontact = time_t_to_daten(t);
  ncr.ncr.lasttry = time_t_to_daten(t);
  return NetworkContact{ncr};
}

static void one_net_ftn_callout(const Config& config, const net_networks_rec& net,
                                const wwivd_config_t& c, int network_number) {
  wwiv::sdk::fido::FidoCallout callout(config, net);

  // TODO(rushfan): 1. Right now we just keep the map of last callout
  // time in memory, but we should checkpoint this to disk and reload
  // on startup.
  // 2. Also we should look for files outbound to the node so that we can
  // handle min_k right.
  // 3. We should look for outbound files to other addresses we don't
  // know about and then figure out how to contact them (since their
  // address is in the nodelist.
  static std::map<int, std::map<std::string, time_t>> last_contact;
  auto& current_last_contact = last_contact[network_number];

  for (const auto& kv : callout.node_configs_map()) {
    const auto address = kv.first.as_string();
    const auto& callout = kv.second.callout_config;
    if (!wwiv::sdk::net::allowed_to_call(callout, DateTime::now())) {
      // Is the callout bit set.
      continue;
    }
    auto ncn = network_contact_from_last_time(address, current_last_contact[address]);
    if (!wwiv::sdk::net::should_call(ncn, callout, DateTime::now())) {
      // Has it been long enough, or do we have enough k waiting.
      continue;
    }
    // 1: Update the last contact time to now.
    current_last_contact[address] = DateTime::now().to_time_t();
    // 2: Call it.
    LOG(INFO) << "ftn: should call out to: " << kv.first << "." << net.name;
    const std::map<char, string> params = {{'N', address}, {'T', std::to_string(network_number)}};
    const auto cmd = CreateCommandLine(c.network_callout_cmd, params);
    if (!ExecCommandAndWait(cmd, StrCat("[", get_pid(), "]"), -1, -1)) {
      LOG(ERROR) << "Error executing command: '" << cmd << "'";
    }
  }
}

static void one_net_wwivnet_callout(const Config& config, const net_networks_rec& net,
                                    const wwivd_config_t& c, int network_number) {
  Contact contact(net);
  Callout callout(net);
  for (const auto& kv : callout.callout_config()) {
    const auto& ncn = contact.contact_rec_for(kv.first);
    if (!wwiv::sdk::net::allowed_to_call(kv.second, DateTime::now())) {
      continue;
    }
    if (!wwiv::sdk::net::should_call(*ncn, kv.second, DateTime::now())) {
      continue;
    }
    // Call it.
    LOG(INFO) << "should call out to: " << kv.first << "." << net.name;
    const std::map<char, string> params = {{'N', std::to_string(kv.first)},
                                           {'T', std::to_string(network_number)}};
    const auto cmd = CreateCommandLine(c.network_callout_cmd, params);
    if (!ExecCommandAndWait(cmd, StrCat("[", get_pid(), "]"), -1, -1)) {
      LOG(ERROR) << "Error executing command: " << cmd;
    }
  }
}

static void one_callout_loop(const Config& config, const wwivd_config_t& c) {
  VLOG(1) << "do_wwivd_callouts: one_callout_loop: ";
  Networks networks(config);
  const auto& nets = networks.networks();
  int network_number = 0;
  for (const auto& net : nets) {
    if (net.type == network_type_t::wwivnet) {
      one_net_wwivnet_callout(config, net, c, network_number++);
    } else if (net.type == network_type_t::ftn) {
      one_net_ftn_callout(config, net, c, network_number++);
    }
  }
}

// This is called from the thread
static void do_wwivd_callout_loop(const Config& config, const wwivd_config_t& original_config) {
  wwivd_config_t c{original_config};

  StatusMgr sm(config.datadir(), [](int) {});
  auto e = need_to_exit.load();
  while (!e) {
    // Reload the config if we've gotten a HUP?
    if (need_to_reload_config.load()) {
      LOG(INFO) << "Received HUP: Reloading Configuration for Callouts.";
      need_to_reload_config.store(false);
      c.Load(config);
    }
    if (c.do_network_callouts) {
      one_callout_loop(config, c);
    }
    if (need_to_exit.load()) {
      return;
    }
    sleep_for(15s);
    e = need_to_exit.load();

    auto last_date_status = sm.GetStatus();
    if (date() != last_date_status->GetLastDate()) {
      LOG(INFO) << "Executing BeginDay Event.";
      const std::map<char, string> params{};
      const auto cmd = CreateCommandLine(c.beginday_cmd, params);
      if (!ExecCommandAndWait(cmd, StrCat("[", get_pid(), "]"), -1, -1)) {
        LOG(ERROR) << "Error executing [BeginDay Event]: '" << cmd << "'";
      }
    }
  }
}

void do_wwivd_callouts(const Config& config, const wwivd_config_t& c) {
  if (c.do_network_callouts) {
    LOG(INFO) << "WWIVD is handling network callouts.";
  } else if (c.do_beginday_event) {
    LOG(INFO) << "WWIVD is only handling beginday event.";
  }
  std::thread callout_thread(do_wwivd_callout_loop, config, c);
  callout_thread.detach();
}

} // namespace wwivd
} // namespace wwiv
