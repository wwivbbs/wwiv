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

class FileRecord final {
public:
  explicit FileRecord(const uploadsrec& u) : u_(u) {}
  FileRecord();
  ~FileRecord() = default;

  uploadsrec& u() { return u_; }
  uint32_t numbytes() const { return u_.numbytes; }

  bool set_filename(const std::string& unaligned_filename);
  bool set_description(const std::string& desc);
  std::string aligned_filename() const ;
  std::string unaligned_filename() const ;

private:
  uploadsrec u_;
};

/**
 * Represents a file area header.
 * Internally this is stuffed into an uploadsrec re-purposing some
 * of the fields:
 *
 * filename must contain "|MARKER|"
 * numbytes is the total number of files in this area.
 * daten is the date of the newest file.
 */
class FileAreaHeader {
public:
  explicit FileAreaHeader(const uploadsrec& u);
  ~FileAreaHeader() = default;

  uint32_t num_files() const { return u_.numbytes; }
  bool set_num_files(uint32_t n) { u_.numbytes = n; return true; }
  bool FixHeader(const core::Clock& clock, uint32_t num_files);
  uploadsrec& u() { return u_; }
  void set_daten(daten_t d);
  daten_t daten() const;
private:
  uploadsrec u_;
};

class FileArea final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = FileRecord;
  using difference_type = int;
  using pointer = FileRecord*;
  using reference = FileRecord&;

  FileArea(FileApi* api, std::string data_directory, const directoryrec& dir);
  FileArea(FileApi* api, std::string data_directory, const std::string& filename);
  ~FileArea() = default;
  
  // File Header specific
  bool FixFileHeader();
  FileAreaHeader& header() const;

  // File Dir Specific Operations
  bool Save();
  bool Close();
  bool Lock();
  bool Unlock();
  int number_of_files() const;

  // File specific
  FileRecord ReadFile(int num);
  bool AddFile(FileRecord& f);
  bool UpdateFile(FileRecord& f, int num);
  bool DeleteFile(int file_number);

protected:
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

  std::unique_ptr<FileAreaHeader> header_;
};

std::string align(const std::string& file_name);
std::string unalign(const std::string& file_name);

} // namespace wwiv::sdk::files

#endif  // __INCLUDED_SDK_FILES_FILES_H__
