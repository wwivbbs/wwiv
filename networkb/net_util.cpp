/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2017, WWIV Software Services              */
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
#include "networkb/net_util.h"

#include <string>

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/net/packets.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {

void AddStandardNetworkArgs(wwiv::core::CommandLine& cmdline,
                            const std::string& current_directory) {
  cmdline.add_argument({"net", "Network number to use (i.e. 0).", "0"});
  cmdline.add_argument(
      {"bbsdir", "(optional) BBS directory if other than current directory", current_directory});
  cmdline.add_argument(
      BooleanCommandLineArgument("skip_net", "Skip invoking network1/network2/network3"));
  cmdline.add_argument(
      BooleanCommandLineArgument("skip_delete", "Don't delete packets, move to save area"));
  cmdline.add_argument({"semaphore_timeout",
      "Timeout (in seconds) to wait for the network semaphore to become available.",
      "30"});
}

NetworkCommandLine::NetworkCommandLine(wwiv::core::CommandLine& cmdline, char net_cmd)
    : cmdline_(cmdline), net_cmd_(net_cmd) {
  cmdline.set_no_args_allowed(true);
  cmdline.AddStandardArgs();
  AddStandardNetworkArgs(cmdline, File::current_directory());

  if (!cmdline.Parse()) {
    initialized_ = false;
  }
  bbsdir_ = cmdline.arg("bbsdir").as_string();
  network_number_ = cmdline.arg("net").as_int();

  config_.reset(new wwiv::sdk::Config(bbsdir_));
  networks_.reset(new wwiv::sdk::Networks(*config_.get()));

  if (!config_->IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT.";
    initialized_ = false;
  }
  if (!networks_->IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    initialized_ = false;
  }
  const auto& nws = networks_->networks();

  if (network_number_ < 0 || network_number_ >= size_int(nws)) {
    LOG(ERROR) << "network number must be between 0 and " << nws.size() << ".";
    initialized_ = false;
    return;
  }
  network_ = nws[network_number_];
  network_name_ = ToStringLowerCase(network_.name);
  LOG(INFO) << cmdline.program_name() << " [" << wwiv_version << beta_version << "]"
            << " for network: " << network_name_;

  if (!LoadNetIni()) {
    LOG(ERROR) << "Error loading INI file for defaults";
  }
}

// Returns the name of the network command for the command character
// e.g. returns "network1" for '1', etc.  If 0 or '\0' then it returns
// "network".
static std::string network_cmd_name(char net_cmd) {
  return (net_cmd == '\0') ? "network" : StrCat("network", net_cmd);
}

std::string NetworkCommandLine::semaphore_filename() const noexcept {
  return StrCat(network_.dir, network_cmd_name(net_cmd_), ".bsy");
}

bool NetworkCommandLine::LoadNetIni() {
  const auto net_tag = network_cmd_name(net_cmd_);
  const auto net_tag_net = StrCat(net_tag, "-", network_name());

  auto ini = std::unique_ptr<IniFile>(
      new IniFile(FilePath(config().root_directory(), "net.ini"), {net_tag_net, net_tag}));
  if (!ini || !ini->IsOpen()) {
    // This is fine and can happen.
    return true;
  }

  if (cmdline_.contains_arg("skip_net") && cmdline().arg("skip_net").is_default()) {
    auto skip_net = ini->value<bool>("skip_net", cmdline().barg("skip_net"));
    cmdline_.SetNewDefault("skip_net", skip_net ? "Y" : "N");
  }
  if (cmdline_.contains_arg("crc") && cmdline().arg("crc").is_default()) {
    auto skip_net = ini->value<bool>("crc", cmdline().barg("crc"));
    cmdline_.SetNewDefault("crc", skip_net ? "Y" : "N");
  }
  if (cmdline_.contains_arg("cram_md5") && cmdline().arg("cram_md5").is_default()) {
    auto skip_net = ini->value<bool>("cram_md5", cmdline().barg("cram_md5"));
    cmdline_.SetNewDefault("cram_md5", skip_net ? "Y" : "N");
  }
  if (cmdline_.contains_arg("semaphore_timeout") && cmdline().arg("semaphore_timeout").is_default()) {
    auto semaphore_timeout =
        ini->value<int>("semaphore_timeout", cmdline().iarg("semaphore_timeout"));
    cmdline_.SetNewDefault("semaphore_timeout", std::to_string(semaphore_timeout));
  }

  return true;
}

bool NetworkCommandLine::skip_delete() const noexcept { return cmdline_.barg("skip_delete"); }

bool NetworkCommandLine::skip_net() const noexcept { return cmdline_.barg("skip_net"); }

std::chrono::duration<double> NetworkCommandLine::semaphore_timeout() const noexcept {
  auto semaphore_timeout = cmdline_.iarg("semaphore_timeout");
  return std::chrono::seconds(semaphore_timeout);
}


} // namespace net
} // namespace wwiv
