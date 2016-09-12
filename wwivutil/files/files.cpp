/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016 WWIV Software Services              */
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
#include <cstdio>
#include <ctime>
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
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/net.h"
#include "sdk/names.h"
#include "sdk/networks.h"

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

namespace wwiv {
namespace wwivutil {

static bool ReadAreas(const std::string& datadir, vector<directoryrec>& dirs) {
  DataFile<directoryrec> file(datadir, DIRS_DAT);
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << file.file().full_pathname();
    return false;
  }
  if (!file.ReadVector(dirs)) {
    LOG(ERROR) << "Unable to read from file: " << file.file().full_pathname();
    return false;
  }
  return true;
}

class AreasCommand: public UtilCommand {
public:
  AreasCommand()
    : UtilCommand("areas", "Lists the file areas") {}

  virtual ~AreasCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   areas" << endl;
    return ss.str();
  }

  int Execute() override final {
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

  bool AddSubCommands() override final {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every area.", false));
    return true;
  }
};

class ListCommand: public UtilCommand {
public:
  ListCommand()
    : UtilCommand("list", "Lists the files in an area") {}

  virtual ~ListCommand() {}

  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage:   list [num]" << endl;
    return ss.str();
  }

  int Execute() override final {
    if (remaining().empty()) {
      clog << "Missing file areas #." << endl;
      cout << GetUsage() << GetHelp() << endl;
      return 2;
    }
    vector<directoryrec> dirs;
    if (!ReadAreas(config()->config()->datadir(), dirs)) {
      return 2;
    }
    int area_num = StringToInt(remaining().front());

    if (area_num < 0 || area_num >= size_int(dirs)) {
      LOG(ERROR) << "invalid area number '" << area_num << "' specified. ";
      LOG(ERROR) << "area_num must be between 0 and " << std::max(0U, dirs.size() - 1);
      return 1;
    }

    const auto& dir = dirs.at(area_num);
    const string filename = StrCat(dir.filename, ".dir");
    DataFile<uploadsrec> file(config()->config()->datadir(), filename);
    if (!file) {
      LOG(ERROR) << "Unable to open file: " << file.file().full_pathname();
      return 1;
    }
    vector<uploadsrec> files;
    if (!file.ReadVector(files)) {
      LOG(ERROR) << "Unable to read dir entries from file: " << file.file().full_pathname();
      return 1;
    }
    int num = 0;
    cout << "#Num FileName " << std::left << "Description" << std::endl;
    cout << string(78, '=') << endl;
    for (const auto& f : files) {
      if (num == 0) {
        num = 1;
        continue;
      }
      cout << "#" << std::setw(3) << std::left << num++ << " "
        << std::setw(8) << f.filename << " "
        << f.description << std::endl;
    }
    return 0;
  }

  bool AddSubCommands() override final {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every area.", false));
    return true;
  }
};

class DeleteFileCommand: public UtilCommand {
public:
  DeleteFileCommand()
    : UtilCommand("delete", "Deleate a file in an area") {}

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
    int area_num = StringToInt(remaining().front());

    if (area_num < 0 || area_num >= size_int(dirs)) {
      LOG(ERROR) << "invalid area number '" << area_num << "' specified. ";
      LOG(ERROR) << "area_num must be between 0 and " << std::max(0U, dirs.size() - 1);
      return 1;
    }

    const auto& dir = dirs.at(area_num);
    const string filename = StrCat(dir.filename, ".dir");
    DataFile<uploadsrec> file(config()->config()->datadir(), filename,
      File::modeBinary | File::modeReadWrite);
    if (!file) {
      LOG(ERROR) << "Unable to open file: " << file.file().full_pathname();
      return 1;
    }
    vector<uploadsrec> files;
    if (!file.ReadVector(files)) {
      LOG(ERROR) << "Unable to read dir entries from file: " << file.file().full_pathname();
      return 1;
    }

    int file_number = arg("num").as_int();
    if (file_number < 0 || file_number >= size_int(files)) {
      LOG(ERROR) << "invalid file number '" << area_num << "' specified. ";
      LOG(ERROR) << "num must be between 0 and " << std::max(0U, files.size() - 1);
      return 1;
    }

    auto iter = files.begin();
    std::advance(iter, file_number); // move to file number.
    // erase item.
    files.erase(iter);

    file.Seek(0);
    if (!file.WriteVector(files)) {
      LOG(ERROR) << "Unable to write dir entries in file: " << file.file().full_pathname();
      return 1;
    }
    // TODO(rushfan): Push this into the DataFile class.
    file.file().SetLength(files.size() * sizeof(uploadsrec));

    // TODO(rushfan): Set the # files in record 0.

    return 0;
  }

  bool AddSubCommands() override final {
    add_argument({"num", "File Number to delete.", "-1"});
    return true;
  }
};

bool FilesCommand::AddSubCommands() {
  if (!add(make_unique<AreasCommand>())) { return false; }
  if (!add(make_unique<ListCommand>())) { return false; }
  if (!add(make_unique<DeleteFileCommand>())) { return false; }
  return true;
}


}  // namespace wwivutil
}  // namespace wwiv
