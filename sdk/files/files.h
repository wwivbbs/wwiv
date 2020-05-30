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
#ifndef __INCLUDED_SDK_FILES_FILES_H__
#define __INCLUDED_SDK_FILES_FILES_H__

#include "core/clock.h"
#include "sdk/config.h"
#include <filesystem>
#include <string>
#include <vector>

namespace wwiv::sdk::files {

class FileArea;

class FileApi {
public:
  virtual ~FileApi() = default;
  FileApi(std::string data_directory);

  [[nodiscard]] bool Exist(const std::string& filename) const;
  [[nodiscard]] bool Exist(const directoryrec& dir) const;
  [[nodiscard]] bool Create(const std::string& filename);
  [[nodiscard]] bool Create(const directoryrec& dir);
  [[nodiscard]] bool Remove(const std::string& filename);
  [[nodiscard]] std::unique_ptr<FileArea> Open(const std::string& filename);
  [[nodiscard]] std::unique_ptr<FileArea> Open(const directoryrec& dir);

  [[nodiscard]] const core::Clock* clock() const noexcept;
  void set_clock(std::unique_ptr<core::Clock> clock);

private:
  std::string data_directory_;
  std::unique_ptr<core::Clock> clock_;
};

class FileRecord {
public:
  explicit FileRecord(const uploadsrec& u) : u_(u) {};
  virtual ~FileRecord() = default;

  uploadsrec& u() { return u_; }

  bool set_filename(const std::string unaligned_filename);
  std::string aligned_filename();
  std::string unaligned_filename();

private:
  uploadsrec u_;
};

class FileArea {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = FileRecord;
  using difference_type = int;
  using pointer = FileRecord*;
  using reference = FileRecord&;

  FileArea(FileApi* api, std::string data_directory, const directoryrec& dir);
  FileArea(FileApi* api, std::string data_directory, const std::string& filename);
  virtual ~FileArea() = default;
  
  // File Header specific
  bool FixFileHeader();

  // File Dir Specific Operations
  bool Close();
  bool Lock();
  bool Unlock();
  int number_of_files() const;

  // File specific
  FileRecord ReadFile(int num);
  bool AddFile(FileRecord& f);
  bool DeleteFile(int file_number);

protected:
  bool Save();
  std::filesystem::path path() const noexcept;

  // Not owned.
  FileApi* api_;
  const std::string data_directory_;
  const std::string filename_;
  directoryrec dir_{};

  bool dirty_{false};
  bool open_{false};
  std::vector<uploadsrec> files_;
  std::unique_ptr<core::Clock> clock_;
};

std::string align(const std::string& file_name);
std::string unalign(const std::string& file_name);

} // namespace wwiv::sdk::files

#endif  // __INCLUDED_SDK_FILES_FILES_H__
