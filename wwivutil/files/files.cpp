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
#include "wwivutil/files/files.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/stl.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/files/files.h"
#include "wwivutil/files/allow.h"

using std::clog;
using std::cout;
using std::endl;
using std::setw;
using std::string;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil::files {

static bool ReadAreas(const std::string& datadir, vector<directoryrec>& dirs) {
  DataFile<directoryrec> file(PathFilePath(datadir, DIRS_DAT));
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << file.file();
    return false;
  }
  if (!file.ReadVector(dirs)) {
    LOG(ERROR) << "Unable to read from file: " << file.file();
    return false;
  }
  return true;
}

class AreasCommand final : public UtilCommand {
public:
  AreasCommand()
    : UtilCommand("areas", "Lists the file areas") {}

  virtual ~AreasCommand() {}

  std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   areas" << endl;
    return ss.str();
  }

  int Execute() override {
    vector<directoryrec> dirs;
    if (!ReadAreas(config()->config()->datadir(), dirs)) {
      return 2;
    }

    int num = 0;
    cout << "#Num FileName " << std::setw(30) << std::left << "Name" << " " 
      << "Path" << std::endl;
    cout << string(78, '=') << endl;
    for (const auto& d : dirs) {
      cout << "#" << std::setw(3) << std::left << num++ << " "
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

  virtual ~ListCommand() {}

  std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   list [num]" << endl;
    return ss.str();
  }

  int Execute() override {
    if (remaining().empty()) {
      clog << "Missing file areas #." << endl;
      cout << GetUsage() << GetHelp() << endl;
      return 2;
    }
    vector<directoryrec> dirs;
    if (!ReadAreas(config()->config()->datadir(), dirs)) {
      return 2;
    }
    const auto area_num = to_number<int>(remaining().front());

    if (area_num < 0 || area_num >= ssize(dirs)) {
      LOG(ERROR) << "invalid area number '" << area_num << "' specified. ";
      const auto max_size = std::max<int>(0, dirs.size() - 1);
      LOG(ERROR) << "area_num must be between 0 and " << max_size;
      return 1;
    }
    sdk::files::FileApi api(config()->config()->datadir());
    const auto& dir = dirs.at(area_num);
    auto area = api.Open(dirs.at(area_num));
    if (!area) {
      LOG(ERROR) << "Unable to open area: #" << area_num << "; filename: " << dir.filename;
      return 1;
    }

    auto num_files = area->number_of_files();
    cout << fmt::format("File Area: {} ({} files)", dir.name, num_files) << std::endl;
    cout << std::endl;
    const auto& h = area->header();
    if (static_cast<int>(h.num_files()) != num_files) {
      cout << fmt::format("WARNING: Header doesn't match header:{} vs. size:{}", h.num_files(),
                          num_files)
           << std::endl;
    }
    cout << "#Num File Name   " << std::left << "Description" << std::endl;
    cout << string(78, '=') << endl;
    for (auto num = 1; num <= num_files; num++) {
      auto f = area->ReadFile(num);
      cout << "#" << std::setw(3) << std::left << num << " "
           << std::setw(8) << f.unaligned_filename() << " "
           << f.u().description << std::endl;
    }
    return 0;
  }

  bool AddSubCommands() override {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every area.", false));
    return true;
  }
};

class DeleteFileCommand : public UtilCommand {
public:
  DeleteFileCommand() : UtilCommand("delete", "Deleate a file in an area") {}

  virtual ~DeleteFileCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   delete --num=NN <sub #>" << endl;
    ss << "Example: delete --num=10 1" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      clog << "Missing file area #." << endl;
      cout << GetUsage() << GetHelp() << endl;
      return 2;
    }

    vector<directoryrec> dirs;
    if (!ReadAreas(config()->config()->datadir(), dirs)) {
      return 2;
    }
    const auto area_num = to_number<int>(remaining().front());

    if (area_num < 0 || area_num >= ssize(dirs)) {
      LOG(ERROR) << "invalid area number '" << area_num << "' specified. ";
      const auto max_size = std::max<int>(0, dirs.size() - 1);
      LOG(ERROR) << "area_num must be between 0 and " << max_size;
      return 1;
    }

    const auto& dir = dirs.at(area_num);
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

  bool AddSubCommands() override final {
    add_argument({"num", "File Number to delete.", "-1"});
    return true;
  }
};

bool FilesCommand::AddSubCommands() {
  if (!add(make_unique<AllowCommand>())) {
    return false;
  }
  if (!add(make_unique<AreasCommand>())) {
    return false;
  }
  if (!add(make_unique<ListCommand>())) {
    return false;
  }
  if (!add(make_unique<DeleteFileCommand>())) {
    return false;
  }
  return true;
}

} // namespace wwiv::wwivutil::files
