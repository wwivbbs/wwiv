/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#include "wwivutil/files/arc.h"

#include "core/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/files/arc.h"
#include <iostream>
#include <map>
#include <string>

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil::files {


class ArcViewCommand : public UtilCommand {
public:
  ArcViewCommand() : UtilCommand("view", "Views the contents of an archive.") {}

  virtual ~ArcViewCommand() = default;

  [[nodiscard]] std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   list [search spec]" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const std::filesystem::path fn{remaining().front()};

    auto o = wwiv::sdk::files::list_archive(fn);
    if (!o) {
      LOG(INFO) << "Unable to view archive: '" << fn.string() << "'.";
      return 1;
    }
    const auto& files = o.value();
    std::cout << "CompSize     Size   Date   Time  CRC-32   File Name" << std::endl;
    std::cout << "======== -------- ======== ----- ======== ----------------------------------"
              << std::endl;
    for (const auto& f : files) {
      const auto dt = DateTime::from_time_t(f.dt);
      auto d = dt.to_string("%m/%d/%y");
      auto t = dt.to_string("%I:%M");
      auto line = fmt::format("{:>8} {:>8} {:<8} {:<5} {:>08x} {}", 
        f.compress_size, f.uncompress_size, d, t, f.crc32, f.filename);
      std::cout << line << std::endl;
    }

    return 0;
  }

  bool AddSubCommands() override final { return true; }
};

std::string ArcCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   arc " << endl;
  return ss.str();
}

bool ArcCommand::AddSubCommands() {
  if (!add(std::make_unique<ArcViewCommand>())) {
    return false;
  }
  return true;
}

} // namespace wwiv
