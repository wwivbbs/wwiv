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

Contact::Contact(const string& network_dir) : network_dir_(network_dir) {
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

Contact::Contact(std::initializer_list<net_contact_rec> l) {
  for (const auto& r : l) {
    contacts_.emplace_back(r);
  }
}

Contact::~Contact() {
  if (!initialized_) {
    return;
  }
  if (network_dir_.empty()) {
    return;
  }

  DataFile<net_contact_rec> file(network_dir_, CONTACT_NET, File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!file) {
    return;
  }
  file.Write(&contacts_[0], contacts_.size());
}

net_contact_rec* Contact::contact_rec_for(int node) {
  for (net_contact_rec& c : contacts_) {
    if (c.systemnumber == node) {
      return &c;
    }
  }
  return nullptr;
}

static std::string DumpCallout(const net_contact_rec& n) {
  std::ostringstream ss;
  ss << "sysnum:        "  << n.systemnumber << std::endl;
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

