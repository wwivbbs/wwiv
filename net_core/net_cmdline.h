/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2019, WWIV Software Services               */
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
#ifndef __INCLUDED_NETWORKB_NET_UTIL_H__
#define __INCLUDED_NETWORKB_NET_UTIL_H__

#include <chrono>
#include <memory>
#include <string>

#include "core/command_line.h"
#include <filesystem>
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"

namespace wwiv {
namespace net {

void AddStandardNetworkArgs(wwiv::core::CommandLine& cmdline);

/**
 * Wrapper class that augments CommandLine to specialize it for the network commands.
 */
class NetworkCommandLine {
public:
  NetworkCommandLine(wwiv::core::CommandLine& cmdline, char net_cmd);

  bool IsInitialized() const noexcept { return initialized_; }
  const wwiv::sdk::Config& config() const noexcept { return *config_.get(); }
  const wwiv::sdk::Networks& networks() const noexcept { return *networks_.get(); }
  std::string network_name() const noexcept { return network_name_; }
  int network_number() const noexcept { return network_number_; }
  const net_networks_rec& network() const noexcept { return network_; }
  const wwiv::core::CommandLine& cmdline() const noexcept { return cmdline_; }
  char net_cmd() const noexcept { return net_cmd_; }
  std::filesystem::path semaphore_path() const noexcept;
  std::string GetHelp() const;

  /** Process net.ini and reparse command line applying new defaults */
  bool LoadNetIni(char net_cmd, const std::string& bbsdir);
  bool skip_delete() const noexcept;
  bool skip_net() const noexcept;
  bool quiet() const noexcept;

  std::chrono::duration<double> semaphore_timeout() const noexcept;

private:
  std::unique_ptr<wwiv::sdk::Config> config_;
  std::unique_ptr<wwiv::sdk::Networks> networks_;
  std::string network_name_;
  int network_number_{0};
  bool initialized_{true};
  net_networks_rec network_;
  wwiv::core::CommandLine& cmdline_;
  char net_cmd_;
};

} // namespace net
} // namespace wwiv

#endif // __INCLUDED_NETWORKB_NET_UTIL_H__
