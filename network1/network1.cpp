/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2016, WWIV Software Services               */
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
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/wfndfile.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::os;

INITIALIZE_EASYLOGGINGPP

static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp()
       << ".####      Network number (as defined in INIT)" << endl
       << endl;
  exit(1);
}

static uint16_t get_forsys( const BbsListNet& b, uint16_t node) {
  auto n = b.node_config_for(node);
  if (node == 0) {
    return 0;
  }
  if (n == nullptr || n->forsys == 65535) {
    return 65535;
  }
  return n->forsys;
}

static string CreateNetworkFileName(const net_networks_rec& net, uint16_t node) {
  if (node == net.sysnum || node == 0) {
    // Messages to us to into local.net.
    return LOCAL_NET;
  } else if (node == 65535) {
    return DEAD_NET;
  } else {
    return StringPrintf("s%u.net", node);
  }
}

static bool handle_packet(
  const BbsListNet& b,
  const net_networks_rec& net,
  const net_header_rec& nh, const std::vector<uint16_t>& list, const string& text) {

  if (nh.tosys == net.sysnum) {
    // Local Packet.
    return write_packet(LOCAL_NET, net, nh, list, text);
  } else if (list.empty()) {
    // Network packet, single destination
    return write_packet(CreateNetworkFileName(net, get_forsys(b, nh.tosys)), net, nh, list, text);
  } else {
    // Network packet, multiple destinations.
    std::map<uint16_t, std::set<uint16_t>> forsys_to_all;
    for (const auto& node : list) {
      uint16_t forsys = get_forsys(b, node);
      forsys_to_all[forsys].insert(node);
    }

    bool result = true;
    for (const auto& fa : forsys_to_all) {
      const auto forsys = fa.first;
      auto forsys_list = fa.second;
      net_header_rec mutable_nh = nh;
      mutable_nh.list_len = static_cast<uint16_t>(forsys_list.size());
      if (forsys_list.size() == 1) {
        // If we only have 1, move it out of list into tosys.
        mutable_nh.tosys = *forsys_list.begin();
        mutable_nh.list_len = 0;
        forsys_list.clear();
      }
      if (!write_packet(CreateNetworkFileName(net, forsys), net, mutable_nh, forsys_list, text)) {
        result = false;
      }
    }
    return result;
  }

  return false;
}

static bool handle_file(const BbsListNet& b, const net_networks_rec& net, const string& name) {
  File f(net.dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(INFO) << "Unable to open file: " << net.dir << name;
    return false;
  }

  bool done = false;
  while (!done) {
    net_header_rec nh;
    std::vector<uint16_t> list;
    string text;
    int num_read = f.Read(&nh, sizeof(net_header_rec));
    if (num_read == 0) {
      // at the end of the packet.
      return true;
    }
    if (num_read != sizeof(net_header_rec)) {
      LOG(INFO) << "error reading header, got short read of size: " << num_read
          << "; expected: " << sizeof(net_header_rec);
      return false;
    }
    if (nh.method > 0) {
      LOG(INFO) << "compression: de" << nh.method;
    }

    if (nh.list_len > 0) {
      // read list of addresses.
      list.resize(nh.list_len);
      f.Read(&list[0], 2 * nh.list_len);
    }
    if (nh.length > 0) {
      text.resize(nh.length);
      f.Read(&text[0], nh.length);
    }
    if (!handle_packet(b, net, nh, list, text)) {
      LOG(INFO) << "error handing packet: type: " << nh.main_type;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.set_no_args_allowed(true);
    cmdline.AddStandardArgs();
    AddStandardNetworkArgs(cmdline, File::current_directory());

    if (!cmdline.Parse() || cmdline.arg("help").as_bool()) {
      ShowHelp(cmdline);
      return 1;
    }
    string network_name = cmdline.arg("network").as_string();
    string network_number = cmdline.arg("network_number").as_string();
    if (network_name.empty() && network_number.empty()) {
      LOG(ERROR) << "--network=[network name] or .[network_number] must be specified.";
      ShowHelp(cmdline);
      return 1;
    }

    string bbsdir = cmdline.arg("bbsdir").as_string();
    Config config(bbsdir);
    if (!config.IsInitialized()) {
      LOG(ERROR) << "Unable to load CONFIG.DAT.";
      return 1;
    }
    Networks networks(config);
    if (!networks.IsInitialized()) {
      LOG(ERROR) << "Unable to load networks.";
      return 1;
    }

    if (!network_number.empty() && network_name.empty()) {
      // Need to set the network name based on the number.
      network_name = networks[std::stoi(network_number)].name;
    }

    LOG(INFO) << "NETWORK1 for network: " << network_name;
    auto net = networks[network_name];

    VLOG(1) << "Reading BBSDATA.NET..";
    BbsListNet b = BbsListNet::ReadBbsDataNet(net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: Unable to read BBSDATA.NET.";
      return 1;
    }

    WFindFile s_files;
    bool has_next = s_files.open(StrCat(net.dir, "p*.net"), WFINDFILE_FILES);
    while (has_next) {
      const string name = s_files.GetFileName();
      LOG(INFO) << "Processing: " << net.dir << name;
      if (handle_file(b, net, name)) {
        LOG(INFO) << "Deleting: " << net.dir << name;
        File::Remove(net.dir, name);
      }
      has_next = s_files.next();
    }

    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network]: " << e.what();
  }
  return 2;
}
