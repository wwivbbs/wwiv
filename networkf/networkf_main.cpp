/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "networkf/networkf.h"

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fmt/format.h"
#include "net_core/net_cmdline.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/fido/fido_directories.h"
#include "sdk/fido/fido_packets.h"
#include "sdk/fido/fido_util.h"
#include "sdk/filenames.h"
#include "sdk/files/arc.h"
#include "sdk/net/ftn_msgdupe.h"
#include "sdk/net/packets.h"
#include "sdk/net/subscribers.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <signal.h>
#endif // _WIN32

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::os;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

using namespace wwiv::net::networkf;

int main(int argc, char** argv) {

#ifndef _WIN32
  // Set this to the default handling, since when wwivd invokes
  // this (and wwivd ignores SIGCHLD).
  signal(SIGCHLD, SIG_DFL);
#endif // !_WIN32

  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  CommandLine cmdline(argc, argv, "net");
  const NetworkCommandLine net_cmdline_(cmdline, 'f');
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    if (!net_cmdline_.IsInitialized() || net_cmdline_.cmdline().help_requested()) {
      ShowNetworkfHelp(net_cmdline_);
      return 1;
    }
    const auto& net = net_cmdline_.network();
    if (net.type != network_type_t::ftn) {
      LOG(ERROR) << "NETWORKF is only for use on FTN type networks.";
      ShowNetworkfHelp(net_cmdline_);
      return 1;
    }

    VLOG(3) << "Reading bbsdata.net_..";
    auto b = BbsListNet::ReadBbsDataNet(net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: Unable to read bbsdata.net_.";
      LOG(ERROR) << "       Do you need to run network3?";
      return 3;
    }

    const auto fake_ftn_node = b.node_config_for(FTN_FAKE_OUTBOUND_NODE);
    if (!fake_ftn_node) {
      LOG(ERROR) << "Can not find node for outbound FTN address.";
      LOG(ERROR) << "       Do you need to run network3?";
      return 2;
    }

    auto semaphore =
        SemaphoreFile::try_acquire(net_cmdline_.semaphore_path(), net_cmdline_.semaphore_timeout());
    SystemClock clock{};
    NetworkF nf(net_cmdline_, b, clock);
    return nf.Run() ? 0 : 2;
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline_.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkf]: " << e.what();
  }
  return 2;
}
