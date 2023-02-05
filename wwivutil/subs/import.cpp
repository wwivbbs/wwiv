/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#include "sdk/fido/backbone.h"
#include "sdk/net/subscribers.h"
#include <iostream>
#include <sstream>

using namespace wwiv::core;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil {



std::string SubsImportCommand::GetUsage() const {
return R"(
Imports subs using the backbone file and values specified in the ini file
specified in wwivconfig for the network or override using --defaults.

Usage:   import --net=NN [--defaults=import.ini] [backbone.na]

Example: wwivutil subs import --net=1

  Sample import.ini:

  [backbone]
  post_acs = user.sl >= 20
  read_acs = user.sl >= 10
  maxmsgs = 5000
  uplink = 21:2/100
  conf = ANF
)";
}

bool SubsImportCommand::AddSubCommands() {
  add_argument({"backbone", 'b', "Override the backbone.na file defined in wwivconfig.", ""});
  add_argument({"defaults", 'd', "Override the ini file defined in wwivconfig.", ""});
  add_argument({"net", "Network number to use (i.e. 0).", ""});

  return true;
}

int SubsImportCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << std::endl;
    return 1;
  }

  auto defaults_fn = sarg("defaults");
  int16_t net_num = 0;
  if (!arg("net").as_string().empty()) {
    // Use the network number on the commandline as an override.
    net_num = iarg<int16_t>("net");
  }
  const auto& net = config()->networks().at(net_num);

  if (net.type != network_type_t::ftn) {
    std::cout << "Can only import subs for FTN network" << std::endl;
    std::cout << GetUsage() << std::endl;
  }

  if (defaults_fn.empty()) {
    // If --defaults is empty, use the one from networks.json
    defaults_fn = net.settings.auto_add_ini;
  }
  if (defaults_fn.empty() || !core::File::Exists(defaults_fn)) {
    std::cout << "Defaults INI file: " << defaults_fn << " does not exist " << std::endl;
    std::cout << GetUsage() << std::endl;
    return 1;
  }

  std::filesystem::path backbone_filename = sarg("backbone");
  if (!backbone_filename.empty()) {
    // Override the backbone filename
    backbone_filename = FilePath(net.dir, net.fido.backbone_filename);
  }
  if (backbone_filename.empty() || !core::File::Exists(backbone_filename)) {
    std::cout << "BACKBONE.NA file: " << backbone_filename << " does not exist " << std::endl;
    std::cout << GetUsage() << std::endl;
    return 1;
  }

  sdk::Subs subs(config()->config()->datadir(), config()->networks().networks(),
                 config()->config()->max_backups());
  if (!subs.Load()) {
    std::cout << "Unable to load subs.json" << std::endl;
    return 1;
  }

  IniFile ini(defaults_fn, "backbone");
  if (!ini.IsOpen()) {
    std::cout << "Unable to open INI file: " << defaults_fn << std::endl;
    return 1;
  }

  auto backbone_echos = ParseBackboneFile(backbone_filename);
  if (backbone_echos.empty()) {
    std::cout << "Nothing to do, no subs parsed out of " << backbone_filename << std::endl;
    return 0;
  }

  //
  // From here down we assume backbone filename, net, etc are all set
  //

  auto r = ImportSubsFromBackbone(subs, net, net_num, ini, backbone_echos);

  if (r.success && r.subs_dirty) {
    if (!subs.Save()) {
      LOG(ERROR) << "Error Saving subs.dat";
    }
  }

  std::cout << "Done!" << std::endl;
  return 0;
}

}
