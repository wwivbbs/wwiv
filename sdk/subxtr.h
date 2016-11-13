/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#ifndef __INCLUDED_SUBXTR_H__
#define __INCLUDED_SUBXTR_H__

#include <vector>

#include "sdk/net.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {

// per-network message data or subboards.
struct subboard_network_data_t {
  std::string stype;
  int32_t flags = 0;
  int16_t net_num = 0;
  int16_t host = 0;
  int16_t category = 0;
};

// New (5.2+) style subboard. 
struct subboard_t {
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
  // how messages are stored
  uint16_t storage_type = 0;
  // per-network data type for networked subs.
  std::vector<subboard_network_data_t> nets;
};

struct subs_t {
  std::vector<subboard_t> subs;
};

class Subs {
public:
  Subs(const std::string& datadir, const std::vector<net_networks_rec>& net_networks);
  virtual ~Subs();

  bool Load();
  bool Save();

  const subboard_t& sub(std::size_t n) const { return subs_.at(n); }
  subboard_t& sub(std::size_t n) { return subs_[n]; }
  void set_sub(std::size_t n, subboard_t s) { subs_[n] = s; }
  const std::vector<subboard_t> subs() const { return subs_; }
  bool insert(std::size_t n, subboard_t r);
  bool erase(std::size_t n);
  std::vector<net_networks_rec>::size_type size() const { return subs_.size(); }

  static bool LoadFromJSON(const std::string& dir, const std::string& filename, subs_t& entries);
  static bool SaveToJSON(const std::string& dir, const std::string& filename, const subs_t& entries);


private:
  const std::string datadir_;
  const std::vector<net_networks_rec> net_networks_;
  std::vector<subboard_t> subs_;
};

// Not serialized as binary on disk.
/*
 * Info for each network the sub is on.
 *  flags - bitmask
 *  sess->network_num_ - index into networks.dat
 *  type - numeric sub type = atoi(stype)
 *  host - host system of sub, or 0 if locally hosted
 *  stype - string sub type (up to 7 chars)
 */
struct xtrasubsnetrec {
  long flags;
  int16_t net_num;
  int16_t host;
  int16_t category;
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
  long unused_xtra_flags;
  char desc[61];
  std::vector<xtrasubsnetrec> nets;
};

#define XTRA_NET_AUTO_ADDDROP 0x00000001    /* can auto add-drop the sub */
#define XTRA_NET_AUTO_INFO    0x00000002    /* sends subs.lst info for sub */

}
}


#endif // __INCLUDED_SUBXTR_H__

