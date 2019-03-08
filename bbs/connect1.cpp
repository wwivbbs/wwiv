/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include "core/file.h"
#include "core/datafile.h"
#include "core/log.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/bbslist.h"
#include "sdk/filenames.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

net_system_list_rec *next_system(int ts) {
  static net_system_list_rec csne;

  auto b = BbsListNet::ReadBbsDataNet(a()->current_net().dir);
  auto c = b.node_config_for(ts);
  if (c == nullptr) {
    csne = {};
    return nullptr;
  }

  csne = *c;
  return &csne;
}

bool valid_system(int ts) {
  return next_system(ts) != nullptr;
}

void set_net_num(int network_number) {
  if (network_number >= 0 && network_number < wwiv::stl::size_int(a()->net_networks)) {
    a()->set_net_num(network_number);
  }
}

int32_t next_system_reg(int16_t ts) {
  auto b = BbsListNet::ReadBbsDataNet(a()->current_net().dir);
  const auto& r = b.reg_number();
  if (r.find(ts) != r.end()) {
    return r.at(ts);
  }
  return 0;
}
