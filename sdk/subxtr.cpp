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

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

struct subboard_network_data_t {
  string stype;
  int32_t flags;
  int16_t net_num;
  int16_t host;
  int16_t category;

  template <class Archive>
  void serialize(Archive & ar) {
    ar(CEREAL_NVP(stype), CEREAL_NVP(flags), CEREAL_NVP(net_num), CEREAL_NVP(host), CEREAL_NVP(category));
  }

};


struct subboard_t {
  // board name
  string name;
  // long description - for subs.lst
  string desc;

  // board database filename
  string filename;
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

  vector<subboard_network_data_t> nets;

  template <class Archive>
  void serialize(Archive & archive) {
    archive(CEREAL_NVP(name), CEREAL_NVP(desc), CEREAL_NVP(filename), CEREAL_NVP(key), 
      CEREAL_NVP(readsl), CEREAL_NVP(postsl), CEREAL_NVP(anony), CEREAL_NVP(age), CEREAL_NVP(maxmsgs), 
      CEREAL_NVP(ar), CEREAL_NVP(storage_type), CEREAL_NVP(type), CEREAL_NVP(nets));
  }
};

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
  strcpy(x.stype, stype.c_str());
  x.net_num = static_cast<int16_t>(net_num);

  stream >> x.flags;
  stream >> x.host;
  stream >> x.category;

  xsub.nets.push_back(x);
  return true;
}

bool read_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec>& subs, std::vector<xtrasubsrec>& xsubs) {
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

  vector<subboard_t> new_subs;
  subboard_t t1 = {};
  t1.desc = "test1"; t1.name = "name, test1"; t1.filename = "TEST1";
  t1.readsl = 20; t1.postsl = 50;
  subboard_network_data_t tn1 = {};
  tn1.category = 99; tn1.host = 1; tn1.net_num = 1; tn1.stype = "TEST1";
  t1.nets.emplace_back(tn1);
  new_subs.emplace_back(t1);

  std::ostringstream os;
  cereal::JSONOutputArchive archive(os);
  archive(CEREAL_NVP(new_subs));
  std::cerr << os.str();

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
          n.stype,
          n.flags,
          n.host,
          n.category);
      }
    }
    i++;
  }
  return true;
}

vector<subboardrec> read_subs(const string &datadir) {
  DataFile<subboardrec> file(datadir, SUBS_DAT);
  if (!file) {
    // TODO(rushfan): Figure out why this caused link errors. What's missing?
    //LOG(ERROR) << file.file().GetName() << " NOT FOUND.";
    return{};
  }
  std::vector<subboardrec> subboards;
  if (!file.ReadVector(subboards)) {
    return{};
  }
  return subboards;
}

bool write_subs(const string &datadir, const vector<subboardrec>& subboards) {
  DataFile<subboardrec> subsfile(datadir, SUBS_DAT,
    File::modeBinary | File::modeReadWrite | File::modeCreateFile | File::modeTruncate, File::shareDenyReadWrite);
  if (!subsfile) {
    return false;
  }
  return subsfile.WriteVector(subboards);
}

}
}
