/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2016 WWIV Software Services                  */
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
#ifndef __INCLUDED_SDK_FIDO_NODELIST_H__
#define __INCLUDED_SDK_FIDO_NODELIST_H__

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "sdk/fido/fido_address.h"

/**
 * Classes to represent a FidoNet NodeList.
 *
 * FRL-1003 defines the format of the nodelist.
 * FTS-5001.004 defines the flags supported.
 */

namespace wwiv {
namespace sdk {
namespace fido {

// The 1st entry of the 8 mandatory ones is the keyword.
enum class NodelistKeyword {
  zone, region, host, hub, pvt, down, node
};

/**
 * Representes one data line in the nodelist.
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
  int16_t number_ = 0;
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
  // The rest are ignoded
  // IP, IFC, IFT, IVM, IN04
};

/**
 Representes a FidoNet NodeList as defined in FRL-1003.
 */
class Nodelist {
public:
  /** Parses address.  If it fails, throws bad_fidonet_address. */
  Nodelist(const std::string& path);
  Nodelist(const std::vector<std::string>& lines);
  virtual ~Nodelist();

  bool initialized() const { return initialized_; }
  explicit operator bool() const { return initialized_; }

  bool Load(const std::string& path);
  bool Load(const std::vector<std::string>& lines);
  const NodelistEntry& entry(const FidoAddress& a) const { return entries_.at(a); }
  const std::map<FidoAddress, NodelistEntry> entries() const { return entries_; }
  const std::vector<NodelistEntry> entries(int16_t zone, int16_t net) const;
  const std::vector<NodelistEntry> entries(int16_t zone) const;
  const std::vector<int16_t> zones() const;
  const std::vector<int16_t> nets(int16_t zone) const;
  const std::vector<int16_t> nodes(int16_t zone, int16_t net) const;
  const NodelistEntry* entry(int16_t zone, int16_t net, int16_t node);

private:

  bool HandleLine(const std::string& line, int16_t& zone, int16_t& region, int16_t& net, int16_t& hub );
  std::map<FidoAddress, NodelistEntry> entries_;
  bool initialized_ = false;
};


}  // namespace fido
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_FIDO_NODELIST_H__
