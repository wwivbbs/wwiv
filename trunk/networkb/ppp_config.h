#pragma once
#ifndef __INCLUDED_NETWORKB_PPP_CONFIG_H__
#define __INCLUDED_NETWORKB_PPP_CONFIG_H__

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

struct PPPNodeConfig {
  std::string email_address;
};

class PPPConfig {
 public:
  PPPConfig(const std::string& network_name, const wwiv::sdk::Config& config, const wwiv::sdk::Networks& networks);
  virtual ~PPPConfig();
  const PPPNodeConfig* node_config_for(int node) const;

  uint16_t node_number() const { return node_; }
  const std::string& system_name() const { return system_name_; }
  const std::string& network_name() const { return network_name_; }
  const std::string& network_dir() const { return network_dir_; }

 private:
  std::map<uint16_t, PPPNodeConfig> node_config_;
  std::string home_dir_;

  uint16_t node_;
  std::string system_name_;
  std::string network_name_;
  std::string network_dir_;
};

bool ParseAddressNetLine(const std::string& line,
			 uint16_t* node,
			 PPPNodeConfig* config);

}  // namespace net
}  // namespace wwiv


#endif  // __INCLUDED_NETWORKB_PPP_CONFIG_H__
