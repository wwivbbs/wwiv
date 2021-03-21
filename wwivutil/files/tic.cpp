/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#include "sdk/files/tic.h"
#include "wwivutil/files/tic.h"

#include "core/log.h"
#include "core/strings.h"

#include <iostream>
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil::files {


class TicValidateCommand final : public UtilCommand {
public:
  TicValidateCommand() : UtilCommand("validate", "Validates the contents of a TIC file.") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   list [search spec]" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const std::filesystem::path fn{remaining().front()};
    const auto dir = fn.parent_path();
    const sdk::files::TicParser parser(dir);
    const auto otic = parser.parse(fn.filename().string());
    if (!otic) {
      LOG(ERROR) << "Failed to parse TIC file: " << fn.string();
      return 1;
    }
    if (const auto & tic = otic.value(); !tic.IsValid()) {
      LOG(INFO) << "TIC file:   '" << fn.string() << " is not valid. ";
      LOG(INFO) << "crc_valid:  " << tic.crc_valid();
      LOG(INFO) << "size_valid: " << tic.size_valid();
      LOG(INFO) << "Exists:     " << tic.exists();
      LOG(INFO) << "IsValid:    " << tic.IsValid();
      return 1;
    }
    LOG(INFO) << "TIC file:   '" << fn.string() << " is valid. ";
    return 0;
  }

  bool AddSubCommands() override { return true; }
};

std::string TicCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   tic " << std::endl;
  return ss.str();
}

bool TicCommand::AddSubCommands() {
  if (!add(std::make_unique<TicValidateCommand>())) {
    return false;
  }
  return true;
}

} // namespace wwiv
