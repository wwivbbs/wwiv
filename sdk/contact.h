/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016 WWIV Software Services              */
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
#ifndef __INCLUDED_SDK_CONTACT_H__
#define __INCLUDED_SDK_CONTACT_H__

#include <ctime>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "core/wwivport.h"
#include "sdk/net.h"

namespace wwiv {
namespace sdk {

struct network_contact_record {
  std::string address;
  net_contact_rec ncr;
};

static inline network_contact_record to_network_contact_record(const net_contact_rec& n) {
  network_contact_record ncr{};
  ncr.address = std::to_string(n.systemnumber);
  ncr.ncr = n;
  return ncr;
}

class NetworkContact {
public:
  NetworkContact(): ncr_() {}
  explicit NetworkContact(uint16_t node): ncr_() { ncr_.ncr.systemnumber = node; ncr_.address = std::to_string(node); }
  explicit NetworkContact(const std::string& node): ncr_() { ncr_.address = node; }
  explicit NetworkContact(const net_contact_rec& ncr): ncr_(to_network_contact_record(ncr)) {}
  explicit NetworkContact(const wwiv::sdk::network_contact_record& ncr): ncr_(ncr) {}
  ~NetworkContact() {}

  const std::string address() const { return ncr_.address; }
  uint16_t systemnumber() const { return ncr_.ncr.systemnumber; }
  uint16_t numcontacts() const { return ncr_.ncr.numcontacts; }
  uint16_t numfails() const { return ncr_.ncr.numfails; }
  daten_t firstcontact() const { return ncr_.ncr.firstcontact; }
  daten_t lastcontact() const { return ncr_.ncr.lastcontact; }
  daten_t lastcontactsent() const { return ncr_.ncr.lastcontactsent; }
  daten_t lasttry() const { return ncr_.ncr.lasttry; }
  uint32_t bytes_received() const { return ncr_.ncr.bytes_received; }
  uint32_t bytes_sent() const { return ncr_.ncr.bytes_sent; }
  uint32_t bytes_waiting() const { return ncr_.ncr.bytes_waiting; }
  void set_bytes_waiting(int32_t bw) { ncr_.ncr.bytes_waiting = bw; }

  void fixup();
  void AddContact(time_t t);
  void AddConnect(time_t t, uint32_t bytes_sent, uint32_t bytes_received);
  void AddFailure(time_t t);
private:
  network_contact_record ncr_{};
};

static_assert(sizeof(network_contact_record) == sizeof(NetworkContact),
  "network_contact_record == NetworkContact");

 /**
  * Class for manipulating CONTACT.NET
  */
class Contact {
 public:
  Contact(const std::string& network_dir, bool save_on_destructor);
  // VisibleForTesting
  Contact(std::initializer_list<NetworkContact> l);
  virtual ~Contact();

  // Was this list initialized properly.
  bool IsInitialized() const { return initialized_; }
  // returns a mutable net_contact_rec for system number "node"
  NetworkContact* contact_rec_for(uint16_t node);
  NetworkContact* contact_rec_for(const std::string& node);
  void ensure_rec_for(uint16_t node);
  void ensure_rec_for(const std::string& node);

  /** add a connection to node, including bytes send and received. */
  void add_connect(int node, time_t time, uint32_t bytes_sent, uint32_t bytes_received);
  void add_connect(const std::string& node, time_t time, uint32_t bytes_sent, uint32_t bytes_received);
  /** add a failure attempt to node */
  void add_failure(int node, time_t time);
  void add_failure(const std::string& node, time_t time);

  bool Save();
  std::string ToString() const;

 private:
   /** add a contact. called by connect or failure. */
   void add_contact(NetworkContact* c, time_t time);

   std::string network_dir_;
   bool save_on_destructor_;
   std::vector<NetworkContact> contacts_;
   bool initialized_;
};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_SDK_CONTACT_H__
