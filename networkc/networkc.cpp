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

// WWIV5 NetworkC
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
#include "core/version.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/fido_util.h"
#include "networkb/packets.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/ftn_msgdupe.h"
#include "sdk/networks.h"
#include "sdk/status.h"
#include "sdk/subscribers.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/fido/fido_packets.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::net::fido;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::os;
using namespace wwiv::sdk::fido;

static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp()
    << ".####      Network number (as defined in INIT)" << endl
    << endl;

  exit(1);
}

string create_network_cmdline(const NetworkCommandLine& net_cmdline, char num, int verbose, const string& cmd) {
  const string path = FilePath(net_cmdline.bbsdir(), StrCat("network", num));

  std::ostringstream ss;
  ss << path << " --v=" << verbose << " ." << net_cmdline.network_number();
  if (num == '3') {
    ss << " Y";
  }
  if (!cmd.empty()) {
    ss << " " << cmd;
  }
  return ss.str();
}

static int System(const string& cmd) {
  LOG(INFO) << "Command: " << cmd;
  return system(cmd.c_str());
}

static bool checkup2(const time_t tFileTime, string dir, string filename) {
  File file(dir, filename);

  if (file.Open(File::modeReadOnly)) {
    time_t tNewFileTime = file.last_write_time();
    file.Close();
    return (tNewFileTime > (tFileTime + 2));
  }
  return true;
}

static bool need_network3(const string& dir, int network_version) {
  if (!File::Exists(dir, BBSLIST_NET)) {
    return false;
  }
  if (!File::Exists(dir, CONNECT_NET)) {
    return false;
  }
  if (!File::Exists(dir, CALLOUT_NET)) {
    return false;
  }

  if (network_version != wwiv_net_version) {
    // always need network3 if the versions do not match.
    LOG(INFO) << "Need to run network3 since current network_version: "
      << network_version << " != our network_version: " << wwiv_net_version;
    return true;
  }
  File bbsdataNet(dir, BBSDATA_NET);
  if (!bbsdataNet.Open(File::modeReadOnly)) {
    return false;
  }

  time_t bbsdata_time = bbsdataNet.last_write_time();
  bbsdataNet.Close();

  return checkup2(bbsdata_time, dir, BBSLIST_NET)
    || checkup2(bbsdata_time, dir, CONNECT_NET)
    || checkup2(bbsdata_time, dir, CALLOUT_NET);
}

bool exists_ftn(const Config& config, const net_networks_rec& net) {
  const std::vector<string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?", "pkt"};
  auto net_dir = File::MakeAbsolutePath(config.root_directory(), net.dir);
  auto inbounddir = File::MakeAbsolutePath(net_dir, net.fido.inbound_dir);
  for (const auto& e : extensions) {
    {
      const string mask = StrCat("*.", e);
      if (File::ExistsWildcard(FilePath(inbounddir, mask))) {
        File f(inbounddir, mask);
        if (f.GetLength() > 0) return true;
      }
    }
    {
      string ue(e);
      StringUpperCase(&ue);
      const string mask = StrCat("*.", ue);
      if (File::ExistsWildcard(FilePath(inbounddir, mask))) {
        File f(inbounddir, mask);
        if (f.GetLength() > 0) return true;
      }
    }
  }
  return false;
}

int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "net");
    NetworkCommandLine net_cmdline(cmdline);
    if (!net_cmdline.IsInitialized() || cmdline.help_requested()) {
      ShowHelp(cmdline);
      return 1;
    }

    const int verbose = cmdline.iarg("v");
    const auto& net = net_cmdline.network();
    std::cout << "NETWORKC for network: " << net.name;

    StatusMgr sm(net_cmdline.config().datadir(), [](int) {});
    std::unique_ptr<WStatus> status(sm.GetStatus());

    const auto dir = net.dir;
    // Pending files, call network1 to put them into s* or local.net.
    if (File::ExistsWildcard(StrCat(dir, "p*.net"))) {
      System(create_network_cmdline(net_cmdline, '1', verbose, ""));
    }

    // If the network type is a FTN network.
    if (net.type == network_type_t::ftn) {
      // Import everything into LOCAL.NET
      if (File::ExistsWildcard(FilePath(net.fido.inbound_dir, "*.*"))) {
        System(create_network_cmdline(net_cmdline, 'f', verbose, "import"));
      }
      
      if (exists_ftn(net_cmdline.config(), net)) {
        System(create_network_cmdline(net_cmdline, 'f', verbose, "export"));
      }

      // Export everything to FTN bundles
      string fido_out = StrCat("s", FTN_FAKE_OUTBOUND_NODE, ".net");
      if (File::Exists(dir, fido_out)) {
        System(create_network_cmdline(net_cmdline, 'f', verbose, "export"));
      }
    }

    // Process local mail with network2.
    if (File::Exists(StrCat(dir, LOCAL_NET))) {
      System(create_network_cmdline(net_cmdline, '2', verbose, ""));
    }

    // If our network files have changed, run network3 and send feedback.
    if (need_network3(dir, status->GetNetworkVersion())) {
      System(create_network_cmdline(net_cmdline, '3', verbose, ""));
    }

    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkf]: " << e.what();
  }
  return 2;
}
