/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#include "wwivutil/subs/subs.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/networks.h"
#include "wwivutil/util.h"
#include "wwivutil/subs/import.h"
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using std::clog;
using std::cout;
using std::endl;
using std::make_unique;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil {


// This is hacked from subacc.cpp.
// TODO(rushfan): move this into the message sdk.
static uint32_t WWIVReadLastRead(const std::string& datadir, const std::string& sub_filename) {
  // open file, and create it if necessary
  postrec p{};

  if (const auto sub_fn = FilePath(datadir, StrCat(sub_filename, ".sub")); !File::Exists(sub_fn)) {
    return 1;
  }
  File subFile(FilePath(datadir, StrCat(sub_filename, ".sub")));
  if (!subFile.Open(File::modeBinary | File::modeReadOnly)) {
    return 0;
  }
  // read in first rec, specifying # posts
  // p.owneruser contains # of posts.
  subFile.Read(&p, sizeof(postrec));

  if (p.owneruser == 0) {
    // Not sure why but iscan1 returned 1 for empty subs.
    return 1;
  }

  // read in sub date, if don't already know it
  subFile.Seek(p.owneruser * sizeof(postrec), File::Whence::begin);
  subFile.Read(&p, sizeof(postrec));
  return p.qscan;
}

class SubsListCommand : public UtilCommand {
public:
  SubsListCommand() : UtilCommand("areas", "Lists the message areas") {}

  virtual ~SubsListCommand() = default;

  [[nodiscard]] std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   areas" << endl;
    return ss.str();
  }

  int Execute() override final {
    Subs subs(config()->config()->datadir(), config()->networks().networks());
    if (!subs.Load()) {
      LOG(ERROR) << "Unable to load subs";
      return 2;
    }

    int num = 0;
    cout << "#Num FileName LastRead   " << std::setw(30) << std::left << "Name"
         << " " << std::endl;
    cout << string(78, '=') << endl;
    for (const auto& d : subs.subs()) {
      const auto lastread = WWIVReadLastRead(config()->config()->datadir(), d.filename);
      cout << "#" << std::setw(3) << std::left << num++ << " " 
           << std::setw(8) << d.filename << " "
           << std::setw(std::numeric_limits<uint32_t>::digits10 + 1) << lastread << " "
           << std::setw(30) << d.name << std::endl;
    }
    return 0;
  }

  bool AddSubCommands() override final {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every area.", false));
    return true;
  }
};

bool SubsCommand::AddSubCommands() {
  if (!add(make_unique<SubsImportCommand>())) {
    return false;
  }
  if (!add(make_unique<SubsListCommand>())) {
    return false;
  }
  
  return true;
}

} // namespace wwiv
