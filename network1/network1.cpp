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
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/semaphore_file.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "networkb/net_util.h"
#include "sdk/net/packets.h"

#include "sdk/bbslist.h"
#include "sdk/contact.h"
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
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::os;

static void ShowHelp(const CommandLine& cmdline) {
  cout << cmdline.GetHelp() << ".####      Network number (as defined in wwivconfig)" << endl
       << endl;
  exit(1);
}

static std::string wwivnet_packet_name(const net_networks_rec& net, uint16_t node) {
  return Packet::wwivnet_packet_name(net, node);
}

/**
 * Determines the filename for each of the nodes in list to forward to
 * and writes packets (calling write_wwivnet_packet) to each of them.
 */
static bool write_multiple_wwivnet_packets(const net_networks_rec& net,
                                           const wwiv::sdk::BbsListNet b, const net_header_rec& nh,
                                           const std::vector<uint16_t>& list,
                                           const std::string& text) {
  std::map<uint16_t, std::set<uint16_t>> forsys_to_all;
  for (const auto& node : list) {
    auto forsys = get_forsys(b, node);
    forsys_to_all[forsys].insert(node);
  }

  auto result = true;
  for (const auto& fa : forsys_to_all) {
    const auto forsys = fa.first;
    Packet np(nh, std::vector<uint16_t>(fa.second.begin(), fa.second.end()), text);
    np.nh.list_len = static_cast<uint16_t>(np.list.size());
    if (np.list.size() == 1) {
      // If we only have 1, move it out of list into tosys.
      np.nh.tosys = *np.list.begin();
      np.nh.list_len = 0;
      np.list.clear();
    }
    if (!write_wwivnet_packet(Packet::wwivnet_packet_name(net, forsys), net, np)) {
      result = false;
    }
  }
  return result;
}

static bool handle_packet(const BbsListNet& b, const net_networks_rec& net, Packet& p) {

  // Update the routing information on this packet since
  // we're unpacking it.
  p.UpdateRouting(net);

  if (p.nh.tosys == net.sysnum) {
    // Local Packet.
    return write_wwivnet_packet(LOCAL_NET, net, p);
  }
  if (p.list.empty()) {
    // Network packet, single destination
    return write_wwivnet_packet(Packet::wwivnet_packet_name(net, get_forsys(b, p.nh.tosys)), net,
                                p);
  }
  // Network packet, multiple destinations.
  return write_multiple_wwivnet_packets(net, b, p.nh, p.list, p.text());
}

static bool handle_file(const BbsListNet& b, const net_networks_rec& net, const string& name) {
  File f(FilePath(net.dir, name));
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

int network1_main(const NetworkCommandLine& net_cmdline) {
  try {
    const auto& net = net_cmdline.network();

    VLOG(1) << "Reading bbsdata.net..";
    BbsListNet b = BbsListNet::ReadBbsDataNet(net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: Unable to read bbsdata.net.";
      LOG(ERROR) << "       Do you need to run network3?";
      return 1;
    }

    LOG(INFO) << " * Analyzing " << net.name << " pending files...";
    FindFiles ff(net.dir, "p*.net", FindFilesType::files);
    for (const auto& f : ff) {
      LOG(INFO) << "Processing: " << net.dir << f.name;
      if (handle_file(b, net, f.name)) {
        LOG(INFO) << "Deleting: " << net.dir << f.name;
        if (net_cmdline.skip_delete()) {
          backup_file(FilePath(net.dir,f.name));
        }
        File::Remove(net.dir, f.name);
      }
    }

    // Update contact record.
    Contact contact(net, true);
    for (const auto& kv : contact.contacts()) {
      File outbound(FilePath(net.dir, StrCat("s", kv.second.systemnumber(), ".net")));
      auto c = contact.contact_rec_for(kv.second.systemnumber());
      if (outbound.Exists()) {
        c->set_bytes_waiting(outbound.length());
      } else {
        c->set_bytes_waiting(0);
      }
    }

    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network1]: " << e.what();
  }
  return 2;
}

int main(int argc, char** argv) { 
  Logger::Init(argc, argv);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  NetworkCommandLine net_cmdline(cmdline, '1');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline.cmdline());
    return 1;
  }

  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_filename(),
                                                net_cmdline.semaphore_timeout());
    return network1_main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}