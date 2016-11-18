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
#ifndef __INCLUDED_SDK_FIDO_FIDO_ADDRESS_H__
#define __INCLUDED_SDK_FIDO_FIDO_ADDRESS_H__

#include <cstdint>
#include <stdexcept>
#include <string>

namespace wwiv {
namespace sdk {
namespace fido {

/**
 Representes a FidoNet Address [FRL-1002] as a class.
 
 (from http://ftsc.org/docs/frl-1002.001)

 Fidonet addressing uses the following format:

 ZZ:NN/FF.PP@DO

 where the fields refer to...

 ZZ - Zone Number:  The zone the node is part of.
 Min: 1 Max: 32767
 If 'ZZ:' is missing then assume 1 as the zone.

 NN - Net Number:   The network the node is a member of.
 Min: 1 Max: 32767
 Must be present.

 FF - Node Number:  The actual node number.
 Min: -1 Max: 32767
 Must be present.

 PP - Point Number: If the system is a point rather than a node then
 this is their point number off the node.
 Min: 0 Max: 32767
 If '.PP' is missing then assume 0 (ie not a
 point) as the point number.

 DO - Domain:       The name of the 'Fidonet Technology Network'.
 Maximum length of 8 characters. The domain
 should not include periods, thus 'fidonet.org'
 is invalid (should be fidonet).
 If '@DO' is missing then fidonet can be assumed.

 The following are all valid examples:
 1:234/5.6@fidonet  (a '5D' address)   => 1:234/5.6@fidonet
 2:34/6.78          (a '4D' address)   => 2:34/6.78@fidonet
 4:610/34           (a '3D' address)   => 4:610/34.0@fidonet
 123/45             (a '2D' address)   => 1:123/45.0@fidonet
 955:95/2@othernet  (another FTN)      => 955:95/2.0@othernet
 2:259/-1           (node application) => 2:259/-1.0@fidonet

 The limits on each various part of the address are a result of
 fts-0005 (zone, net, node, point), fsc-0045 (domain) and Policy 4
 (-1 node address for node application).
 */
class FidoAddress {
public:
  /** Parses address.  If it fails, throws bad_fidonet_address. */
  explicit FidoAddress(const ::std::string& address);
  FidoAddress(): FidoAddress(0, 0, 0, 0, "") {}
  FidoAddress(int16_t zone, int16_t net, int16_t node, int16_t point, const ::std::string& domain)
    : zone_(zone), net_(net), node_(node), point_(point), domain_(domain) {}
  ~FidoAddress() {}

  ::std::string as_string(bool include_domain = false) const;
  int16_t zone() const { return zone_; }
  int16_t net() const { return net_; }
  int16_t node() const { return node_; }
  int16_t point() const { return point_; }
  ::std::string domain() const { return domain_; }

  // Needed to put FidoAddress into a set.
  inline bool operator< (const FidoAddress& r) const {
    if (zone_ < r.zone_) return true;
    if (net_ < r.net_) return true;
    if (node_ < r.node_) return true;
    if (point_ < r.point_) return true;
    if (domain_ < r.domain_) return true;
    return false;
  }
  inline bool operator== (const FidoAddress& o) const {
    if (zone_ != o.zone_) return false;
    if (net_ != o.net_) return false;
    if (node_ != o.node_) return false;
    if (point_ != o.point_) return false;
    if (domain_ != o.domain_) return false;
    return true;
  }

private:
  int16_t zone_ = 0;
  int16_t net_ = 0;
  int16_t node_ = 0;
  int16_t point_ = 0;
  ::std::string domain_;
};

class bad_fidonet_address: public ::std::runtime_error {
public:
  bad_fidonet_address(const ::std::string& message): ::std::runtime_error(message) {}
};


}  // namespace fido
}  // namespace sdk
}  // namespace wwiv

namespace std {
template <>
struct hash<wwiv::sdk::fido::FidoAddress> {
  size_t operator()(const wwiv::sdk::fido::FidoAddress& a) const {
    // See http://stackoverflow.com/a/1646913/126995
    size_t res = 17 * a.zone();
    res = res * 31 + a.net();
    res = res * 31 + a.node();
    res = res * 31 + a.point();
    if (!a.domain().empty()) {
      res = res * 31 + hash<std::string>()(a.domain());
    }
    return res;
  }
};
}  // namespace std

#endif  // __INCLUDED_SDK_FIDO_FIDO_ADDRESS_H__
