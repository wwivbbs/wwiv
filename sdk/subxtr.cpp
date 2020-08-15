/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "sdk/subxtr.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using cereal::make_nvp;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;


namespace wwiv::sdk {

bool read_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec_422_t>& subs, std::vector<xtrasubsrec>& xsubs);
bool write_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<xtrasubsrec>& xsubs);

std::vector<subboardrec_422_t> read_subs(const std::string &datadir);
bool write_subs(const std::string &datadir, const std::vector<subboardrec_422_t>& subboards);

#define SERIALIZE(n, field)                                                                        \
  {                                                                                                \
    try {                                                                                          \
      ar(cereal::make_nvp(#field, n.field));                                                       \
    } catch (const cereal::Exception&) {                                                           \
      ar.setNextName(nullptr);                                                                     \
    }                                                                                              \
  } // NOLINT(cppcoreguidelines-macro-usage)

template <class Archive>
void serialize(Archive & ar, subboard_network_data_t& s) {
  try {
    ar(make_nvp("stype", s.stype));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
  ar(make_nvp("flags", s.flags));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("net_num", s.net_num));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("host", s.host));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("category", s.category));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
}

template <class Archive> void serialize(Archive& ar, subboard_t& s) { 
  SERIALIZE(s, name);
  SERIALIZE(s, filename);
  SERIALIZE(s, key);
  SERIALIZE(s, readsl);
  SERIALIZE(s, postsl);
  SERIALIZE(s, anony);
  SERIALIZE(s, age);
  SERIALIZE(s, maxmsgs);
  SERIALIZE(s, ar);
  SERIALIZE(s, storage_type);
  SERIALIZE(s, nets);
}

bool Subs::LoadFromJSON(const std::string& dir, const std::string& filename, std::vector<subboard_t>& s) {
  s.clear();
  const auto path = FilePath(dir, filename);
  JsonFile f(path, "subs", s);
  return f.Load();
}


//static 
bool Subs::SaveToJSON(const std::string& dir, const std::string& filename, const std::vector<subboard_t>& s) {
  const auto path = FilePath(dir, filename);
  JsonFile f(path, "subs", s);
  return f.Save();
}

static int FindNetworkByName(const std::vector<net_networks_rec>& net_networks, const std::string& name) {
  for (auto i = 0; i < wwiv::stl::ssize(net_networks); i++) {
    if (iequals(net_networks[i].name, name.c_str())) {
      return i;
    }
  }
  return -1;
}

bool ParseXSubsLine(const std::vector<net_networks_rec>& net_networks, const std::string& line, xtrasubsrec& xsub) {
  std::stringstream stream(line);
  string net_name;
  stream >> net_name;
  StringTrim(&net_name);
  const auto net_num = FindNetworkByName(net_networks, net_name);
  if (net_num == -1) {
    return false;
  }

  xtrasubsnetrec x;

  string stype;
  stream >> stype;
  StringTrim(&stype);
  x.stype_str = stype;
  x.net_num = static_cast<int16_t>(net_num);

  stream >> x.flags;
  stream >> x.host;
  stream >> x.category;

  xsub.nets.push_back(x);
  return true;
}

bool read_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec_422_t>& subs, std::vector<xtrasubsrec>& xsubs) {
  // Clear the existing xsubs.
  xsubs.clear();
  // add default constructed xtrasubsrec entries
  xsubs.resize(subs.size());

  // subs.xtr may not exist on new installs.
  if (!File::Exists(FilePath(datadir, SUBS_XTR))) {
    return true;
  }

  TextFile subs_xtr(FilePath(datadir, SUBS_XTR), "rt");
  if (!subs_xtr.IsOpen()) {
    return false;
  }

  // Clear the existing xsubs.
  xsubs.clear();
  // add default constructed xtrasubsrec entries
  xsubs.resize(subs.size());

  // Only load the configuration file if it exists.
  string line;
  auto curn = -1;
  while (subs_xtr.ReadLine(&line)) {
    StringTrim(&line);
    const auto identifier = line.front();
    line = line.substr(1);
    switch (identifier) {
    case '!':
    {                        /* sub idx */
      curn = to_number<int>(line);
      if (curn >= ssize(subs)) {
        // Bad number on ! line.
        curn = -1;
        break;
      }
    } break;
    case '@':                         /* desc */
      if (curn >= 0) {
        to_char_array(xsubs[curn].desc, line);
      }
      break;
    case '$':                         /* net info */
      if (curn >= 0) {
        ParseXSubsLine(net_networks, line, xsubs[curn]);
      } break;
    default:
      // NOP
      ;
    }
  }

  return true;
}

bool write_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks,
                    const vector<xtrasubsrec>& xsubs) {
  // Backup subs.xtr
  const auto sx = FilePath(datadir, SUBS_XTR);
  backup_file(sx);

  TextFile f(sx, "w");
  if (!f.IsOpen()) {
    return false;
  }

  auto i = 0;
  for (const auto& x : xsubs) {
    if (!x.nets.empty()) {
      f.Write(fmt::sprintf("!%u\n@%s\n#0\n", i, x.desc));
      for (const auto& n : x.nets) {
        if (ssize(net_networks) <= n.net_num || n.net_num < 0) {
          LOG(ERROR) << "Unable to write a subs.xtr line for network number: " << n.net_num
                     << " for sub with description: " << x.desc;
          continue;
        }
        const auto& net = net_networks[n.net_num];
        f.Write(
            fmt::sprintf("$%s %s %lu %u %u\n", net.name, n.stype_str, n.flags, n.host, n.category));
      }
    }
    i++;
  }
  return true;
}

vector<subboardrec_422_t> read_subs(const string &datadir) {
  DataFile<subboardrec_422_t> file(FilePath(datadir, SUBS_DAT));
  if (!file) {
    // TODO(rushfan): Figure out why this caused link errors. What's missing?
    //LOG(ERROR) << file.file() << " NOT FOUND.";
    return{};
  }
  std::vector<subboardrec_422_t> subboards;
  if (!file.ReadVector(subboards)) {
    return{};
  }
  return subboards;
}

bool write_subs(const string &datadir, const vector<subboardrec_422_t>& subboards) {
  DataFile<subboardrec_422_t> subsfile(FilePath(datadir, SUBS_DAT),
                                       File::modeBinary | File::modeReadWrite |
                                           File::modeCreateFile | File::modeTruncate,
                                       File::shareDenyReadWrite);
  if (!subsfile) {
    return false;
  }
  return subsfile.WriteVector(subboards);
}


// Classes

Subs::Subs(std::string datadir, 
           const std::vector<net_networks_rec>& net_networks)
  : datadir_(std::move(datadir)), net_networks_(net_networks) {};

Subs::~Subs() = default;

bool Subs::Load() {
  if (!LoadFromJSON(datadir_, SUBS_JSON, subs_)) {
    return LoadLegacy();
  }
  return true;
}

bool Subs::LoadLegacy() {
  auto old_subs = read_subs(datadir_);
  std::vector<xtrasubsrec> xsubs;
  if (!read_subs_xtr(datadir_, net_networks_, old_subs, xsubs)) {
    return false;
  }

  subs_.clear();
  for (decltype(old_subs)::size_type i = 0; i < old_subs.size(); i++) {
    const auto& olds = old_subs.at(i);
    auto& oldx = xsubs[i];
    subboard_t sub{};
    sub.name = olds.name;
    sub.desc = oldx.desc;
    sub.filename = olds.filename;
    sub.key = olds.key;
    sub.readsl = olds.readsl;
    sub.postsl = olds.postsl;
    sub.anony = olds.anony;
    sub.age = olds.age;
    sub.maxmsgs = olds.maxmsgs;
    sub.ar = olds.ar;
    sub.storage_type = static_cast<uint8_t>(olds.storage_type);
    for (const auto& n : oldx.nets) {
      subboard_network_data_t netdata = {};
      netdata.stype = n.stype_str;
      netdata.flags = n.flags;
      netdata.net_num = n.net_num;
      netdata.host = n.host;
      netdata.category = n.category;
      sub.nets.emplace_back(netdata);
    }
    subs_.emplace_back(std::move(sub));
  }
  return true;
}

bool Subs::Save() {
  std::vector<subboardrec_422_t> subs;
  std::vector<xtrasubsrec> xsubs;

  for (const auto& s : subs_) {
    subboardrec_422_t ls = {};
    xtrasubsrec lx = {};
    to_char_array(ls.name, s.name);
    to_char_array(lx.desc, s.desc);
    to_char_array(ls.filename, s.filename);
    ls.key = s.key;
    ls.readsl = s.readsl;
    ls.postsl = s.postsl;
    ls.anony = s.anony;
    ls.age = s.age;
    ls.maxmsgs = s.maxmsgs;
    ls.ar = s.ar;
    ls.storage_type = s.storage_type;
    ls.unused_legacy_type = 0;
    for (const auto& n : s.nets) {
      xtrasubsnetrec on = {};
      on.stype_str = n.stype;
      on.flags = n.flags;
      on.net_num = n.net_num;
      on.host = n.host;
      on.category = n.category;
      lx.nets.emplace_back(on);
    }
    subs.emplace_back(ls);
    xsubs.emplace_back(lx);
  }

  if (!write_subs(datadir_, subs)) {
    LOG(ERROR) << "Error saving subs";
    return false;
  }

  if (!write_subs_xtr(datadir_, net_networks_, xsubs)) {
    LOG(ERROR) << "Error saving xsubs";
    return false;
  }

  // Backup subs.json
  backup_file(FilePath(datadir_, SUBS_JSON));

  // Save subs.
  return SaveToJSON(datadir_, SUBS_JSON, subs_);
}

bool Subs::insert(int n, subboard_t r) {
  return insert_at(subs_, n, r);
}

bool Subs::erase(int n) {
  return erase_at(subs_, n);
}

const subboard_t& Subs::sub(const std::string& filename) const {
  for (const auto& n : subs_) {
    if (iequals(filename, n.filename)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find sub of filename: ", filename));
}

subboard_t& Subs::sub(const std::string& filename) {
  for (auto& n : subs_) {
    if (iequals(filename, n.filename)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find sub of filename: ", filename));
}

bool Subs::exists(const std::string& filename) const {
  for (const auto& n : subs_) {
    if (iequals(filename, n.filename)) {
      return true;
    }
  }

  return false;
}

}
