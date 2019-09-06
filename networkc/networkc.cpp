/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2019, WWIV Software Services             */
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
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "net_core/net_cmdline.h"
#include "sdk/fido/fido_util.h"

#include "core/datetime.h"
#include "core/findfiles.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/connect.h"
#include "sdk/fido/fido_address.h"
#include "sdk/filenames.h"
#include "sdk/net/packets.h"
#include "sdk/networks.h"
#include "sdk/status.h"
#include "sdk/subscribers.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::os;
using namespace wwiv::sdk::fido;

static void ShowHelp(const NetworkCommandLine& cmdline) {
  cout << cmdline.GetHelp() << endl;
  exit(1);
}


static void rename_bbs_instance_files(const string& dir, int instance_number, bool quiet) {
  const auto pattern = StringPrintf("p*.%03d", instance_number);
  LOG_IF(!quiet, INFO) << "Processing pending bbs instance files: '" << pattern << "'";
  FindFiles ff(PathFilePath(dir, pattern), FindFilesType::files);
  for (const auto& f : ff) {
    rename_pend(dir, f.name, 'c');
  }
}

string create_network_cmdline(const NetworkCommandLine& net_cmdline, char num, const string& cmd) {
  const auto path = PathFilePath(net_cmdline.cmdline().bindir(), StrCat("network", num));

  std::ostringstream ss;
  ss << path;
  ss << " --v=" << net_cmdline.cmdline().verbose();
  if (net_cmdline.quiet()) {
    ss << " --quiet";
  }
  ss << " --bbsdir=" << net_cmdline.cmdline().bbsdir();
  ss << " --bindir=" << net_cmdline.cmdline().bindir();
  ss << " --configdir=" << net_cmdline.cmdline().configdir();
  ss << " ." << net_cmdline.network_number();
  if (num == '3') {
    ss << " Y";
  }
  if (!cmd.empty()) {
    ss << " " << cmd;
  }
  return ss.str();
}

static int System(const string& cmd) {
  VLOG(1) << "Command: " << cmd;
  return system(cmd.c_str());
}

static bool checkup2(const time_t tFileTime, string dir, string filename) {
  const auto fn = PathFilePath(dir, filename);
  File file(fn);

  if (file.Open(File::modeReadOnly)) {
    const auto tNewFileTime = File::last_write_time(fn);
    return tNewFileTime > (tFileTime + 2);
  }
  return true;
}

static bool need_network3(const string& dir, int network_version) {
  if (!File::Exists(PathFilePath(dir, BBSLIST_NET))) {
    return false;
  }
  if (!File::Exists(PathFilePath(dir, CONNECT_NET))) {
    return false;
  }
  if (!File::Exists(PathFilePath(dir, CALLOUT_NET))) {
    return false;
  }

  if (network_version != wwiv_net_version) {
    // always need network3 if the versions do not match.
    LOG(INFO) << "Need to run network3 since current network_version: " << network_version
              << " != our network_version: " << wwiv_net_version;
    return true;
  }
  File bbsdataNet(PathFilePath(dir, BBSDATA_NET));
  if (!bbsdataNet.Open(File::modeReadOnly)) {
    return false;
  }

  time_t bbsdata_time = bbsdataNet.last_write_time();
  bbsdataNet.Close();

  return checkup2(bbsdata_time, dir, BBSLIST_NET) || checkup2(bbsdata_time, dir, CONNECT_NET) ||
         checkup2(bbsdata_time, dir, CALLOUT_NET);
}

int networkc_main(const NetworkCommandLine& net_cmdline) {
  try {
    const auto process_instance = net_cmdline.cmdline().iarg("process_instance");
    const auto& net = net_cmdline.network();

    StatusMgr sm(net_cmdline.config().datadir(), [](int) {});
    auto status = sm.GetStatus();

    int num_tries = 0;
    bool found = false;
    do {
      found = false;
      if (process_instance > 0) {
        // We need to process pending bbs instance file, these are
        // of the form p1.###.  These will get renamed into p*.net
        rename_bbs_instance_files(net.dir, process_instance, net_cmdline.quiet());
      }

      // Pending files, call network1 to put them into s* or local.net.
      if (File::ExistsWildcard(FilePath(net.dir, "p*.net"))) {
        VLOG(2) << "Found p*.net";
        System(create_network_cmdline(net_cmdline, '1', ""));
        found = true;
      }

      // If the network type is a FTN network.
      if (net.type == network_type_t::ftn) {
        wwiv::sdk::fido::FtnDirectories dirs(net_cmdline.config().root_directory(), net);
        // Import everything into local.net
        if (File::ExistsWildcard(FilePath(dirs.inbound_dir(), "*.*"))) {
          VLOG(2) << "Trying to FTN import";
          System(create_network_cmdline(net_cmdline, 'f', "import"));
        }

        if (exists_bundle(net_cmdline.config(), net)) {
          VLOG(2) << "Trying to FTN export";
          System(create_network_cmdline(net_cmdline, 'f', "export"));
        }

        // Export everything to FTN bundles
        const auto fido_out = StrCat("s", FTN_FAKE_OUTBOUND_NODE, ".net");
        if (File::Exists(PathFilePath(net.dir, fido_out))) {
          VLOG(2) << "Found s" << FTN_FAKE_OUTBOUND_NODE << ".net; trying to export";
          System(create_network_cmdline(net_cmdline, 'f', "export"));
        }
      }

      // Process local mail with network2.
      if (File::Exists(PathFilePath(net.dir, LOCAL_NET))) {
        VLOG(2) << "Found: " << LOCAL_NET;
        System(create_network_cmdline(net_cmdline, '2', ""));
        found = true;
      }

      // If our network files have changed, run network3 and send feedback.
      if (need_network3(net.dir, status->GetNetworkVersion())) {
        VLOG(2) << "Need to run network3";
        System(create_network_cmdline(net_cmdline, '3', ""));
        found = true;
      }
    } while (found && ++num_tries < 3);

    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkf]: " << e.what();
  }
  return 2;
}

int main(int argc, char** argv) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.add_argument({"process_instance", "Also process pending files for BBS instance #", "0"});

  NetworkCommandLine net_cmdline(cmdline, 'c');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline);
    return 1;
  }
  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_path(),
                                                net_cmdline.semaphore_timeout());
    return networkc_main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}