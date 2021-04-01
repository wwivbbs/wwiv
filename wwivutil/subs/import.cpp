/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "wwivutil/subs/import.h"

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/subxtr.h"
#include <iostream>
#include <sstream>

using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

struct backbone_t {
  std::string tag;
  std::string desc;
};

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
    b.desc = StringTrim(line.substr(idx +1));
    out.emplace_back(b);
  }

  return out;
}

std::string SubsImportCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   import  --format=[backbone] --defaults=<filename.ini> <subs filename>"
     << std::endl;
  return ss.str();
}

bool SubsImportCommand::AddSubCommands() {
  add_argument({"format", 'f', "must be 'backbone'", "backbone"});
  add_argument({"defaults", 'd', "Specify an ini file with defaults to use when creating new subs.", ""});
  return true;
}

int SubsImportCommand::Execute() {
  const auto format = sarg("format");
  if (format != "backbone") {
    std::cout << "Format must be backbone. " << std::endl;
    return 1;
  }

  if (remaining().empty()) {
    std::cout << GetUsage() << std::endl;
    return 1;
  }

  const auto defaults_fn = sarg("defaults");
  if (defaults_fn.empty() || !core::File::Exists(defaults_fn)) {
    std::cout << "Defaults INI file: " << defaults_fn << " does not exist " << std::endl;
    std::cout << GetUsage() << std::endl;
    return 1;
  }

  const auto& backbone_filename = remaining().front();

  IniFile ini(defaults_fn, {format});
  if (!ini.IsOpen()) {
    std::cout << "Unable to open INI file: " << defaults_fn << std::endl;
    return 1;
  }

  sdk::subboard_t prototype{};
  prototype.post_acs = ini.value<std::string>("post_acs", "user.sl >= 20");
  prototype.read_acs = ini.value<std::string>("read_acs", "user.sl >= 10");
  prototype.maxmsgs = ini.value<uint16_t>("maxmsgs", 5000);
  prototype.storage_type = ini.value<uint8_t>("storage_type", 2);

  sdk::subboard_network_data_t net{};
  auto net_num = ini.value<int16_t>("net_num", 0);
  net.net_num = net_num;
  net.host = FTN_FAKE_OUTBOUND_NODE;
  net.ftn_uplinks.emplace(sdk::fido::FidoAddress(ini.value<std::string>("uplink")));
  prototype.nets.emplace_back(net);

  auto backbone_echos = ParseBackboneFile(backbone_filename);
  if (backbone_echos.empty()) {
    std::cout << "Nothing to do, no subs parsed out of " << backbone_filename << std::endl;
    return 0;
  }

  sdk::Subs subs(config()->config()->datadir(),
                 config()->networks().networks(),
                 config()->config()->max_backups());
  if (!subs.Load()) {
    std::cout << "Unable to load subs.json" << std::endl;
    return 1;
  }

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
    }
  }

  if (subs_dirty) {
    if (!subs.Save()) {
      LOG(ERROR) << "Error Saving subs.dat";
    }
  }

  std::cout << "Done!" << std::endl;
  return 0;
}

}
