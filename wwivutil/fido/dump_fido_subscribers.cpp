/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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
#include "wwivutil/fido/dump_fido_subscribers.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/net/net.h"
#include "sdk/net/subscribers.h"

using std::cout;
using std::endl;
using std::string;
using wwiv::core::CommandLineCommand;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {
namespace fido {

static int dump_file(const std::filesystem::path& filename) {

  if (!File::Exists(filename)) {
    LOG(ERROR) << "subscriber file: '" << filename << "' does not exist.";
    return 1;
  }

  auto start = std::chrono::steady_clock::now();
  auto subscribers = ReadFidoSubcriberFile(filename);
  auto end = std::chrono::steady_clock::now();
  if (subscribers.empty()) {
    LOG(INFO) << "No Subscribers: " << filename;
    return 0;
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  cout << "Read " << subscribers.size() << " in " << elapsed.count() << " milliseconds. "
       << std::endl;
  cout << "Read " << subscribers.size() << " in " << elapsed.count() / 1000 << " seconds. "
       << std::endl;

  for (const auto& e : subscribers) {
    cout << e.as_string() << std::endl;
  }
  return 0;
}

std::string DumpFidoSubscribersCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   subscribers <subscriber filename>" << endl;
  ss << "Example: subscribers net/ftn/nGENCHAT.net" << endl;
  return ss.str();
}

int DumpFidoSubscribersCommand::Execute() {
  if (remaining().empty()) {
    cout << GetUsage() << GetHelp() << endl;
    return 2;
  }
  const std::filesystem::path p{remaining().front()};
  return dump_file(p);
}

bool DumpFidoSubscribersCommand::AddSubCommands() {
  return true;
}

}
}
}
