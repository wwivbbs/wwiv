/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

using std::cout;
using std::endl;
using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

bool checkDirExists(File &dir, const char *desc) {
  if (dir.Exists()) {
    return true;
  }

  LOG(INFO) << "For File Area: " << desc;
  LOG(INFO) << "Unable to find dir: " << dir.full_pathname();
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

// HACK - make string friendly unalign in BBS. This one is cribbed from batch.cpp
static char *unalign(char *file_name) {
	char* temp = strstr(file_name, " ");
	if (temp) {
		*temp++ = '\0';
		char* temp2 = strstr(temp, ".");
		if (temp2) {
			strcat(file_name, temp2 );
		}
	}
	return file_name;
}

static string Unalign(const char* filename) {
    char s[MAX_PATH];
    strcpy(s, filename);
    return string(unalign(s));
}

void checkFileAreas(const std::string& datadir, bool verbose) {
  vector<directoryrec> directories;
  {
    DataFile<directoryrec> file(datadir, DIRS_DAT);
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
  for (size_t i = 0; i < directories.size(); i++) {
    if (!(directories[i].mask & mask_cdrom) && !(directories[i].mask & mask_offline)) {
      File dir(directories[i].path);
      if (checkDirExists(dir, directories[i].name)) {
        LOG(INFO) << "Checking directory: " << directories[i].name;
        string filename = StrCat(directories[i].filename, ".dir");
        File recordFile(datadir, filename);
        if (recordFile.Exists()) {
          if (!recordFile.Open(File::modeReadWrite | File::modeBinary)) {
            LOG(INFO) << "Unable to open:" << recordFile.full_pathname();
          } else {
            unsigned int numFiles = recordFile.GetLength() / sizeof(uploadsrec);
            uploadsrec upload;
            recordFile.Read(&upload, sizeof(uploadsrec));
            if (upload.numbytes != numFiles) {
              LOG(INFO) << "Corrected # of files in: " << directories[i].name;
              upload.numbytes = numFiles;
              recordFile.Seek(0L, File::Whence::begin);
              recordFile.Write(&upload, sizeof(uploadsrec));
            }
            if (numFiles >= 1) {
              ext_desc_rec *extDesc = nullptr;
              unsigned int recNo = 0;
              string filenameExt = directories[i].filename;
              filenameExt.append(".ext");
              File extDescFile(datadir, filenameExt);
              if (extDescFile.Exists()) {
                if (extDescFile.Open(File::modeReadWrite | File::modeBinary)) {
                  extDesc = (ext_desc_rec *)malloc(numFiles * sizeof(ext_desc_rec));
                  size_t offs = 0;
                  while (offs < (unsigned long)extDescFile.GetLength() && recNo < numFiles) {
                    ext_desc_type ed;
                    extDescFile.Seek(offs, File::Whence::begin);
                    if (extDescFile.Read(&ed, sizeof(ext_desc_type)) == sizeof(ext_desc_type)) {
                      strcpy(extDesc[recNo].name, ed.name);
                      extDesc[recNo].offset = offs;
                      offs += ed.len + sizeof(ext_desc_type);
                      recNo++;
                    }
                  }
                }
                extDescFile.Close();
              }
              for (size_t fileNo = 1; fileNo < numFiles; fileNo++) {
                bool modified = false;
                recordFile.Seek(sizeof(uploadsrec) * fileNo, File::Whence::begin);
                recordFile.Read(&upload, sizeof(uploadsrec));
                bool extDescFound = false;
                for (unsigned int desc = 0; desc < recNo; desc++) {
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
                File file(directories[i].path, Unalign(upload.filename));
                if (strlen(upload.filename) > 0 && file.Exists()) {
                  if (file.Open(File::modeReadOnly | File::modeBinary)) {
                    if (verbose) {
                      LOG(INFO) << "Checking file: " << file.full_pathname();
                    }
                    if (upload.numbytes != (unsigned long)file.GetLength()) {
                      upload.numbytes = file.GetLength();
                      modified = true;
                      LOG(INFO) << "Fixed file size for: " << upload.filename;
                    }
                    file.Close();
                  } else {
                    LOG(INFO) << "Unable to open file '" << file.full_pathname()
                      << "', error:" << file.GetLastError();
                  }
								}
								if (modified) {
									recordFile.Seek(sizeof(uploadsrec) * fileNo, File::Whence::begin);
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
          LOG(INFO) << "Directory '" << directories[i].name
            << "' missing file: " << recordFile.full_pathname();
				}
			}
		} else if (directories[i].mask & mask_offline) {
      LOG(INFO) << "Skipping directory '" << directories[i].name << "' [OFFLINE]";
		} else if (directories[i].mask & mask_cdrom) {
      LOG(INFO) << "Skipping directory '" << directories[i].name << "' [CDROM]";
		} else {
      LOG(INFO) << "Skipping directory '" << directories[i].name << "' [UNKNOWN MASK]";
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

}  // namespace wwivutil
}  // namespavce wwiv

