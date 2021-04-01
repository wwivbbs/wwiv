/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_FIDO_NODELIST_H
#define INCLUDED_SDK_FIDO_NODELIST_H

#include "sdk/fido/fido_address.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

/**
 * Classes to represent a FidoNet NodeList.
 *
 * FRL-1003 defines the format of the nodelist.
 * FTS-5001.004 defines the flags supported.
 */

namespace wwiv::sdk::fido {

// The 1st entry of the 8 mandatory ones is the keyword.
enum class NodelistKeyword {
  zone, region, host, hub, pvt, down, node
};

/**
 * Represents one data line in the nodelist.
 */
class NodelistEntry final {
public:
  NodelistEntry() = default;
  ~NodelistEntry() = default;

  static std::optional<NodelistEntry> ParseDataLine(const std::string& data_line);

  [[nodiscard]] FidoAddress address() const { return address_; }
  void address(const FidoAddress& a) { address_ = a; }
  [[nodiscard]] NodelistKeyword keyword() const { return keyword_; }
  [[nodiscard]] uint16_t number() const { return number_; }
  [[nodiscard]] std::string name() const { return name_; }
  [[nodiscard]] std::string location() const { return location_; }
  [[nodiscard]] std::string sysop_name() const { return sysop_name_; }
  [[nodiscard]] std::string phone_number() const { return phone_number_; }
  [[nodiscard]] uint32_t baud_rate() const { return baud_rate_; }
  [[nodiscard]] bool  cm() const { return cm_; }
  [[nodiscard]] bool  icm() const { return icm_; }
  [[nodiscard]] bool  mo() const { return mo_; }
  [[nodiscard]] bool  lo() const { return lo_; }
  [[nodiscard]] bool  mn() const { return mn_; }
  [[nodiscard]] bool  bark_file() const { return bark_file_; }
  [[nodiscard]] bool  bark_update() const { return bark_update_; }
  [[nodiscard]] bool  wazoo_file() const { return wazoo_file_; }
  [[nodiscard]] bool  wazoo_update() const { return wazoo_update_; }
  [[nodiscard]] std::string hostname() const { return hostname_; }

  [[nodiscard]] bool  binkp() const { return binkp_; }
  [[nodiscard]] uint32_t binkp_port() const { return binkp_port_; }
  [[nodiscard]] std::string binkp_hostname() const { return binkp_hostname_; }

  [[nodiscard]] bool  telnet() const { return telnet_; }
  [[nodiscard]] uint32_t telnet_port() const { return telnet_port_; }
  [[nodiscard]] std::string telnet_hostname() const { return telnet_hostname_; }

  [[nodiscard]] bool  vmodem() const { return vmodem_; }
  [[nodiscard]] uint32_t vmodem_port() const { return vmodem_port_; }
  [[nodiscard]] std::string vmodem_hostname() const { return vmodem_hostname_; }

private:
  FidoAddress address_;
  NodelistKeyword keyword_ = NodelistKeyword::node;
  uint16_t number_ = 0;
  // If the bbs supports internet access, the hostname
  // should be used here instead of the name.
  std::string name_;
  std::string location_;
  std::string sysop_name_;
  std::string phone_number_;
  uint32_t baud_rate_ = 0;
  // flags.
  bool cm_ = false;
  bool icm_ = false;
  bool mo_ = false;
  bool lo_ = false;
  bool mn_ = false;

  /*
   * Capabilities defined by the X{A,B,C,P,R,W,X} flags.
   */
  bool bark_file_ = false;
  bool bark_update_ = false;
  bool wazoo_file_ = false;
  bool wazoo_update_ = false;

  //
  // Internet flags.
  //

  // INA: Hostname to use (without port) for all services.
  std::string hostname_;
  // IBN: BinkP over internet
  bool binkp_ = false;
  uint16_t binkp_port_ = 0;
  std::string binkp_hostname_;
  // ITN: FTS-0001 or later over telnet.
  bool telnet_ = false;
  uint16_t telnet_port_ = 0;
  std::string telnet_hostname_;
  // IVM: FTS-0001 or later over telnet.
  bool vmodem_ = false;
  uint16_t vmodem_port_ = 0;
  std::string vmodem_hostname_;
  // The rest are ignored
  // IP, IFC, IFT, IVM, IN04
};

/**
 * Represents a FidoNet NodeList as defined in FRL-1003.
 */
class Nodelist final {
public:
  /** Parses address.  If it fails, throws bad_fidonet_address. */
  Nodelist(const std::filesystem::path& path, std::string domain);
  Nodelist(const std::vector<std::string>& lines, std::string domain);
  ~Nodelist() = default;

  [[nodiscard]] bool initialized() const { return initialized_; }
  explicit operator bool() const { return initialized_; }

  [[nodiscard]] const NodelistEntry& entry(const FidoAddress& a) const;
  [[nodiscard]] bool contains(const FidoAddress& a) const;
  [[nodiscard]] const std::map<FidoAddress, NodelistEntry>& entries() const { return entries_; }
  [[nodiscard]] std::vector<NodelistEntry> entries(uint16_t zone, uint16_t net) const;
  [[nodiscard]] std::vector<NodelistEntry> entries(uint16_t zone) const;
  [[nodiscard]] std::vector<uint16_t> zones() const;
  [[nodiscard]] std::vector<uint16_t> nets(uint16_t zone) const;
  [[nodiscard]] std::vector<uint16_t> nodes(uint16_t zone, uint16_t net) const;
  [[nodiscard]] const NodelistEntry* entry(uint16_t zone, uint16_t net, uint16_t node);
  [[nodiscard]] bool has_zone(int zone) const noexcept;

  static std::string FindLatestNodelist(const std::filesystem::path& dir, const std::string& base);

private:
  bool Load(const std::filesystem::path& path);
  bool Load(const std::vector<std::string>& lines);

  bool HandleLine(const std::string& line, uint16_t& zone, uint16_t& region, uint16_t& net, uint16_t& hub );
  std::map<FidoAddress, NodelistEntry> entries_;
  std::string domain_;
  bool initialized_{false};
};

}  // namespace


#endif
