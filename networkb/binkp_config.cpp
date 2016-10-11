/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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
#include "networkb/binkp_config.h"

#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/textfile.h"
#include "sdk/networks.h"

using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv {
namespace net {

BinkConfig::BinkConfig(const std::string& callout_network_name, const Config& config, const Networks& networks)
    : callout_network_name_(callout_network_name), networks_(networks) {
  // network names will alwyas be compared lower case.
  StringLowerCase(&callout_network_name_);
  system_name_ = config.config()->systemname;
  if (system_name_.empty()) {
    system_name_ = "Unnamed WWIV BBS";
  }
  sysop_name_ = config.config()->sysopname;
  if (sysop_name_.empty()) {
    sysop_name_ = "Unknown WWIV SysOp";
  }
  gfiles_directory_ = config.config()->gfilesdir;

  if (networks.contains(callout_network_name)) {
    const net_networks_rec& net = networks[callout_network_name];
    callout_node_ = net.sysnum;
    if (callout_node_ == 0) {
      throw config_error(StringPrintf("NODE not specified for network: '%s'", callout_network_name.c_str()));
    }

    binkp_.reset(new Binkp(net.dir));
  }
}

const std::string BinkConfig::network_dir(const std::string& network_name) const {
  return networks_[network_name].dir;
}

static net_networks_rec test_net(const string& network_dir) {
  net_networks_rec net;
  net.sysnum = 1;
  strcpy(net.name, "wwivnet");
  net.type = net_type_wwivnet;
  strcpy(net.dir, network_dir.c_str());
  return net;
}

// For testing
BinkConfig::BinkConfig(int callout_node_number, const wwiv::sdk::Config& config, const string& network_dir)
  : callout_network_name_("wwivnet"), callout_node_(callout_node_number), 
    networks_({ test_net(network_dir) }) {
  binkp_.reset(new Binkp(network_dir));
  system_name_ = config.config()->systemname;
  sysop_name_ = config.config()->sysopname;
  gfiles_directory_ = config.config()->gfilesdir;
}

BinkConfig::~BinkConfig() {}

const BinkNodeConfig* BinkConfig::node_config_for(int node) const {
  if (!binkp_) { return nullptr; }
  return binkp_->node_config_for(node);
}

}  // namespace net
}  // namespace wwiv

