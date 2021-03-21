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
#include "sdk/files/allow.h"
#include "wwivutil/files/allow.h"

#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/filenames.h"

#include <iostream>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;

namespace wwiv::wwivutil::files {


class AllowListCommand final : public UtilCommand {
public:
  AllowListCommand() : UtilCommand("list", "Lists the contents of allow.dat") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   list [search spec]" << std::endl;
    return ss.str();
  }

  int Execute() override {
    std::string pattern;
    if (!remaining().empty()) {
      pattern = ToStringUpperCase(remaining().front());
    }
    const Allow allow(*config()->config());
    const auto& all = allow.allow_vector();
    for (const auto& a : all) {
      const auto fn = ToStringUpperCase(a.a);
      if (pattern.empty() || fn.find(pattern) != std::string::npos) {
        std::cout << a.a << "\n";
      }
    }
    return 0;
  }

  bool AddSubCommands() override { return true; }
};

class AllowAddCommand final : public UtilCommand {
public:
  AllowAddCommand() : UtilCommand("add", "Add to the contents of allow.dat") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   add <filename>" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const auto& fn = remaining().front();
    Allow allow(*config()->config());
    if (!allow.Add(fn)) {
      LOG(ERROR) << "Failed to add file: " << fn;
      return 1;
    }
    if (!allow.Save()) {
      LOG(ERROR) << "Failed to add file (save failed) : " << ALLOW_DAT;
      return 1;
    }
    std::cout << "Added file: " << fn;
    return 0;
  }

  bool AddSubCommands() override { return true; }
};

class AllowedCommand final : public UtilCommand {
public:
  AllowedCommand() : UtilCommand("allowed", "Is the file allowed (i.e. not in the list)") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   allowed <filename>" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const auto& fn = remaining().front();
    Allow allow(*config()->config());
    if (const auto allowed = allow.IsAllowed(fn); allowed) {
      std::cout << "filename allowed:     " << fn;
    } else {
      std::cout << "filename NOT allowed: " << fn;
    }
    return 0;
  }

  bool AddSubCommands() override { return true; }
};

class AllowDeleteCommand final : public UtilCommand {
public:
  AllowDeleteCommand() : UtilCommand("delete", "Delete from the contents of allow.dat") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   delete <filename>" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::cerr << "missing filename";
      return 2;
    }
    const auto& fn = remaining().front();
    Allow allow(*config()->config());
    if (!allow.Remove(fn)) {
      LOG(ERROR) << "Failed to delete file: " << fn;
      return 1;
    }
    if (!allow.Save()) {
      LOG(ERROR) << "Failed to save : " << ALLOW_DAT;
      return 1;
    }
    std::cout << "Deleted file: " << fn;
    return 0;
  }

  bool AddSubCommands() override { return true; }
};

std::string AllowCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   allow " << std::endl;
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

} // namespace wwiv
