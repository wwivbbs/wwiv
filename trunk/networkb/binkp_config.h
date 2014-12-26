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
  BinkConfig(const std::string& network_name, const wwiv::sdk::Config& config, const wwiv::sdk::Networks& networks);
  BinkConfig(int node_number, const std::string& system_name, const std::string& network_dir);
  virtual ~BinkConfig();
  const BinkNodeConfig* node_config_for(int node) const;

  uint16_t node_number() const { return node_; }
  const std::string& system_name() const { return system_name_; }
  const std::string& network_name() const { return network_name_; }
  const std::string& network_dir() const { return network_dir_; }

 private:
  std::map<uint16_t, BinkNodeConfig> node_config_;
  std::string home_dir_;

  uint16_t node_;
  std::string system_name_;
  std::string network_name_;
  std::string network_dir_;
};

bool ParseBinkConfigLine(const std::string& line,
			 uint16_t* node,
			 BinkNodeConfig* config);

}  // namespace net
}  // namespace wwiv


#endif  // __INCLUDED_NETWORKB_BINKP_CONFIG_H__
