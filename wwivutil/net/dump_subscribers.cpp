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
#include "wwivutil/net/dump_subscribers.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/net/subscribers.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil::net {

static int dump_file(const std::string& filename) {

  if (!File::Exists(filename)) {
    LOG(ERROR) << "subscriber file: '" << filename << "' does not exist.";
    return 1;
  }

  const auto start = std::chrono::steady_clock::now();
  std::set<uint16_t> subscribers;
  if (!ReadSubcriberFile(filename, subscribers)) {
    LOG(ERROR) << "Error reading subscriber file: " << filename;
    return 1;
  }
  const auto end = std::chrono::steady_clock::now();
  if (subscribers.empty()) {
    LOG(INFO) << "No Subscribers: " << filename;
    return 0;
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "Read " << subscribers.size() << " in " << elapsed.count() << " milliseconds. "
       << std::endl;
  std::cout << "Read " << subscribers.size() << " in " << elapsed.count() / 1000 << " seconds. "
       << std::endl;

  for (const auto& e : subscribers) {
    std::cout << "@" << e << std::endl;
  }
  return 0;
}

std::string DumpSubscribersCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   subscribers <subscriber filename>" << std::endl;
  ss << "Example: subscribers net/ftn/nGENCHAT.net" << std::endl;
  return ss.str();
}

int DumpSubscribersCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << GetHelp() << std::endl;
    return 2;
  }
  const auto filename = remaining().front();
  return dump_file(filename);
}

bool DumpSubscribersCommand::AddSubCommands() {
  return true;
}

}
