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
#include "sdk/subxtr.h"

#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

#include "bbs/vars.h"
#include "core/datafile.h"
#include "core/log.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using cereal::make_nvp;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

bool read_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec_422_t>& subs, std::vector<xtrasubsrec>& xsubs);
bool write_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<xtrasubsrec>& xsubs);

std::vector<subboardrec_422_t> read_subs(const std::string &datadir);
bool write_subs(const std::string &datadir, const std::vector<subboardrec_422_t>& subboards);


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

template <class Archive>
void serialize(Archive & ar, subboard_t& s) {
  ar(make_nvp("name", s.name));
  try {
    ar(make_nvp("desc", s.desc));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("filename", s.filename));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("key", s.key));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("readsl", s.readsl));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("postsl", s.postsl));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("anony", s.anony));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("age", s.age));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("maxmsgs", s.maxmsgs));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("ar", s.ar));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("storage_type", s.storage_type));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(make_nvp("nets", s.nets));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
}

template <class Archive>
void serialize(Archive & ar, subs_t& s) {
  ar(cereal::make_nvp("subs", s.subs));
}

bool Subs::LoadFromJSON(const std::string& dir, const std::string& filename, subs_t& s) {
  s.subs.clear();
  TextFile file(dir, filename, "r");
  if (!file.IsOpen()) {
    return false;
  }
  string text = file.ReadFileIntoString();
  std::stringstream ss;
  ss << text;
  cereal::JSONInputArchive load(ss);
  load(cereal::make_nvp("subs", s.subs));

  return true;
}

//static 
bool Subs::SaveToJSON(const std::string& dir, const std::string& filename, const subs_t& s) {
  std::ostringstream ss;
  {
    cereal::JSONOutputArchive save(ss);
    save(cereal::make_nvp("subs", s.subs));
  }

  TextFile file(dir, filename, "w");
  if (!file.IsOpen()) {
    // rapidjson will assert if the file does not exist, so we need to 
    // verify that the file exists first.
    return false;
  }

  file.Write(ss.str());
  return true;
}

static int FindNetworkByName(const std::vector<net_networks_rec>& net_networks, const std::string& name) {
  for (size_t i = 0; i < net_networks.size(); i++) {
    if (IsEqualsIgnoreCase(net_networks[i].name, name.c_str())) {
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
  int net_num = FindNetworkByName(net_networks, net_name);
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
  if (!File::Exists(datadir, SUBS_XTR)) {
    return true;
  }

  TextFile subs_xtr(datadir, SUBS_XTR, "rt");
  if (!subs_xtr.IsOpen()) {
    return false;
  }

  // Clear the existing xsubs.
  xsubs.clear();
  // add default constructed xtrasubsrec entries
  xsubs.resize(subs.size());

  // Only load the configuration file if it exists.
  string line;
  int curn = -1;
  while (subs_xtr.ReadLine(&line)) {
    StringTrim(&line);
    char identifier = line.front();
    line = line.substr(1);
    switch (identifier) {
    case '!':
    {                        /* sub idx */
      curn = atoi(line.c_str());
      if (curn >= size_int(subs)) {
        // Bad number on ! line.
        curn = -1;
        break;
      }
    } break;
    case '@':                         /* desc */
      if (curn >= 0) {
        strncpy(xsubs[curn].desc, line.c_str(), 60);
      }
      break;
    case '$':                         /* net info */
      if (curn >= 0) {
        ParseXSubsLine(net_networks, line, xsubs[curn]);
      }
    }
  }

  return true;
}

bool write_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const vector<xtrasubsrec>& xsubs) {
  // Backup subs.xtr
  const string subs_xtr_old_name = StrCat(SUBS_XTR, ".old");
  File::Remove(datadir, subs_xtr_old_name);
  File subs_xtr(datadir, SUBS_XTR);
  File subs_xtr_old(datadir, subs_xtr_old_name);
  File::Move(subs_xtr.full_pathname(), subs_xtr_old.full_pathname());

  TextFile f(datadir, SUBS_XTR, "w");
  if (!f.IsOpen()) {
    return false;
  }

  int i = 0;
  for (const auto& x : xsubs) {
    if (!x.nets.empty()) {
      f.WriteFormatted("!%u\n@%s\n#0\n", i, x.desc);
      for (const auto& n : x.nets) {
        f.WriteFormatted("$%s %s %lu %u %u\n",
          net_networks[n.net_num].name,
          n.stype_str.c_str(),
          n.flags,
          n.host,
          n.category);
      }
    }
    i++;
  }
  return true;
}

vector<subboardrec_422_t> read_subs(const string &datadir) {
  DataFile<subboardrec_422_t> file(datadir, SUBS_DAT);
  if (!file) {
    // TODO(rushfan): Figure out why this caused link errors. What's missing?
    //LOG(ERROR) << file.file().GetName() << " NOT FOUND.";
    return{};
  }
  std::vector<subboardrec_422_t> subboards;
  if (!file.ReadVector(subboards)) {
    return{};
  }
  return subboards;
}

bool write_subs(const string &datadir, const vector<subboardrec_422_t>& subboards) {
  DataFile<subboardrec_422_t> subsfile(datadir, SUBS_DAT,
    File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate, File::shareDenyReadWrite);
  if (!subsfile) {
    return false;
  }
  return subsfile.WriteVector(subboards);
}


// Classes

Subs::Subs(const std::string& datadir, 
    const std::vector<net_networks_rec>& net_networks)
  : datadir_(datadir), net_networks_(net_networks) {};

Subs::~Subs() {}

bool Subs::Load() {
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
    sub.storage_type = olds.storage_type;
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

  {
    // Backup subs.json
    const string subs_xtr_old_name = StrCat(SUBS_JSON, ".old");
    File::Remove(datadir_, subs_xtr_old_name);
    File subs_json(datadir_, SUBS_JSON);
    File subs_json_old(datadir_, subs_xtr_old_name);
    File::Move(subs_json.full_pathname(), subs_json_old.full_pathname());

    // Save subs.
    subs_t t;
    t.subs = subs_;
    SaveToJSON(datadir_, SUBS_JSON, t);
  }
  return true;
}

bool Subs::insert(std::size_t n, subboard_t r) {
  return insert_at(subs_, n, r);
}

bool Subs::erase(std::size_t n) {
  return erase_at(subs_, n);
}

const subboard_t& Subs::sub(const std::string& filename) const {
  for (auto& n : subs_) {
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
  for (auto& n : subs_) {
    if (iequals(filename, n.filename)) {
      return true;
    }
  }

  return false;
}

}
}
