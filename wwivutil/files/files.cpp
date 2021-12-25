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
#include "sdk/files/files.h"
#include "wwivutil/files/files.h"

#include "core/command_line.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "wwivutil/files/allow.h"
#include "wwivutil/files/arc.h"
#include "wwivutil/files/tic.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil::files {

static std::optional<std::vector<sdk::files::directory_t>> ReadAreas(const std::string& datadir) {
  sdk::files::Dirs dirs(datadir, 0);
  if (dirs.Load()) {
    return {dirs.dirs()};
  }
  return std::nullopt;
}

class AreasCommand final : public UtilCommand {
public:
  AreasCommand()
    : UtilCommand("areas", "Lists the file areas") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   areas" << std::endl;
    return ss.str();
  }

  int Execute() override {
    auto dirs = ReadAreas(config()->config()->datadir());
    if (!dirs) {
      return 2;
    }

    int num = 0;
    std::cout << "#Num FileName " << std::setw(30) << std::left << "Name" << " " 
         << "Path" << std::endl;
    std::cout << std::string(78, '=') << std::endl;
    for (const auto& d : dirs.value()) {
      std::cout << "#" << std::setw(3) << std::left << num++ << " "
           << std::setw(8) << d.filename << " " 
           << std::setw(30) << d.name << " " << d.path
           << std::endl;
    }
    return 0;
  }

  bool AddSubCommands() override {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every area.", false));
    return true;
  }
};

class ListCommand final : public UtilCommand {
public:
  ListCommand()
    : UtilCommand("list", "Lists the files in an area") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   list [num]" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::clog << "Missing file areas #." << std::endl;
      std::cout << GetUsage() << GetHelp() << std::endl;
      return 2;
    }
    auto o = ReadAreas(config()->config()->datadir());
    if (!o) {
      return 2;
    }
    const auto& area_string = remaining().front();
    if (area_string.empty()) {
      LOG(ERROR) << "No file area specified";
      return 1;
    }
    if (!isdigit(area_string.front())) {
      LOG(ERROR) << "No file area number specified.  Input given: " << area_string;
      return 1;
    }
    const auto area_num = to_number<int>(area_string);
    const auto& dirs = o.value();

    if (area_num < 0 || area_num >= size_int(dirs)) {
      LOG(ERROR) << "invalid area number '" << area_num << "' specified. ";
      const auto max_size = std::max<int>(0, dirs.size() - 1);
      LOG(ERROR) << "area_num must be between 0 and " << max_size;
      return 1;
    }
    sdk::files::FileApi api(config()->config()->datadir());
    const auto& dir = at(dirs, area_num);
    auto area = api.Open(dir);
    if (!area) {
      LOG(ERROR) << "Unable to open area: #" << area_num << "; filename: " << dir.filename;
      return 1;
    }
    auto num_files = area->number_of_files();
    std::cout << fmt::format("File Area: {} ({} files)", dir.name, num_files) << std::endl;
    std::cout << std::endl;
    const auto& h = area->header();
    if (static_cast<int>(h.num_files()) != num_files) {
      std::cout << fmt::format("WARNING: Header doesn't match header:{} vs. size:{}", h.num_files(),
                          num_files)
           << std::endl;
    }
    std::cout << "#Num File Name   " << std::left << "Description" << std::endl;
    std::cout << std::string(78, '=') << std::endl;
    for (auto num = 1; num <= num_files; num++) {
      auto f = area->ReadFile(num);
      std::cout << fmt::format("#{: <3} {: <12} {}", num, f.unaligned_filename(), f.description()) << std::endl;
      if (f.has_extended_description() && barg("ext")) {
        auto so = area->ReadExtendedDescriptionAsString(f);
        if (!so) {
          continue;
        }
        auto lines = SplitString(so.value(), "\n", false);
        for(const auto& l : lines) {
          std::cout << std::string(18, ' ') << StringTrim(l) << std::endl;
        }
      }
    }
    return 0;
  }

  bool AddSubCommands() override {
    add_argument(BooleanCommandLineArgument("ext", "Display extended descriptions.", false));
    return true;
  }
};

class DeleteFileCommand final : public UtilCommand {
public:
  DeleteFileCommand() : UtilCommand("delete", "Deleate a file in an area") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   delete --num=NN <sub #>" << std::endl;
    ss << "Example: delete --num=10 1" << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      std::clog << "Missing file area #." << std::endl;
      std::cout << GetUsage() << GetHelp() << std::endl;
      return 2;
    }

    auto o = ReadAreas(config()->config()->datadir());
    if (!o) {
      return 2;
    }
    const auto area_num = to_number<int>(remaining().front());
    const auto dirs = o.value();
    if (area_num < 0 || area_num >= size_int(dirs)) {
      LOG(ERROR) << "invalid area number '" << area_num << "' specified. ";
      const auto max_size = std::max<int>(0, dirs.size() - 1);
      LOG(ERROR) << "area_num must be between 0 and " << max_size;
      return 1;
    }

    const auto& dir = at(dirs, area_num);
    sdk::files::FileApi api(config()->config()->datadir());
    auto area = api.Open(dir);
    if (!area) {
      LOG(ERROR) << "Unable to open file: " << dir.filename;
      return 1;
    }

    const auto file_number = arg("num").as_int();
    if (file_number < 1 || file_number > area->number_of_files()) {
      LOG(ERROR) << "invalid file number '" << file_number << "' specified. ";
      LOG(ERROR) << "num must be between 1 and " << area->number_of_files();
      return 1;
    }
    const auto f = area->ReadFile(file_number);
    std::cout << "Attempting to delete file: " << f.unaligned_filename();
    if (!area->DeleteFile(file_number)) {
      LOG(ERROR) << "Unable to delete file.";
      return 1;
    }
    return area->Close() ? 0: 1;
  }

  bool AddSubCommands() override {
    add_argument({"num", "File Number to delete.", "-1"});
    return true;
  }
};

bool FilesCommand::AddSubCommands() {
  if (!add(std::make_unique<AllowCommand>())) {
    return false;
  }
  if (!add(std::make_unique<ArcCommand>())) {
    return false;
  }
  if (!add(std::make_unique<TicCommand>())) {
    return false;
  }
  if (!add(std::make_unique<AreasCommand>())) {
    return false;
  }
  if (!add(std::make_unique<ListCommand>())) {
    return false;
  }
  if (!add(std::make_unique<DeleteFileCommand>())) {
    return false;
  }
  return true;
}

} // namespace
