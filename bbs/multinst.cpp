/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/multinst.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/instmsg.h"
#include "common/output.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/filenames.h"
#include "sdk/files/dirs.h"
#include "sdk/names.h"
#include "sdk/subxtr.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local function prototypes

/*
 * Returns a std::string (in out) like:
 *
 * Instance   1: Offline
 *     LastUser: Sysop #1
 *
 * or
 *
 * Instance  22: Network transmission
 *     CurrUser: Sysop #1
 */
std::string make_inst_str(int instance_num, int format) {
  auto ir = a()->instances().at(instance_num);

  switch (format) {
  case INST_FORMAT_WFC:
    return ir.location_description();
  case INST_FORMAT_OLD:
    // Not used anymore.
    return fmt::sprintf("|#1Instance %-3d: |#2", instance_num);
  case INST_FORMAT_LIST: {
    std::string user_name{"(Nobody)"};
    if (ir.user_number() < a()->config()->max_users() && ir.user_number() > 0) {
      if (ir.online()) {
        user_name = a()->names()->UserName(ir.user_number());
      } else {
        user_name = StrCat("Last: ", a()->names()->UserName(ir.user_number()));
      }
    }
    const auto as = ir.location_description();
    return fmt::sprintf("|#5%-4d |#2%-35.35s |#1%-37.37s", instance_num, user_name, as);
  }
  default:
    return fmt::format("** INVALID INSTANCE FORMAT PASSED [{}] **", format);
  }
}

void multi_instance() {
  bout.nl();
  const auto num = num_instances();
  if (num < 1) {
    bout.outstr("|#6Couldn't find instance data file.\r\n");
    return;
  }

  bout.printf("|#5Node |#1%-35.35s |#2%-37.37s\r\n", "User Name", "Activity");
  bout.print("==== {} {}\r\n", std::string(35, '='), std::string(37, '='));

  for (int inst = 1; inst <= num; inst++) {
    bout.pl(make_inst_str(inst, INST_FORMAT_LIST));
  }
}

int find_instance_by_loc(int loc, int subloc) {
  if (loc == INST_LOC_FSED) {
    return 0;
  }

  const auto instances = a()->instances().all();
  if (instances.size() <= 1) {
    return 0;
  }
  for (const auto& in : instances) {
    if (in.loc_code() == loc && in.subloc_code() == subloc && in.node_number() > 0 &&
        in.node_number() != a()->sess().instance_number()) {
      return in.node_number();
    }
  }
  return 0;
}
