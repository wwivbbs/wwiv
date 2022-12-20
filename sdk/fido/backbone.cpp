/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2022, WWIV Software Services                   */
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
#include "sdk/fido/backbone.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "sdk/net/net.h"
#include "sdk/net/subscribers.h"
#include <set>
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::sdk::fido {

std::vector<backbone_t> ParseBackboneFile(const std::filesystem::path& file) {
  TextFile f(file, "rt");
  if (!f) {
    return {};
  }
  const auto lines = f.ReadFileIntoVector();
  std::vector<backbone_t> out;

  for (const auto& line : lines) {
    if (line.empty() || line.front() == ';') {
      continue;
    }
    const auto idx = line.find_first_of(" \t");
    if (idx == std::string::npos) {
      continue;
    }
    backbone_t b{};
    b.tag = StringTrim(line.substr(0, idx));
    b.desc = StringTrim(line.substr(idx + 1));
    out.emplace_back(b);
  }

  return out;
}

import_result_t ImportSubsFromBackbone(Subs& subs, const Network& net, int16_t net_num, wwiv::core::IniFile& ini,
                                       const std::vector<backbone_t> backbone_echos) {

  sdk::subboard_t prototype{};
  prototype.post_acs = ini.value<std::string>("post_acs", "user.sl >= 20");
  prototype.read_acs = ini.value<std::string>("read_acs", "user.sl >= 10");
  prototype.maxmsgs = ini.value<uint16_t>("maxmsgs", 5000);
  prototype.storage_type = ini.value<uint8_t>("storage_type", 2);

  sdk::subboard_network_data_t netdata{};
  const sdk::fido::FidoAddress uplink(ini.value<std::string>("uplink"));
  netdata.net_num = net_num;

  netdata.host = FTN_FAKE_OUTBOUND_NODE;
  netdata.ftn_uplinks.emplace(sdk::fido::FidoAddress(ini.value<std::string>("uplink")));
  prototype.nets.emplace_back(netdata);

  // Build list of existing echos for this network.
  std::unordered_set<std::string> existing_echos;
  for (const auto& sub : subs.subs()) {
    for (const auto& n : sub.nets) {
      if (n.net_num == net_num) {
        existing_echos.insert(n.stype);
      }
    }
  }

  auto subs_dirty = false;
  for (const auto& b : backbone_echos) {
    if (!stl::contains(existing_echos, b.tag)) {
      // Add it.
      LOG(INFO) << "Echo tag: " << b.tag << " does not already exist.";
      subs_dirty = true;
      auto new_sub{prototype};
      new_sub.nets.front().stype = b.tag;
      new_sub.filename = ToStringLowerCase(b.tag);
      new_sub.name = b.desc;
      subs.add(new_sub);

      // Create nECHOTAG_NAME.net file.
      auto subscriberfile = FilePath(net.dir, fmt::format("n{}.net", b.tag));
      wwiv::sdk::WriteFidoSubcriberFile(subscriberfile, {uplink});
    }
  }

  return {true, subs_dirty};
}

}  // namespace

