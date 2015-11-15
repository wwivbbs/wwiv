/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*                Copyright (C)2015 WWIV Software Services                */
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
#include "networkb/contact.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/datafile.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
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
namespace net {

Contact::Contact(const string& network_dir, bool save_on_destructor) 
    : network_dir_(network_dir), save_on_destructor_(save_on_destructor) {
  DataFile<net_contact_rec> file(network_dir, CONTACT_NET, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return;
  }
  
  const int num_records = file.number_of_records();
  contacts_.resize(num_records);
  initialized_ = file.Read(&contacts_[0], num_records);

  if (!initialized_) {
    std::clog << "failed to read the expected number of bytes: "
        << num_records * sizeof(net_contact_rec) << std::endl;
  }
  initialized_ = true;
}

Contact::Contact(std::initializer_list<net_contact_rec> l) 
    : save_on_destructor_(false) {
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

  DataFile<net_contact_rec> file(network_dir_, CONTACT_NET, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return false;
  }
  return file.Write(&contacts_[0], contacts_.size());
}

Contact::~Contact() {
  if (save_on_destructor_) {
    Save();
  }
}

net_contact_rec* Contact::contact_rec_for(int node) {
  for (net_contact_rec& c : contacts_) {
    if (c.systemnumber == node) {
      return &c;
    }
  }
  return nullptr;
}

void Contact::add_connect(int node, time_t time, uint32_t bytes_sent, uint32_t bytes_received) {
  net_contact_rec* c = contact_rec_for(node);
  add_contact(c, time);

  uint32_t time32 = static_cast<uint32_t>(time);
  if (bytes_sent > 0) {
    c->lastcontactsent = time32;
    c->bytes_waiting = 0;
    c->bytes_sent += bytes_sent;
  }
  c->bytes_received += bytes_received;
}

void Contact::add_failure(int node, time_t time) {
  net_contact_rec* c = contact_rec_for(node);
  add_contact(c, time);
  c->numfails++;
}

void Contact::add_contact(net_contact_rec* c, time_t time) {
  uint32_t time32 = static_cast<uint32_t>(time);

  c->lasttry = time32;
  c->lastcontact = time32;
  c->numcontacts++;

  if (c->firstcontact == 0) {
    c->firstcontact = time32;
  }
}

string daten_to_humantime(uint32_t daten) {
  time_t t = static_cast<time_t>(daten);
  string human_date = string(asctime(localtime(&t)));
  wwiv::strings::StringTrimEnd(&human_date);
  return human_date;
}

static std::string DumpCallout(const net_contact_rec& n) {
  std::ostringstream ss;
  ss << "sysnum:         " << n.systemnumber << std::endl;
  ss << "numcontacts:    " << n.numcontacts << std::endl;
  ss << "numfails:       " << n.numfails << std::endl;
  ss << "firstcontact:   " << daten_to_humantime(n.firstcontact) << std::endl;
  ss << "lastcontact:    " << daten_to_humantime(n.lastcontact) << std::endl;
  ss << "lastcontatsent: " << daten_to_humantime(n.lastcontactsent) << std::endl;
  ss << "lasttry:        " << daten_to_humantime(n.lasttry) << std::endl;
  ss << "bytes_received: " << n.bytes_received << std::endl;
  ss << "bytes_sent:     " << n.bytes_sent << std::endl;
  ss << "bytes_waiting:  " << n.bytes_waiting << std::endl;

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

