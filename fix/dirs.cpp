/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

#include <string>
#include <vector>

#include "wwiv.h"
#include "fix.h"
#include "log.h"
#include "dirs.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace wwiv {
namespace fix {

void checkAllDirsExist() {
	Print(OK, true, "Checking Directories...");

  vector<string> dirs{ syscfg.msgsdir, syscfg.gfilesdir, syscfg.menudir, syscfg.datadir, syscfg.dloadsdir, syscfg.tempdir };

  for (const auto& dir_name : dirs) {
		File dir(dir_name);
    auto full_pathname = dir.full_pathname().c_str();
		if(!checkDirExists(dir, full_pathname)) {
			Print(NOK, true, "%s directory is missing", full_pathname);
			giveUp();
		}
	}
	Print(OK, true, "Basic directories present...");
}

// HACK - make string friendly unalign in BBS. This one is cribbed from batch.cpp
static char *unalign(char *pszFileName) {
	char* pszTemp = strstr(pszFileName, " ");
	if (pszTemp) {
		*pszTemp++ = '\0';
		char* pszTemp2 = strstr(pszTemp, ".");
		if (pszTemp2) {
			strcat(pszFileName, pszTemp2 );
		}
	}
	return pszFileName;
}

static string Unalign(const char* filename) {
    char s[MAX_PATH];
    strcpy(s, filename);
    return string(unalign(s));
}

void checkFileAreas(int num_dirs) {
	Print(OK, true, "Checking %d directories", num_dirs);
	for(int i = 0; i < num_dirs; i++) {
		if(!(directories[i].mask & mask_cdrom) && !(directories[i].mask & mask_offline)) {
			File dir(directories[i].path);
			if(checkDirExists(dir, directories[i].name)) {
				Print(OK, true, "Checking directory '%s'", directories[i].name);
				string filename = directories[i].filename;
				filename.append(".dir");
				File recordFile(syscfg.datadir, filename.c_str());
				if(recordFile.Exists()) {
					if(!recordFile.Open(File::modeReadWrite | File::modeBinary)) {
						Print(NOK, true, "Unable to open '%s'", recordFile.full_pathname().c_str());
					} else {
						unsigned int numFiles = recordFile.GetLength() / sizeof(uploadsrec);
						uploadsrec upload;
						recordFile.Read(&upload, sizeof(uploadsrec));
						if(upload.numbytes != numFiles) {
							Print(NOK, true, "Corrected # of files in %s.", directories[i].name);
							upload.numbytes = numFiles;
							recordFile.Seek(0L, File::seekBegin);
							recordFile.Write(&upload, sizeof(uploadsrec));
						}
						if(numFiles >= 1) {
							ext_desc_rec *extDesc = nullptr;
							unsigned int recNo = 0;
							string filenameExt = directories[i].filename;
							filenameExt.append(".ext");
							File extDescFile(syscfg.datadir, filenameExt.c_str());
							if(extDescFile.Exists()) {
								if(extDescFile.Open(File::modeReadWrite | File::modeBinary)) {
									extDesc = (ext_desc_rec *)malloc(numFiles * sizeof(ext_desc_rec));
									unsigned long offs = 0;
									while(offs < (unsigned long)extDescFile.GetLength() && recNo < numFiles) {
										ext_desc_type ed;
										extDescFile.Seek(offs, File::seekBegin);
										if(extDescFile.Read(&ed, sizeof(ext_desc_type)) == sizeof(ext_desc_type)) {
											strcpy(extDesc[recNo].name, ed.name);
											extDesc[recNo].offset = offs;
											offs += (long)ed.len + sizeof(ext_desc_type);
											recNo++;
										}
									}
								}
								extDescFile.Close();
							}
							for(unsigned int fileNo = 1; fileNo < numFiles; fileNo++) {
								bool modified = false;
								recordFile.Seek(sizeof(uploadsrec) * fileNo, File::seekBegin);
								recordFile.Read(&upload, sizeof(uploadsrec));
								bool extDescFound = false;
								for(unsigned int desc = 0; desc < recNo; desc++) {
									if(strcmp(upload.filename, extDesc[desc].name) == 0) {
										extDescFound = true;
									}
								}
								if(extDescFound && (upload.mask & mask_extended) == 0) {
									upload.mask |= mask_extended;
									modified = true;
									Print(NOK, true, "Fixed extended description for '%s'.", upload.filename);
								} else if(upload.mask & mask_extended) {
									upload.mask &= ~mask_extended;
									modified = true;
									Print(NOK, true, "Fixed extended description for '%s'.", upload.filename);
								}
								File file(directories[i].path, Unalign(upload.filename));
								if (strlen(upload.filename)>0 && file.Exists()) {
									if (file.Open(File::modeReadOnly | File::modeBinary)) {
                                        Print(OK, false, "Checking file '%s'.", file.full_pathname().c_str());
                                        if (upload.numbytes != (unsigned long)file.GetLength()) {

										    upload.numbytes = file.GetLength();
										    modified = true;
										    Print(NOK, true, "Fixed file size for '%s'.", upload.filename);
									    }
									    file.Close();
                                    } else {
                                        Print(NOK, true, "Unable to open file '%s', error '%s'.", 
                                            file.full_pathname().c_str(), file.GetLastError().c_str());
                                    }
								}
								if(modified) {
									recordFile.Seek(sizeof(uploadsrec) * fileNo, File::seekBegin);
									recordFile.Write(&upload, sizeof(uploadsrec));
								}
							}
							if(extDesc != nullptr) {
								free(extDesc);
                                extDesc = nullptr;
							}
						}
						recordFile.Close();
					}
				} else {
					Print(NOK, true, "Directory '%s' missing file '%s'", directories[i].name, recordFile.full_pathname().c_str());
				}
			}
		} else if(directories[i].mask & mask_offline) {
			Print(OK, true, "Skipping directory '%s' [OFFLINE]", directories[i].name);
		} else if(directories[i].mask & mask_cdrom) {
			Print(OK, true, "Skipping directory '%s' [CDROM]", directories[i].name);
		} else {
			Print(OK, true, "Skipping directory '%s' [UNKNOWN MASK]", directories[i].name);
		}
	}
}

int FixDirectoriesCommand::Execute() {
  cout << "Runnning FixDirectoriesCommand::Execute" << endl;
	checkAllDirsExist();
	checkFileAreas(num_dirs_);
  return 0;
}

}  // namespace fix
}  // namespavce wwiv

