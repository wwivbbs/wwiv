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

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/net/net.h"
#include "sdk/fido/nodelist.h"

using std::cout;
using std::endl;
using std::string;
using wwiv::core::CommandLineCommand;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {
namespace fido {

static int dump_file(const std::string& filename) {

  auto start = std::chrono::steady_clock::now();
  Nodelist n(filename);
  auto end = std::chrono::steady_clock::now();
  if (!n.initialized()) {
    LOG(ERROR) << "Unable to load nodelist: " << filename;
    return 1;
  }

  const auto& entries = n.entries();

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  cout << "Parsed " << entries.size() << " in " << elapsed.count() << " milliseconds. " << std::endl;
  cout << "Parsed " << entries.size() << " in " << elapsed.count()/1000 << " seconds. " << std::endl;

  for (const auto& e : entries) {
    cout << e.first << ":" << e.second.name_ << std::endl;
  }
  return 0;
}

std::string DumpNodelistCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   nodelist <nodelistfilename>" << endl;
  ss << "Example: nodelist nodelist.123" << endl;
  return ss.str();
}

int DumpNodelistCommand::Execute() {
  if (remaining().empty()) {
    cout << GetUsage() << GetHelp() << endl;
    return 2;
  }
  const string filename(remaining().front());
  return dump_file(filename);
}

bool DumpNodelistCommand::AddSubCommands() {
  return true;
}

}
}
}
