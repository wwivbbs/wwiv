/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FILES_FILES_EXT_H__
#define __INCLUDED_SDK_FILES_FILES_EXT_H__

#include "sdk/files/file_record.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::sdk::files {


class FileApi;
class FileRecord;

class FileAreaExtendedDesc final {
public:

  FileAreaExtendedDesc(FileApi* api, std::string data_directory, const directoryrec& dir, int num_files);
  FileAreaExtendedDesc(FileApi* api, std::string data_directory, const std::string& filename, int num_files);
  ~FileAreaExtendedDesc() = default;
  
  // File Dir Specific Operations
  bool Load();
  bool Save(); // NOP?
  bool Close();
  bool Lock() { return true; }
  bool Unlock() { return true; }
  int number_of_files() const { return num_files_; };
  bool Compact() { return false; }

  // File specific
  bool AddExtended(const FileRecord& f, const std::string& text);
  bool DeleteExtended(const FileRecord& f);
  std::optional<std::string> ReadExtended(const FileRecord& f);
  std::optional<std::vector<std::string>> ReadExtendedAsLines(const FileRecord& f);
  bool UpdateExtended(const FileRecord& f, const std::string& text);

protected:
  [[nodiscard]] std::filesystem::path path() const noexcept;

  // Not owned.
  FileApi* api_;
  const std::string data_directory_;
  const std::string filename_;
  directoryrec dir_{};

  bool dirty_{false};
  bool open_{false};
  std::vector<ext_desc_rec> ext_;
  int num_files_;
};

std::string align(const std::string& file_name);
std::string unalign(const std::string& file_name);

} // namespace wwiv::sdk::files

#endif  // __INCLUDED_SDK_FILES_FILES_EXT_H__
