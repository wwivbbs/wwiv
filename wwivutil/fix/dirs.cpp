/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "wwivutil/fix/dirs.h"

#include <iostream>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"

#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include "sdk/files/files.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

static bool checkDirExists(const std::string& dir, const char *desc) {
  if (File::Exists(dir)) {
    return true;
  }

  LOG(INFO) << "For File Area: " << desc;
  LOG(INFO) << "Unable to find dir: " << dir;
  LOG(INFO) << "Please create it.";
  return false;
  //printf("   Do you wish to CREATE it (y/N)?\n");
  //string s;
  //std::cin >> s;
  //if (s[0] == 'Y' || s[0] == 'y') {
  //  if (!File::mkdirs(dir)) {
  //    Print(NOK, true, "Unable to create dir '%s' for %s dir.", dir.full_pathname().c_str(), desc);
  //    return false;
  //  }
  //}
  //return true;
}

void checkFileAreas(const std::string& datadir, bool verbose) {
  vector<directoryrec_422_t> directories;
  {
    DataFile<directoryrec_422_t> file(FilePath(datadir, DIRS_DAT));
    if (!file) {
      // TODO(rushfan): show error?
      return;
    }
    if (!file.ReadVector(directories)) {
      // TODO(rushfan): show error?
      return;
    }
  }

  LOG(INFO) << "Checking " << directories.size() << " directories.";
  for (auto& d : directories) {
    if (!(d.mask & mask_cdrom) && !(d.mask & mask_offline)) {
      if (checkDirExists(d.path, d.name)) {
        LOG(INFO) << "Checking directory: " << d.name;
        const auto filename = StrCat(d.filename, ".dir");
        const auto record_path = FilePath(datadir, filename);
        if (File::Exists(record_path)) {
          File recordFile(record_path);
          if (!recordFile.Open(File::modeReadWrite | File::modeBinary)) {
            LOG(INFO) << "Unable to open:" << recordFile;
          } else {
            auto numFiles = static_cast<int>(recordFile.length() / sizeof(uploadsrec));
            uploadsrec upload{};
            recordFile.Read(&upload, sizeof(uploadsrec));
            if (static_cast<int>(upload.numbytes) != numFiles) {
              LOG(INFO) << "Corrected # of files in: " << d.name;
              upload.numbytes = numFiles;
              recordFile.Seek(0L, File::Whence::begin);
              recordFile.Write(&upload, sizeof(uploadsrec));
            }
            if (numFiles >= 1) {
              ext_desc_rec *extDesc = nullptr;
              int recNo = 0;
              const auto fn = FilePath(datadir, StrCat(d.filename, ".ext"));
              if (File::Exists(fn)) {
                File extDescFile(fn);
                if (extDescFile.Open(File::modeReadWrite | File::modeBinary)) {
                  extDesc = static_cast<ext_desc_rec*>(malloc(numFiles * sizeof(ext_desc_rec)));
                  auto offs = 0;
                  while (offs < extDescFile.length() && recNo < numFiles) {
                    ext_desc_type ed{};
                    extDescFile.Seek(offs, File::Whence::begin);
                    if (extDescFile.Read(&ed, sizeof(ext_desc_type)) == sizeof(ext_desc_type)) {
                      strcpy(extDesc[recNo].name, ed.name);
                      extDesc[recNo].offset = offs;
                      offs += static_cast<int>(ed.len + sizeof(ext_desc_type));
                      ++recNo;
                    }
                  }
                }
                extDescFile.Close();
              }
              for (int file_no = 1; file_no < numFiles; file_no++) {
                bool modified = false;
                recordFile.Seek(sizeof(uploadsrec) * file_no, File::Whence::begin);
                recordFile.Read(&upload, sizeof(uploadsrec));
                bool extDescFound = false;
                for (auto desc = 0; desc < recNo; desc++) {
                  if (strcmp(upload.filename, extDesc[desc].name) == 0) {
                    extDescFound = true;
                  }
                }
                if (extDescFound && ((upload.mask & mask_extended) == 0)) {
                  upload.mask |= mask_extended;
                  modified = true;
                  LOG(INFO) << "Fixed (added) extended description for: " << upload.filename;
                } else if (!extDescFound && (upload.mask & mask_extended)) {
                  upload.mask &= ~mask_extended;
                  modified = true;
                  LOG(INFO) << "Fixed (removed) extended description for: " << upload.filename;
                }
                const auto dir_fn = FilePath(d.path, wwiv::sdk::files::unalign(upload.filename));
                File file(dir_fn);
                if (strlen(upload.filename) > 0 && File::Exists(dir_fn)) {
                  if (file.Open(File::modeReadOnly | File::modeBinary)) {
                    if (verbose) {
                      LOG(INFO) << "Checking file: " << file;
                    }
                    if (upload.numbytes != static_cast<decltype(upload.numbytes)>(file.length())) {
                      upload.numbytes = file.length();
                      modified = true;
                      LOG(INFO) << "Fixed file size for: " << upload.filename;
                    }
                    file.Close();
                  } else {
                    LOG(INFO) << "Unable to open file '" << dir_fn
                              << "', error:" << file.last_error();
                  }
                }
                if (modified) {
                  recordFile.Seek(sizeof(uploadsrec) * file_no, File::Whence::begin);
                  recordFile.Write(&upload, sizeof(uploadsrec));
                }
              }
              if (extDesc != nullptr) {
                free(extDesc);
                extDesc = nullptr;
              }
            }
            recordFile.Close();
          }
        } else {
          LOG(INFO) << "Directory '" << d.name << "' missing file: " << record_path;
        }
      }
    } else if (d.mask & mask_offline) {
      LOG(INFO) << "Skipping directory '" << d.name << "' [OFFLINE]";
    } else if (d.mask & mask_cdrom) {
      LOG(INFO) << "Skipping directory '" << d.name << "' [CDROM]";
    } else {
      LOG(INFO) << "Skipping directory '" << d.name << "' [UNKNOWN MASK]";
    }
  }
}

std::string FixDirectoriesCommand::GetUsage() const {
  std::ostringstream ss;
  ss<< "Usage:   fix dirs" << endl;
  ss << "Example: WWIVUTIL fix dirs" << endl;
  return ss.str();
}

bool FixDirectoriesCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument("verbose", 'v', "Enable verbose output.", false));

  return true;
}

int FixDirectoriesCommand::Execute() {
  cout << "Runnning FixDirectoriesCommand::Execute" << endl;

  checkFileAreas(config()->config()->datadir(), arg("verbose").as_bool());
  return 0;
}

} // namespavce wwiv

