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
#include "wwivutil/files/allow.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/allow.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {
namespace files {


class AllowListCommand : public UtilCommand {
public:
  AllowListCommand() : UtilCommand("list", "Lists the contents of allow.dat") {}

  virtual ~AllowListCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   list [search spec]" << endl;
    return ss.str();
  }

  int Execute() override final {
    std::string pattern;
    if (!remaining().empty()) {
      pattern = remaining().front();
    }
    Allow allow(*config()->config());
    const auto all = allow.allow_vector();
    for (const auto a : all) {
      std::cout << a.a << "\n";
    }
    return 0;
  }

  bool AddSubCommands() override final { return true; }
};

class AllowAddCommand : public UtilCommand {
public:
  AllowAddCommand() : UtilCommand("add", "Add to the contents of allow.dat") {}
  virtual ~AllowAddCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   add <filename>" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const auto fn = remaining().front();
    Allow allow(*config()->config());
    if (!allow.Add(fn)) {
      LOG(ERROR) << "Failed to add file: " << fn;
      return 1;
    } else {
      std::cout << "Added file: " << fn;
      if (!allow.Save()) {
        LOG(ERROR) << "Failed to save : " << ALLOW_DAT;
      }
    }
    return 0;
  }

  bool AddSubCommands() override final { return true; }
};

class AllowedCommand : public UtilCommand {
public:
  AllowedCommand() : UtilCommand("allowed", "Is the file allowed (i.e. not in the list)") {}
  virtual ~AllowedCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   allowed <filename>" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const auto fn = remaining().front();
    Allow allow(*config()->config());
    bool allowed = allow.IsAllowed(fn);
    if (allowed) {
      std::cout << "filename allowed:     " << fn;
    } else {
      std::cout << "filename NOT allowed: " << fn;
    }
    return 0;
  }

  bool AddSubCommands() override final { return true; }
};

class AllowDeleteCommand : public UtilCommand {
public:
  AllowDeleteCommand() : UtilCommand("delete", "Delete from the contents of allow.dat") {}
  virtual ~AllowDeleteCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   delete <filename>" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const auto fn = remaining().front();
    Allow allow(*config()->config());
    if (!allow.Remove(fn)) {
      LOG(ERROR) << "Failed to delete file: " << fn;
      return 1;
    } else {
      std::cout << "Deleted file: " << fn;
      if (!allow.Save()) {
        LOG(ERROR) << "Failed to save : " << ALLOW_DAT;
      }
    }
    return 0;
  }

  bool AddSubCommands() override final { return true; }
};
std::string AllowCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   allow " << endl;
  return ss.str();
}

bool AllowCommand::AddSubCommands() {
  if (!add(std::make_unique<AllowListCommand>())) {
    return false;
  }
  if (!add(std::make_unique<AllowAddCommand>())) {
    return false;
  }
  if (!add(std::make_unique<AllowedCommand>())) {
    return false;
  }
  if (!add(std::make_unique<AllowDeleteCommand>())) {
    return false;
  }
  return true;
}

} // namespace files
} // namespace wwivutil
} // namespace wwiv
