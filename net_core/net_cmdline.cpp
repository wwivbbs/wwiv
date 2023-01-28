/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2022, WWIV Software Services              */
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
#include "net_core/net_cmdline.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/net/packets.h"
#include <filesystem>
#include <iomanip>
#include <ostream>
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::net {

void AddStandardNetworkArgs(wwiv::core::CommandLine& cmdline) {
  cmdline.add_argument({"net", "Network number to use (i.e. 0).", "0"});
  cmdline.add_argument(
      BooleanCommandLineArgument("skip_net", "Skip invoking network1/network2/network3"));
  cmdline.add_argument(
      BooleanCommandLineArgument("skip_delete", "Don't delete packets, move to save area"));
  cmdline.add_argument(
      BooleanCommandLineArgument("quiet", "Be more quiet and don't log entry/exit"));
  cmdline.add_argument(
      {"semaphore_timeout",
      "Timeout (in seconds) to wait for the network semaphore to become available.",
      "30"});
  cmdline.add_argument(BooleanCommandLineArgument("help", 'h', "Displays Help", false));
}

NetworkCommandLine::NetworkCommandLine(wwiv::core::CommandLine& cmdline, char net_cmd)
    : cmdline_(cmdline), net_cmd_(net_cmd) {
  cmdline.set_no_args_allowed(true);
  cmdline.AddStandardArgs();
  AddStandardNetworkArgs(cmdline);

  if (!cmdline.Parse()) {
    initialized_ = false;
  }

  if (!LoadNetIni(net_cmd, cmdline.bbsdir())) {
    LOG(ERROR) << "Error loading INI file for defaults";
  }

  network_number_ = cmdline.arg("net").as_int();
  // TODO(rushfan): Need to look to see if WWIV_CONFIG_FILE is set 1st.
  config_ = sdk::load_any_config(cmdline.bbsdir());
  if (!config_) {
    LOG(ERROR) << "Unable to load config.json or config.dat";
    initialized_ = false;
    return;
  }
  networks_.reset(new sdk::Networks(*config_));

  if (!config_->IsInitialized()) {
    LOG(ERROR) << "Unable to load config.json.";
    initialized_ = false;
  }
  if (!networks_->IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    initialized_ = false;
  }
  const auto& nws = networks_->networks();

  if (nws.size()==0) {
    LOG(ERROR) << "Networks need to be set up first.";
    initialized_ = false;
    return;
  }

  if (network_number_ < 0 || network_number_ >= size_int(nws)) {
    LOG(ERROR) << "network number must be between 0 and " << nws.size()-1 << ".";
    initialized_ = false;
    return;
  }
  network_ = nws[network_number_];
  network_name_ = ToStringLowerCase(network_.name);
  LOG(STARTUP) << cmdline.program_name() << " [" << full_version() << "]"
               << " for network: " << network_name_;
  if (!quiet()) {
    std::cerr << cmdline.program_name() << " [" << full_version() << "]"
              << " for network: " << network_name_ << std::endl;
  }
}

// Returns the name of the network command for the command character
// e.g. returns "network1" for '1', etc.  If 0 or '\0' then it returns
// "network".
static std::string network_cmd_name(char net_cmd) {
  return (net_cmd == '\0') ? "network" : StrCat("network", net_cmd);
}

std::filesystem::path NetworkCommandLine::semaphore_path() const noexcept {
  return FilePath(network_.dir, StrCat(network_cmd_name(net_cmd_), ".bsy"));
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool NetworkCommandLine::LoadNetIni(char net_cmd, const std::string& bbsdir) {
  const auto net_tag = network_cmd_name(net_cmd);
  const auto net_tag_net = StrCat(net_tag, "-", network_name());

  const auto ini = std::make_unique<IniFile>(
      FilePath(bbsdir, "net.ini"), net_tag_net, net_tag);
  if (!ini || !ini->IsOpen()) {
    // This is fine and can happen.
    return true;
  }

  SetNewBooleanDefault(cmdline_, *ini, "skip_net");
  SetNewBooleanDefault(cmdline_, *ini, "crc");
  SetNewBooleanDefault(cmdline_, *ini, "cram_md5");
  SetNewBooleanDefault(cmdline_, *ini, "quiet");
  SetNewIntDefault(cmdline_, *ini, "semaphore_timeout");
  SetNewIntDefault(cmdline_, *ini, "v", [](int v) { Logger::set_cmdline_verbosity(v); });
  SetNewBooleanDefault(cmdline_, *ini, "skip_delete");
  SetNewStringDefault(cmdline_, *ini, "configdir");
  SetNewStringDefault(cmdline_, *ini, "bindir");
  SetNewStringDefault(cmdline_, *ini, "logdir");

  // Reparse the commandline args to update the variables.
  cmdline_.Parse();
  return true;
}

bool NetworkCommandLine::skip_delete() const noexcept { return cmdline_.barg("skip_delete"); }

bool NetworkCommandLine::skip_net() const noexcept { return cmdline_.barg("skip_net"); }

bool NetworkCommandLine::quiet() const noexcept { return cmdline_.barg("quiet"); }

std::string NetworkCommandLine::GetHelp() const {
  std::ostringstream ss;
  ss << cmdline_.GetHelp() << std::endl
     << "   .#### " << std::setw(22) << " "
     << "Network number (as defined in WWIVconfig)" << std::endl;
  return ss.str();
}

std::chrono::duration<double> NetworkCommandLine::semaphore_timeout() const noexcept {
  const auto semaphore_timeout = cmdline_.iarg("semaphore_timeout");
  return std::chrono::seconds(semaphore_timeout);
}


} // namespace wwiv
