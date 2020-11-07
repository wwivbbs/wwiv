/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2020, WWIV Software Services               */
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

#include "core/stl.h"
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
class NodelistEntry {
public:
  NodelistEntry() {}
  virtual ~NodelistEntry() {}

  static bool ParseDataLine(const std::string& data_line, NodelistEntry& e);

  // TODO(rushfan): private
public:
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
  explicit Nodelist(const std::filesystem::path& path);
  explicit Nodelist(const std::vector<std::string>& lines);
  ~Nodelist() = default;

  [[nodiscard]] bool initialized() const { return initialized_; }
  explicit operator bool() const { return initialized_; }

  [[nodiscard]] const NodelistEntry& entry(const FidoAddress& a) const { return entries_.at(a); }
  [[nodiscard]] bool contains(const FidoAddress& a) const { return wwiv::stl::contains(entries_, a); }
  [[nodiscard]] const std::map<FidoAddress, NodelistEntry>& entries() const { return entries_; }
  [[nodiscard]] std::vector<NodelistEntry> entries(uint16_t zone, uint16_t net) const;
  [[nodiscard]] std::vector<NodelistEntry> entries(uint16_t zone) const;
  [[nodiscard]] std::vector<uint16_t> zones() const;
  [[nodiscard]] std::vector<uint16_t> nets(uint16_t zone) const;
  [[nodiscard]] std::vector<uint16_t> nodes(uint16_t zone, uint16_t net) const;
  [[nodiscard]] const NodelistEntry* entry(uint16_t zone, uint16_t net, uint16_t node);

  static std::string FindLatestNodelist(const std::filesystem::path& dir, const std::string& base);

private:
  bool Load(const std::filesystem::path& path);
  bool Load(const std::vector<std::string>& lines);

  bool HandleLine(const std::string& line, uint16_t& zone, uint16_t& region, uint16_t& net, uint16_t& hub );
  std::map<FidoAddress, NodelistEntry> entries_;
  bool initialized_{false};
};

}  // namespace


#endif
