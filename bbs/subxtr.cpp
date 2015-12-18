/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using std::string;
using std::vector;
using namespace wwiv::strings;


static xtrasubsnetrec *xsubsn;
static int nn;

static int FindNetworkByName(const std::string& name) {
  for (int i = 0; i < session()->max_net_num(); i++) {
    if (IsEqualsIgnoreCase(net_networks[i].name, name.c_str())) {
      return i;
    }
  }
  return -1;
}

bool ParseXSubsLine(const std::string& line, xtrasubsrec& xsub) {
  std::stringstream stream(line);
  string net_name;
  stream >> net_name;
  StringTrim(&net_name);
  int net_num = FindNetworkByName(net_name);
  if (net_num == -1) {
    return false;
  }

  xtrasubsnetrec x;

  string stype;
  stream >> stype;
  StringTrim(&stype);
  strcpy(x.stype, stype.c_str());
  x.type = atoi(x.stype);
  x.net_num = net_num;

  stream >> x.flags;
  stream >> x.host;
  stream >> x.category;

  xsub.nets.push_back(x);
  return true;
}

bool read_subs_xtr(const std::vector<subboardrec>& subs, std::vector<xtrasubsrec>& xsubs) {
  TextFile subs_xtr(syscfg.datadir, SUBS_XTR, "rt");
  if (!subs_xtr.IsOpen()) {
    return false;
  }
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
      if (curn > subs.size()) {
        // Bad number on ! line.
        break;
      }
      while (xsubs.size() <= curn) {
        xtrasubsrec x;
        memset(&x, 0, sizeof(xtrasubsrec));
        xsubs.push_back(x);
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
        ParseXSubsLine(line, xsubs[curn]);
      }
    }
  }
  return true;
}

