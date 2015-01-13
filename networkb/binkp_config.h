#pragma once
#ifndef __INCLUDED_NETWORKB_BINKP_CONFIG_H__
#define __INCLUDED_NETWORKB_BINKP_CONFIG_H__

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "core/inifile.h"
#include "networkb/config_exceptions.h"
#include "sdk/networks.h"

namespace wwiv {
namespace net {

struct BinkNodeConfig {
  std::string host;
  int port;
};

class BinkConfig {
 public:
  BinkConfig(const std::string& callout_network_name, const wwiv::sdk::Config& config, const wwiv::sdk::Networks& networks);
  BinkConfig(int node_number, const std::string& system_name, const std::string& network_dir);
  virtual ~BinkConfig();
  const BinkNodeConfig* node_config_for(int node) const;

  uint16_t callout_node_number() const { return callout_node_; }
  const std::string system_name() const { return system_name_; }
  const std::string callout_network_name() const { return callout_network_name_; }
  const std::string network_dir(const std::string& network_name) const;
  const wwiv::sdk::Networks& networks() { return networks_; }

  void set_skip_net(bool skip_net) { skip_net_ = skip_net; }
  bool skip_net() const { return skip_net_; }

 private:
  std::map<uint16_t, BinkNodeConfig> node_config_;
  std::string home_dir_;

  uint16_t callout_node_;
  std::string system_name_;
  std::string callout_network_name_;
  const wwiv::sdk::Networks networks_;
  bool skip_net_;
};

bool ParseBinkConfigLine(const std::string& line,
			 uint16_t* node,
			 BinkNodeConfig* config);

}  // namespace net
}  // namespace wwiv


#endif  // __INCLUDED_NETWORKB_BINKP_CONFIG_H__
