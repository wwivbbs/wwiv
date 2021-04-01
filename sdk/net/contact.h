/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_CONTACT_H
#define INCLUDED_SDK_CONTACT_H

#include "core/datetime.h"
#include "core/wwivport.h"
#include "sdk/net/net.h"
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>

namespace wwiv::sdk {

struct network_contact_record {
  std::string address;
  net_contact_rec ncr{};
};

network_contact_record to_network_contact_record(const net_contact_rec& n);

class NetworkContact {
public:
  NetworkContact() noexcept : ncr_() {}
  explicit NetworkContact(uint16_t node): ncr_() { 
    ncr_.ncr.systemnumber = node;
    ncr_.address = CreateFakeFtnAddress(node); 
  }
  explicit NetworkContact(const std::string& address): ncr_() { ncr_.address = address; }
  explicit NetworkContact(const net_contact_rec& ncr): ncr_(to_network_contact_record(ncr)) {}
  explicit NetworkContact(wwiv::sdk::network_contact_record ncr): ncr_(std::move(ncr)) {}
  ~NetworkContact() = default;

  [[nodiscard]] std::string address() const { return ncr_.address; }
  [[nodiscard]] uint16_t systemnumber() const { return ncr_.ncr.systemnumber; }
  [[nodiscard]] uint16_t numcontacts() const { return ncr_.ncr.numcontacts; }
  [[nodiscard]] uint16_t numfails() const { return ncr_.ncr.numfails; }
  [[nodiscard]] daten_t firstcontact() const { return ncr_.ncr.firstcontact; }
  [[nodiscard]] daten_t lastcontact() const { return ncr_.ncr.lastcontact; }
  [[nodiscard]] daten_t lastcontactsent() const { return ncr_.ncr.lastcontactsent; }
  [[nodiscard]] daten_t lasttry() const { return ncr_.ncr.lasttry; }
  [[nodiscard]] uint32_t bytes_received() const { return ncr_.ncr.bytes_received; }
  [[nodiscard]] uint32_t bytes_sent() const { return ncr_.ncr.bytes_sent; }
  [[nodiscard]] uint32_t bytes_waiting() const { return ncr_.ncr.bytes_waiting; }
  void set_bytes_waiting(int32_t bw) { ncr_.ncr.bytes_waiting = bw; }

  void fixup();
  void AddContact(const wwiv::core::DateTime& t);
  void AddConnect(const wwiv::core::DateTime& t, uint32_t bytes_sent, uint32_t bytes_received);
  void AddFailure(const wwiv::core::DateTime& t);

  [[nodiscard]] const net_contact_rec& ncr() const { return ncr_.ncr; }

  static std::string CreateFakeFtnAddress(int node);

private:
  network_contact_record ncr_{};
};

static_assert(sizeof(network_contact_record) == sizeof(NetworkContact),
  "network_contact_record == NetworkContact");

 /**
  * Class for manipulating contact.net
  */
class Contact {

 public:
  explicit Contact(const net::Network& net);
  Contact(net::Network net, bool save_on_destructor);
   // VisibleForTesting
  Contact(net::Network net, std::initializer_list<NetworkContact> l);
  virtual ~Contact();

  // Was this list initialized properly.
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  // returns a mutable net_contact_rec for system number "node"
  [[nodiscard]] NetworkContact* contact_rec_for(int node);
  [[nodiscard]] NetworkContact* contact_rec_for(const std::string& node);
  void ensure_rec_for(int node);
  void ensure_rec_for(const std::string& node);

  /** add a connection to node, including bytes send and received. */
  void add_connect(int node, const wwiv::core::DateTime& time, uint32_t bytes_sent, uint32_t bytes_received);
  void add_connect(const std::string& node, const wwiv::core::DateTime& time, uint32_t bytes_sent,
                   uint32_t bytes_received);
  /** add a failure attempt to node */
  void add_failure(int node, const wwiv::core::DateTime& time);
  void add_failure(const std::string& node, const wwiv::core::DateTime& time);

  /** Removes the entry for a node */
  void remove(int node);

  bool Save();
  [[nodiscard]] const std::map<std::string, NetworkContact>& contacts() const noexcept { return contacts_; }
  [[nodiscard]] std::map<std::string, NetworkContact>& mutable_contacts() { return contacts_; }
  [[nodiscard]] std::string ToString() const;
  [[nodiscard]] std::string full_pathname() const noexcept;
  [[nodiscard]] std::filesystem::path path() const noexcept;

 private:
   /** add a contact. called by connect or failure. */
   void add_contact(NetworkContact* c, const wwiv::core::DateTime& time);

   const net::Network net_;
   bool save_on_destructor_{false};
   std::map<std::string, NetworkContact> contacts_;
   bool initialized_{false};

   NetworkContact empty_contact{};
};


}  // namespace

#endif
