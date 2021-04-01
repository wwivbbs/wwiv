/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/

#ifndef INCLUDED_SDK_SUBXTR_H
#define INCLUDED_SDK_SUBXTR_H

#include "core/stl.h"
#include "fido/fido_address.h"
#include "sdk/net/net.h"
#include "sdk/conf/conf_set.h"
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::sdk {

// per-network message data or subboards.
struct subboard_network_data_t {
  std::string stype;
  int32_t flags = 0;
  int16_t net_num = 0;
  int16_t host = 0;
  int16_t category = 0;
  std::set<fido::FidoAddress> ftn_uplinks;
};

// New (5.6+) style sub-board. 
struct subboard_t {
  // sub name
  std::string name;
  // long description - for subs.lst
  std::string desc;

  // board database filename
  std::string filename;
  // special key
  char key{0};

  // sl required to read
  std::string read_acs;
  // sl required to post
  std::string post_acs;
  // anonymous board?
  uint8_t anony{0};

  // max # of msgs
  uint16_t maxmsgs{0};
  // how it is stored (type, 1 or 2)
  uint8_t storage_type{0};
  // per-network data type for networked subs.
  std::vector<subboard_network_data_t> nets;
  // Conference keys.
  conf_set_t conf;
};

// 5.2 style sub-board. 
struct subboard_52_t {
  // sub name
  std::string name;
  // long description - for subs.lst
  std::string desc;

  // board database filename
  std::string filename;
  // special key
  char key = 0;

  // sl required to read
  uint8_t readsl = 0;
  // sl required to post
  uint8_t postsl = 0;
  // anonymous board?
  uint8_t anony = 0;
  // minimum age for sub
  uint8_t age = 0;

  // max # of msgs
  uint16_t maxmsgs = 0;
  // AR for sub-board
  uint16_t ar = 0;
  // how it is stored (type, 1 or 2)
  uint8_t storage_type = 0;
  // per-network data type for networked subs.
  std::vector<subboard_network_data_t> nets;
};

class Subs final {
public:
  Subs(std::string datadir, const std::vector<net::Network>& net_networks, int max_backups = 0);
  ~Subs();

  bool LoadLegacy();
  bool Load();
  bool Save();

  [[nodiscard]] const subboard_t& sub(std::size_t n) const { return stl::at(subs_, n); }
  [[nodiscard]] const subboard_t& sub(const std::string& filename) const;
  subboard_t& sub(std::size_t n) { return subs_[n]; }
  subboard_t& sub(const std::string& filename);

  const subboard_t& operator[](std::size_t n) const { return sub(n); }
  const subboard_t& operator[](const std::string& filename) const { return sub(filename); }
  subboard_t& operator[](std::size_t n) { return sub(n); }
  subboard_t& operator[](const std::string& filename) { return sub(filename); }

  [[nodiscard]] bool exists(const std::string& filename) const;

  void set_sub(int n, subboard_t s) { subs_[n] = std::move(s); }
  [[nodiscard]] const std::vector<subboard_t>& subs() const { return subs_; }
  bool insert(int n, subboard_t r);
  bool add(subboard_t r);
  bool erase(int n);
  [[nodiscard]] int size() const { return stl::size_int(subs_); }

  static bool LoadFromJSON(const std::filesystem::path& dir, const std::string& filename, std::vector<subboard_t>& entries);
  static bool SaveToJSON(const std::filesystem::path& dir, const std::string& filename, const std::vector<subboard_t>& entries);


private:
  const std::string datadir_;
  const std::vector<net::Network> net_networks_;
  const int max_backups_;
  std::vector<subboard_t> subs_;
};

// Not serialized as binary on disk.
/*
 * Info for each network the sub is on.
 *  flags - bitmask
 *  net_num - index into networks.dat
 *  type - numeric sub type = to_number<int>(stype)
 *  host - host system of sub, or 0 if locally hosted
 *  stype - string sub type (up to 7 chars)
 */
struct xtrasubsnetrec {
  long flags{0};
  int16_t net_num{-1};
  int16_t host{-1};
  int16_t category{-1};
  std::string stype_str;
};


/*
 * Extended info for each sub, relating to network.
 * Note: This structure is not loaded/saved to disk as-is.
 *  flags - bitmask
 *  desc - long description, for auto subs.lst info
 *  nets - vector of network info for sub
 */
struct xtrasubsrec {
  long unused_xtra_flags{0};
  char desc[61]{};
  std::vector<xtrasubsnetrec> nets;
};

#define XTRA_NET_AUTO_ADDDROP 0x00000001    /* can auto add-drop the sub */
#define XTRA_NET_AUTO_INFO    0x00000002    /* sends subs.lst info for sub */

}


#endif
