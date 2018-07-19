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
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/datetime.h"
#include "sdk/net/packets.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {

void AddStandardNetworkArgs(wwiv::core::CommandLine& cmdline, const std::string& current_directory) {
  cmdline.add_argument({"net", "Network number to use (i.e. 0).", "0"});
  cmdline.add_argument({"bbsdir", "(optional) BBS directory if other than current directory", current_directory});
  cmdline.add_argument(BooleanCommandLineArgument("skip_net", "Skip invoking network1/network2/network3"));
  cmdline.add_argument(
      BooleanCommandLineArgument("skip_delete", "Don't delete packets, move din save area"));
}

NetworkCommandLine::NetworkCommandLine(wwiv::core::CommandLine& cmdline) : cmdline_(cmdline) {
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
}

std::unique_ptr<wwiv::core::IniFile> NetworkCommandLine::LoadNetIni(char net_cmd) const {
  const auto net_tag = StrCat("network", net_cmd);
  const auto net_tag_net = StrCat(net_tag, "-", network_name());

  File file(config().root_directory(), "net.ini");
  return std::unique_ptr<IniFile>(new IniFile(file.full_pathname(), { net_tag_net, net_tag }));
}

bool NetworkCommandLine::skip_delete() const { return cmdline_.barg("skip_delete"); }

bool NetworkCommandLine::skip_net() const { return cmdline_.barg("skip_delete"); }


}  // namespace net
}  // namespace wwiv

