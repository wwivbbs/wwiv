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
#include "sdk/net/contact.h"

#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include "sdk/fido/fido_address.h"
#include "sdk/net/networks.h"
#include <map>
#include <sstream>
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;

namespace wwiv::sdk {

namespace {
// Time
constexpr long MINUTES_PER_HOUR = 60L;
constexpr long SECONDS_PER_MINUTE = 60L;
constexpr long SECONDS_PER_HOUR = MINUTES_PER_HOUR * SECONDS_PER_MINUTE;
constexpr long HOURS_PER_DAY = 24L;
constexpr long SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
} // namespace

// static
std::string NetworkContact::CreateFakeFtnAddress(int node) {
  return wwiv::strings::StrCat("20000:20000/", node);
}

Contact::Contact(const Network& net) : Contact(net, false) {}

Contact::Contact(Network net, bool save_on_destructor)
    : net_(std::move(net)), save_on_destructor_(save_on_destructor) {
  DataFile<net_contact_rec> file(FilePath(net_.dir, CONTACT_NET),
                                 File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return;
  }

  std::vector<net_contact_rec> vs;
  if (file.number_of_records() > 0) {
    initialized_ = file.ReadVector(vs);
  }

  for (const auto& v : vs) {
    if (v.systemnumber == 0) {
      VLOG(2) << "Skipping contact.net entry for system #0 from file: " << file.file();
      continue;
    }
    network_contact_record r{};
    r.address = NetworkContact::CreateFakeFtnAddress(v.systemnumber);
    r.ncr = v;
    contacts_.emplace(r.address, NetworkContact(r));
  }

  if (!initialized_ && !contacts_.empty()) {
    // No need to log on noo contacts, just if we had partial read.
    LOG(ERROR) << "failed to read the expected number of bytes: "
               << contacts_.size() * sizeof(NetworkContact);
    // TODO(rushfan): Should we move the initialized_ = true into here or
    // just remove it?
  }
  initialized_ = true;
}

Contact::Contact(Network net, std::initializer_list<NetworkContact> l)
    : net_(std::move(net)), save_on_destructor_(false), initialized_(true) {
  for (const auto& r : l) {
    contacts_.emplace(r.address(), r);
  }
}

bool Contact::Save() {
  if (net_.dir.empty()) {
    return false;
  }

  VLOG(3) << "Saving contact.net to: " << FilePath(net_.dir, CONTACT_NET).string();
  DataFile<net_contact_rec> file(FilePath(net_.dir, CONTACT_NET),
                 File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate,
                 File::shareDenyReadWrite);
  if (!file) {
    return false;
  }
  if (contacts_.empty()) {
    return false;
  }

  std::vector<net_contact_rec> vs;
  for (const auto& v : contacts_) {
    vs.emplace_back(v.second.ncr());
  }

  return file.WriteVector(vs);
}

Contact::~Contact() {
  if (save_on_destructor_) {
    Save();
  }
}

static void fixup_long(daten_t& f) {
  if (f == 0) {
    // Don't fixup empty dates.
    return;
  }
  const auto now = daten_t_now();
  if (f > now) {
    f = now;
  }
  // We used to keep it no more than 1 month old, however that
  // makes no sense, just clear it if we've aged out past a month.
  // which is ~720 hours
  if (f + (SECONDS_PER_DAY * 30L) < now) {
    f = 0; // now - (SECONDS_PER_DAY * 30L);
  }
}

void NetworkContact::fixup() {
  fixup_long(ncr_.ncr.lastcontactsent);
  fixup_long(ncr_.ncr.lasttry);
}

void NetworkContact::AddContact(const wwiv::core::DateTime& t) {
  const auto d = t.to_daten_t();
  ncr_.ncr.lasttry = d;
  ncr_.ncr.lastcontact = d;
  ncr_.ncr.numcontacts++;

  if (ncr_.ncr.firstcontact == 0) {
    ncr_.ncr.firstcontact = d;
  }
}

void NetworkContact::AddConnect(const wwiv::core::DateTime& t, uint32_t bytes_sent,
                                uint32_t bytes_received) {
  AddContact(t);

  if (bytes_sent > 0) {
    ncr_.ncr.lastcontactsent = t.to_daten_t();
    ncr_.ncr.bytes_waiting = 0;
    ncr_.ncr.bytes_sent += bytes_sent;
  }
  ncr_.ncr.bytes_received += bytes_received;
}

void NetworkContact::AddFailure(const wwiv::core::DateTime& t) {
  AddContact(t);
  ncr_.ncr.numfails++;
}

NetworkContact* Contact::contact_rec_for(int node) {
  const auto key = NetworkContact::CreateFakeFtnAddress(node);
  return contact_rec_for(key);
}

NetworkContact* Contact::contact_rec_for(const std::string& node) {
  auto it = contacts_.find(node);
  if (it == std::end(contacts_)) {
    return nullptr;
  }
  it->second.fixup();
  return &it->second;
}

void Contact::ensure_rec_for(int node) {
  const auto key = NetworkContact::CreateFakeFtnAddress(node);
  return ensure_rec_for(key);
}

void Contact::ensure_rec_for(const std::string& node) {
  const auto* current = contact_rec_for(node);
  if (current == nullptr) {
    network_contact_record ncr{};
    ncr.address = node;
    const fido::FidoAddress address(node);
    ncr.ncr.systemnumber = address.node();
    contacts_.emplace(node, NetworkContact(ncr));
  }
}

void Contact::add_connect(int node, const wwiv::core::DateTime& time, uint32_t bytes_sent,
                          uint32_t bytes_received) {
  const auto key = NetworkContact::CreateFakeFtnAddress(node);
  add_connect(key, time, bytes_sent, bytes_received);
}

void Contact::add_connect(const std::string& node, const wwiv::core::DateTime& time,
                          uint32_t bytes_sent,
                          uint32_t bytes_received) {
  const auto key = node;
  auto* c = contact_rec_for(key);
  if (c == nullptr) {
    ensure_rec_for(key);
    c = contact_rec_for(key);
    if (c == nullptr) {
      // Unable to add node in add_connect.
      return;
    }
  }
  c->AddConnect(time, bytes_sent, bytes_received);
}

void Contact::add_failure(int node, const wwiv::core::DateTime& time) {
  auto* c = contact_rec_for(node);
  if (c == nullptr) {
    ensure_rec_for(node);
    c = contact_rec_for(node);
    if (c == nullptr) {
      // Unable to add node in add_connect.
      return;
    }
  }
  c->AddFailure(time);
}

void Contact::add_failure(const std::string& n, const wwiv::core::DateTime& time) {
  auto* c = contact_rec_for(n);
  if (c == nullptr) {
    ensure_rec_for(n);
    c = contact_rec_for(n);
    if (c == nullptr) {
      // Unable to add node in add_connect.
      return;
    }
  }
  c->AddFailure(time);
}


void Contact::remove(int node) {
  const auto sn = NetworkContact::CreateFakeFtnAddress(node);
  contacts_.erase(sn);
}


void Contact::add_contact(NetworkContact* c, const wwiv::core::DateTime& time) {
  c->AddContact(time);
}

static std::string DumpCallout(const NetworkContact& n) {
  std::ostringstream ss;
  ss << "sysnum:         " << n.systemnumber() << std::endl;
  ss << "numcontacts:    " << n.numcontacts() << std::endl;
  ss << "numfails:       " << n.numfails() << std::endl;
  ss << "firstcontact:   " << daten_to_wwivnet_time(n.firstcontact()) << std::endl;
  ss << "lastcontact:    " << daten_to_wwivnet_time(n.lastcontact()) << std::endl;
  ss << "lastcontatsent: " << daten_to_wwivnet_time(n.lastcontactsent()) << std::endl;
  ss << "lasttry:        " << daten_to_wwivnet_time(n.lasttry()) << std::endl;
  ss << "bytes_received: " << n.bytes_received() << std::endl;
  ss << "bytes_sent:     " << n.bytes_sent() << std::endl;
  ss << "bytes_waiting:  " << n.bytes_waiting() << std::endl;

  return ss.str();
}

std::string Contact::ToString() const {
  std::ostringstream ss;
  for (const auto& kv : contacts_) {
    ss << DumpCallout(kv.second) << std::endl;
  }
  return ss.str();
}

std::filesystem::path Contact::path() const noexcept { return FilePath(net_.dir, CONTACT_NET); }

std::string Contact::full_pathname() const noexcept { return path().string(); }

network_contact_record to_network_contact_record(const net_contact_rec& n) {
  network_contact_record ncr{};
  ncr.address = NetworkContact::CreateFakeFtnAddress(n.systemnumber);
  ncr.ncr = n;
  return ncr;
}

} // namespace wwiv::sdk
