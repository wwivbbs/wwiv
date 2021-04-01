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
#include "wwivutil/fido/dump_nodelist.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/fido/nodelist.h"
#include "sdk/net/net.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using wwiv::core::CommandLineCommand;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv::wwivutil::fido {

static int dump_file(const std::string& filename) {

  const auto start = std::chrono::steady_clock::now();
  const Nodelist n(filename, "");
  const auto end = std::chrono::steady_clock::now();
  if (!n.initialized()) {
    LOG(ERROR) << "Unable to load nodelist: " << filename;
    return 1;
  }

  const auto& entries = n.entries();

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "Parsed " << entries.size() << " in " << elapsed.count() << " milliseconds. " << std::endl;
  std::cout << "Parsed " << entries.size() << " in " << elapsed.count()/1000 << " seconds. " << std::endl;
  std::cout << std::endl;

  for (const auto& e : entries) {
    std::cout << e.first << ": " << e.second.name() << std::endl;
  }
  return 0;
}

std::string DumpNodelistCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   nodelist <nodelistfilename>" << std::endl;
  ss << "Example: nodelist nodelist.123" << std::endl;
  return ss.str();
}

int DumpNodelistCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << GetHelp() << std::endl;
    return 2;
  }
  const auto filename(remaining().front());
  return dump_file(filename);
}

bool DumpNodelistCommand::AddSubCommands() {
  return true;
}

}
