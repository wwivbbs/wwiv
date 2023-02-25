/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2022, WWIV Software Services             */
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
#include "network1.h"

#include "core/clock.h"
#include "core/command_line.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "net_core/net_cmdline.h"
#include "sdk/bbslist.h"
#include "sdk/filenames.h"
#include "sdk/net/contact.h"
#include "sdk/net/packets.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::os;

static void ShowHelp(const NetworkCommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl;
  exit(1);
}

int NetworkStat::k() const {
  return bytes == 0 ? 0 : (bytes + 1023) / 1024;
}

Network1::Network1(const NetworkCommandLine& cmdline, const BbsListNet& bbslist,
                   wwiv::core::Clock& clock)
    : net_cmdline_(cmdline), bbslist_(bbslist), clock_(clock), net_(net_cmdline_.network()),
      netdat_(net_cmdline_.config().gfilesdir(),
        net_cmdline_.config().logdir(), 
        net_, net_cmdline_.net_cmd(), clock_) {}


/**
 * Determines the filename for each of the nodes in list to forward to
 * and writes packets (calling write_wwivnet_packet) to each of them.
 */
bool Network1::write_multiple_wwivnet_packets(const net_header_rec& orig_header,
                                              const std::vector<uint16_t>& list,
                                              const std::string& text) {
  std::map<uint16_t, std::set<uint16_t>> forsys_to_all;
  for (const auto& node : list) {
    auto forsys = get_forsys(bbslist_, node);
    forsys_to_all[forsys].insert(node);
  }

  if (orig_header.tosys != 0) {
    // TODO(rushfan): Does this happen, and if so should we also forward 
    // this packet to nh.tosys?  Maybe?
    LOG(ERROR) << "write_multiple_wwivnet_packets called to a tosys != 0; got; " << orig_header.tosys;
  }

  auto result = true;
  for (const auto& fa : forsys_to_all) {
    net_header_rec nh{ orig_header };
    std::vector<uint16_t> nplist(fa.second.begin(), fa.second.end());
    nh.list_len = static_cast<uint16_t>(nplist.size());
    NetPacket np(nh, nplist , text);
    if (np.list.size() == 1) {
      // If we only have 1, move it out of list into tosys.
      np.nh.tosys = *std::begin(np.list);
      np.nh.list_len = 0;
      np.list.clear();
    }
    const auto forsys = fa.first;
    netdat_.add_file_bytes(forsys, np.length());
    if (!write_wwivnet_packet(NetPacket::wwivnet_packet_path(net_, forsys), np)) {
      result = false;
    }
  }
  return result;
}

// returns true on success, false on error.
bool Network1::handle_packet(NetPacket& p) {

  if (p.nh.main_type == 65535) {
    // Deleted message. return true since ignoring it *is* handling it appropriately.
    LOG(INFO) << "Skipping deleted message of type: " << main_type_name(p.nh.main_type);
    return true;
  }

  // Update the routing information on this packet since
  // we're unpacking it.
  p.UpdateRouting(net_);

  if (p.nh.tosys == net_.sysnum) {
    // Local Packet.
    netdat_.add_file_bytes(net_.sysnum, p.length());
    return write_wwivnet_packet(FilePath(net_.dir, LOCAL_NET), p);
  }
  if (p.list.empty()) {
    // Network packet, single destination
    const auto forsys = get_forsys(bbslist_, p.nh.tosys);
    netdat_.add_file_bytes(forsys, p.length());
    return write_wwivnet_packet(NetPacket::wwivnet_packet_path(net_, forsys), p);
  }
  // Network packet, multiple destinations.
  return write_multiple_wwivnet_packets(p.nh, p.list, p.text());
}

bool Network1::handle_file(const std::string& name) {
  File f(FilePath(net_.dir, name));
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << net_.dir << name;
    return false;
  }

  for (;;) {
    auto [packet, response] = read_packet(f, false);
    if (response == ReadNetPacketResponse::END_OF_FILE) {
      return true;
    }
    if (response == ReadNetPacketResponse::ERROR) {
      return false;
    }
    if (!handle_packet(packet)) {
      LOG(ERROR) << "error handing packet: type: " << packet.nh.main_type;
    }
  }
}

bool Network1::Run() {
  try {
    LOG(INFO) << " * Analyzing " << net_.name << " pending files...";
    FindFiles ff(FilePath(net_.dir, "p*.net"), FindFiles::FindFilesType::files);
    for (const auto& f : ff) {
      VLOG(1) << "Processing: " << net_.dir.string() << f.name;
      if (handle_file(f.name)) {
        VLOG(1) << "Deleting: " << net_.dir.string() << f.name;
        if (net_cmdline_.skip_delete()) {
          backup_file(FilePath(net_.dir, f.name));
        }
        File::Remove(FilePath(net_.dir, f.name));
      }
    }

    // Update contact record.
    LOG(INFO) << " * Updating " << net_.name << " contact.net...";
    Contact contact(net_, true);
    auto& m = contact.mutable_contacts();
    for (auto it = std::begin(m); it != std::end(m); ) {
      const auto sn = it->second.systemnumber();
      if (!bbslist_.node_config_for(sn)) {
        // Try to remove a entry that does not exist..
        LOG(INFO) << "Removing contact.net entry for node that does not exist: " << sn;
        it = m.erase(it);
        continue;
      }
      auto* c = contact.contact_rec_for(sn);
      // Should *never* happen since we checked above.
      DCHECK(c);
      VLOG(1) << "Updating contact entry for node: @" << sn;
      const auto outbound_fn = FilePath(net_.dir, StrCat("s", it->second.systemnumber(), ".net"));
      if (File::Exists(outbound_fn)) {
        File of(outbound_fn);
        c->set_bytes_waiting(static_cast<int32_t>(of.length()));
      } else {
        c->set_bytes_waiting(0);
      }
      ++it;
    }

    if (netdat_.empty()) {
      // No need to log stats, just exit here.
      return true;
    }

    LOG(INFO) << netdat_.ToDebugString();
    netdat_.WriteStats();
    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network1]: " << e.what();
  }
  return false;
}

int main(int argc, char** argv) { 
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  auto at_exit = finally(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  const NetworkCommandLine net_cmdline(cmdline, '1');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline);
    return 1;
  }


  VLOG(3) << "Reading bbsdata.net..";
  const auto& net = net_cmdline.network();
  const auto b = BbsListNet::ReadBbsDataNet(net.dir);
  if (b.empty()) {
    LOG(ERROR) << "ERROR: Unable to read bbsdata.net.";
    LOG(ERROR) << "       You likely need to run network3?";
    return 1;
  }

  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_path(),
                                                net_cmdline.semaphore_timeout());
    SystemClock clock;
    Network1 n1(net_cmdline, b, clock);
    return n1.Run() ? 0 : 2;
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}