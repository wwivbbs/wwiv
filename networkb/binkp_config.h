#pragma once
#ifndef __INCLUDED_NETWORKB_BINKP_CONFIG_H__
#define __INCLUDED_NETWORKB_BINKP_CONFIG_H__

#include <cstdint>
#include <exception>
#include <map>
#include <string>

namespace wwiv {
namespace net {

struct BinkNodeConfig {
  std::string host;
  int port;
  std::string password;
};

class BinkConfig {
 public:
  explicit BinkConfig(const std::string& config_file);
  virtual ~BinkConfig();
  const BinkNodeConfig* node_config_for(int node);

 private:
  std::string config_file_;
  std::map<uint16_t, BinkNodeConfig> node_config_;
  std::string home_dir_;
};

struct config_error : public std::runtime_error {
 config_error(const std::string& message) : std::runtime_error(message) {}
};

bool ParseBinkConfigLine(const std::string& line,
			 uint16_t* node,
			 BinkNodeConfig* config);

}  // namespace net
}  // namespace wwiv


#endif  // __INCLUDED_NETWORKB_BINKP_CONFIG_H__
