/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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

// WWIV5 Network1
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/findfiles.h"
#include "networkb/net_util.h"
#include "networkb/packets.h"

#include "sdk/bbslist.h"
#include "sdk/filenames.h"

using std::cout;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::os;


static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp()
       << ".####      Network number (as defined in wwivconfig)" << endl
       << endl;
  exit(1);
}

static uint16_t get_forsys( const BbsListNet& b, uint16_t node) {
  VLOG(2) << "get_forsys (forward to systen number) for node: " << node;
  auto n = b.node_config_for(node);
  if (node == 0) {
    return 0;
  }
  if (n == nullptr || n->forsys == WWIVNET_NO_NODE) {
    VLOG(2) << "get_forsys: no route to node: " << node;
    return WWIVNET_NO_NODE;
  }
  VLOG(2) << "get_forsys: route to node: " << node << "; is through node: " << n->forsys;
  return n->forsys;
}

static string wwivnet_packet_name(const net_networks_rec& net, uint16_t node) {
  if (node == net.sysnum || node == 0) {
    // Messages to us to into local.net.
    return LOCAL_NET;
  }
  if (node == WWIVNET_NO_NODE) {
    return DEAD_NET;
  }
  return StringPrintf("s%u.net", node);
}

static bool handle_packet(
  const BbsListNet& b,
  const net_networks_rec& net, Packet& p) {

  // Update the routing information on this packet since
  // we're unpacking it.
  p.UpdateRouting(net);

  if (p.nh.tosys == net.sysnum) {
    // Local Packet.
    return write_wwivnet_packet(LOCAL_NET, net, p);
  } 
  if (p.list.empty()) {
    // Network packet, single destination
    return write_wwivnet_packet(wwivnet_packet_name(net, get_forsys(b, p.nh.tosys)), net, p);
  } 
  // Network packet, multiple destinations.
  map<uint16_t, set<uint16_t>> forsys_to_all;
  for (const auto& node : p.list) {
    auto forsys = get_forsys(b, node);
    forsys_to_all[forsys].insert(node);
  }

  auto result = true;
  for (const auto& fa : forsys_to_all) {
    const auto forsys = fa.first;
    Packet np(p.nh, vector<uint16_t>(fa.second.begin(), fa.second.end()), p.text);
    np.nh.list_len = static_cast<uint16_t>(np.list.size());
    if (np.list.size() == 1) {
      // If we only have 1, move it out of list into tosys.
      np.nh.tosys = *np.list.begin();
      np.nh.list_len = 0;
      np.list.clear();
    }
    if (!write_wwivnet_packet(wwivnet_packet_name(net, forsys), net, np)) {
      result = false;
    }
  }
  return result;
}

static bool handle_file(const BbsListNet& b, const net_networks_rec& net, const string& name) {
  File f(net.dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(INFO) << "Unable to open file: " << net.dir << name;
    return false;
  }

  for (;;) {
    Packet packet;
    const auto response = read_packet(f, packet, false);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return true;
    }
    if (response == ReadPacketResponse::ERROR) {
      return false;
    }
    if (!handle_packet(b, net, packet)) {
      LOG(INFO) << "error handing packet: type: " << packet.nh.main_type;
    }
  }
}

int network1_main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "net");
    NetworkCommandLine net_cmdline(cmdline);
    if (!net_cmdline.IsInitialized() || cmdline.help_requested()) {
      ShowHelp(cmdline);
      return 1;
    }

    const auto& net = net_cmdline.network();
    LOG(INFO) << "NETWORK1 for network: " << net.name;

    VLOG(1) << "Reading bbsdata.net..";
    BbsListNet b = BbsListNet::ReadBbsDataNet(net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: Unable to read bbsdata.net.";
      LOG(ERROR) << "       Do you need to run network3?";
      return 1;
    }

    FindFiles ff(net.dir, "p*.net", FindFilesType::files);
    for (const auto& f : ff) {
      LOG(INFO) << "Processing: " << net.dir << f.name;
      if (handle_file(b, net, f.name)) {
        LOG(INFO) << "Deleting: " << net.dir << f.name;
        File::Remove(net.dir, f.name);
      }
    }

    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network1]: " << e.what();
  }
  return 2;
}

int main(int argc, char** argv) { return network1_main(argc, argv); }