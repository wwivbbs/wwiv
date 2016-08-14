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
  int32_t flags;
  int16_t net_num;
  int16_t host;
  int16_t category;
};

// New (5.2+) style subboard. 
// This data is persisted in JSON, not on disk.
struct subboard_t {
  // board name
  std::string name;
  // long description - for subs.lst
  std::string desc;

  // board database filename
  std::string filename;
  // special key
  char key;

  // sl required to read
  uint8_t readsl;
  // sl required to post
  uint8_t postsl;
  // anonymous board?
  uint8_t anony;
  // minimum age for sub
  uint8_t age;

  // max # of msgs
  uint16_t maxmsgs;
  // AR for sub-board
  uint16_t ar;
  // how messages are stored
  uint16_t storage_type;
  // 4 digit board type
  uint16_t type;
  // per-network data type for networked subs.
  std::vector<subboard_network_data_t> nets;
};

// Wrapper for new 5.2+ style subboards
struct subs_t {
  std::vector<subboard_t> subs;
};

/*
 * Info for each network the sub is on.
 *  flags - bitmask
 *  sess->m_nNetworkNumber - index into networks.dat
 *  type - numeric sub type = atoi(stype)
 *  host - host system of sub, or 0 if locally hosted
 *  stype - string sub type (up to 7 chars)
 */

struct xtrasubsnetrec {
  long flags;
  int16_t net_num;
  int16_t host;
  int16_t category;
  char stype[8];
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


bool read_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec>& subs, std::vector<xtrasubsrec>& xsubs);
bool write_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<xtrasubsrec>& xsubs);

std::vector<subboardrec> read_subs(const std::string &datadir);
bool write_subs(const std::string &datadir, const std::vector<subboardrec>& subboards);

}
}


#endif // __INCLUDED_SUBXTR_H__

