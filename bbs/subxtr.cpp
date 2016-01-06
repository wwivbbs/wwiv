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
#include "bbs/subxtr.h"

#include <string>
#include <vector>
#include <sstream>

#include "bbs/bbs.h"
#include "bbs/vars.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using std::string;
using std::vector;
using namespace wwiv::stl;
using namespace wwiv::strings;

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
  x.type = StringToUnsignedShort(x.stype);
  x.net_num = static_cast<int16_t>(net_num);

  stream >> x.flags;
  stream >> x.host;
  stream >> x.category;

  xsub.nets.push_back(x);
  return true;
}

bool read_subs_xtr(const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec>& subs, std::vector<xtrasubsrec>& xsubs) {
  TextFile subs_xtr(session()->config()->datadir(), SUBS_XTR, "rt");
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
    case '!': {                        /* sub idx */
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
    case '#':                         /* flags */
      if (curn >= 0) {
        xsubs[curn].flags = atol(line.c_str());
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

bool write_subs_xtr(const std::vector<net_networks_rec>& net_networks, const vector<xtrasubsrec>& xsubs) {
  // Backup subs.xtr
  const string subs_xtr_old_name = StrCat(SUBS_XTR, ".old");
  File::Remove(session()->config()->datadir(), subs_xtr_old_name);
  File subs_xtr(session()->config()->datadir(), SUBS_XTR);
  File subs_xtr_old(session()->config()->datadir(), subs_xtr_old_name);
  File::Move(subs_xtr.full_pathname(), subs_xtr_old.full_pathname());

  TextFile fileSubsXtr(session()->config()->datadir(), SUBS_XTR, "w");
  if (fileSubsXtr.IsOpen()) {
    int i = 0;
    for (const auto& x : xsubs) {
      if (!x.nets.empty()) {
        fileSubsXtr.WriteFormatted("!%u\n@%s\n#%lu\n", i, x.desc, x.flags);
        for (const auto& n : x.nets) {
          fileSubsXtr.WriteFormatted("$%s %s %lu %u %u\n",
            net_networks[n.net_num].name,
            n.stype,
            n.flags,
            n.host,
            n.category);
        }
      }
      i++;
    }
    fileSubsXtr.Close();
  }
  return true;
}
