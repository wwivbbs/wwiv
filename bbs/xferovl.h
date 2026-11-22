/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_XFEROVL_H
#define INCLUDED_BBS_XFEROVL_H

#include <string>
#include <vector>

namespace wwiv::sdk::files {
class FileRecord;
}

/** One file to upload, as listed in an upload list file. See upload_files. */
struct UploadListEntry {
  std::string file_name;
  std::string description;
  /** Extended description lines, each ending in "\r\n". */
  std::string extended_description;
};

/**
 * Parses the lines of an upload list file, as read by fgets (so each line may
 * still end in '\n'). type is the number of words to skip between the file
 * name and the description. Lines starting with a space are extended
 * description lines for the file before them.
 */
std::vector<UploadListEntry> parse_upload_list(const std::vector<std::string>& lines, int type);

void move_file();
void sortdir(int directory_num, int type);
void sort_all(int type);
void rename_file();
bool maybe_upload(const std::string& file_name, int directory_num, const std::string& description);
void upload_files(const std::string& file_name, int directory_num, int type);
bool uploadall(int directory_num);
void relist();
void edit_database();
void add_to_file_database(const std::string& file_name);
void add_to_file_database(const wwiv::sdk::files::FileRecord& f);
void remove_from_file_database(const std::string& file_name);
bool is_uploadable(const std::string& file_name);
void xfer_defaults();
void finddescription();
void arc_l();

#endif
