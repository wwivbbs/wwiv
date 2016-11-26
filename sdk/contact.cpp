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
#include "sdk/contact.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/datafile.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"

using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv {
namespace sdk {

namespace {
// Time
constexpr long MINUTES_PER_HOUR = 60L;
constexpr long SECONDS_PER_MINUTE = 60L;
constexpr long SECONDS_PER_HOUR = MINUTES_PER_HOUR * SECONDS_PER_MINUTE;
constexpr long HOURS_PER_DAY = 24L;
constexpr long SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
}

Contact::Contact(const string& network_dir, bool save_on_destructor) 
    : network_dir_(network_dir), save_on_destructor_(save_on_destructor) {
  DataFile<net_contact_rec> file(network_dir, CONTACT_NET, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return;
  }
  
  std::vector<net_contact_rec> vs;
  if (file.number_of_records() > 0) {
    initialized_ = file.ReadVector(vs);
  }

  for (const auto& v : vs) {
    network_contact_record r{};
    r.address = StringPrintf("20000:20000/%u@wwivnet", v.systemnumber);
    r.ncr = v;
    contacts_.emplace_back(r);
  }

  if (!initialized_) {
    LOG(ERROR) << "failed to read the expected number of bytes: "
      << contacts_.size() * sizeof(NetworkContact);
  }
  initialized_ = true;
}

Contact::Contact(std::initializer_list<NetworkContact> l)
    : save_on_destructor_(false), initialized_(true) {
  for (const auto& r : l) {
    contacts_.emplace_back(r);
  }
}

bool Contact::Save() {
  if (!initialized_) {
    return false;
  }
  if (network_dir_.empty()) {
    return false;
  }

  DataFile<NetworkContact> file(network_dir_, CONTACT_NET, 
      File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate,
      File::shareDenyReadWrite);
  if (!file) {
    return false;
  }
  if (contacts_.size() == 0) {
    return false;
  }
  return file.WriteVector(contacts_);
}

Contact::~Contact() {
  if (save_on_destructor_) {
    Save();
  }
}

static void fixup_long(uint32_t &f) {
  auto now = static_cast<uint32_t>(time(nullptr));
  if (f > now) {
    f = now;
  }
  if (f + (SECONDS_PER_DAY * 30L) < now) {
    f = now - (SECONDS_PER_DAY * 30L);
  }
}

void NetworkContact::fixup() {
  fixup_long(ncr_.ncr.lastcontactsent);
  fixup_long(ncr_.ncr.lasttry);
}

void NetworkContact::AddContact(time_t t) {
  daten_t d = static_cast<daten_t>(t);
  ncr_.ncr.lasttry = d;
  ncr_.ncr.lastcontact = d;
  ncr_.ncr.numcontacts++;
}

void NetworkContact::AddConnect(time_t t, uint32_t bytes_sent, uint32_t bytes_received) {
  AddContact(t);

  daten_t d = static_cast<daten_t>(t);
  if (bytes_sent > 0) {
    ncr_.ncr.lastcontactsent = d;
    ncr_.ncr.bytes_waiting = 0;
    ncr_.ncr.bytes_sent += bytes_sent;
  }
  ncr_.ncr.bytes_received += bytes_received;
}

void NetworkContact::AddFailure(time_t t) {
  ncr_.ncr.numfails++;
}

NetworkContact* Contact::contact_rec_for(uint16_t node) {
  for (NetworkContact& c : contacts_) {
    if (c.address() == std::to_string(node)) {
      c.fixup();
      return &c;
    }
  }
  return nullptr;
}

NetworkContact* Contact::contact_rec_for(const std::string& node) {
  for (NetworkContact& c : contacts_) {
    if (c.address() == node) {
      c.fixup();
      return &c;
    }
  }
  return nullptr;
}

void Contact::ensure_rec_for(uint16_t node) {
  const auto* current = contact_rec_for(node);
  if (current == nullptr) {
    NetworkContact r(node);
    contacts_.emplace_back(r);
  }
}

void Contact::ensure_rec_for(const std::string& node) {
  const auto* current = contact_rec_for(node);
  if (current == nullptr) {
    NetworkContact r(node);
    contacts_.emplace_back(r);
  }
}

void Contact::add_connect(int node, time_t time, uint32_t bytes_sent, uint32_t bytes_received) {
  NetworkContact* c = contact_rec_for(node);
  if (c == nullptr) {
    ensure_rec_for(node);
    c = contact_rec_for(node);
    if (c == nullptr) {
      // Unable to add node in add_connect.
      return;
    }
  }
  c->AddConnect(time, bytes_sent, bytes_received);
}

void Contact::add_connect(const std::string& node, time_t time, uint32_t bytes_sent, uint32_t bytes_received) {
  NetworkContact* c = contact_rec_for(node);
  if (c == nullptr) {
    ensure_rec_for(node);
    c = contact_rec_for(node);
    if (c == nullptr) {
      // Unable to add node in add_connect.
      return;
    }
  }
  c->AddConnect(time, bytes_sent, bytes_received);
}

void Contact::add_failure(int node, time_t time) {
  NetworkContact* c = contact_rec_for(node);
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

void Contact::add_failure(const std::string& node, time_t time) {
  NetworkContact* c = contact_rec_for(node);
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

void Contact::add_contact(NetworkContact* c, time_t time) {
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
    ss << DumpCallout(kv) << std::endl;
  }
  return ss.str();
}

}  // namespace net
}  // namespace wwiv

