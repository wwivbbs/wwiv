/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#ifndef INCLUDED_CORE_FINDFILES_H
#define INCLUDED_CORE_FINDFILES_H

#include <filesystem>
#include <set>
#include <string>
#include <utility>

namespace wwiv::core {

class FileEntry {
public:
  FileEntry(std::string n, long s) : name(std::move(n)), size(s) {}

  bool operator<(const FileEntry& other) const {
    return name < other.name;
  }
  bool operator == (const FileEntry& other) const {
    return name == other.name;
  }
  const std::string name;
  long size;
  
};

class FindFiles {
public:
  typedef std::set<FileEntry>::iterator iterator;
  typedef std::set<FileEntry>::const_iterator const_iterator;
  typedef std::set<FileEntry>::size_type size_type;

  enum class FindFilesType { directories, files, any };
  enum class WinNameType { short_name, long_name };

  FindFiles(const std::filesystem::path& mask, FindFilesType type, WinNameType name_type);
  FindFiles(const std::filesystem::path& mask, FindFilesType type);

  [[nodiscard]] iterator begin() { return entries_.begin(); }
  [[nodiscard]] const_iterator begin() const { return entries_.begin(); }
  [[nodiscard]] const_iterator cbegin() const { return entries_.cbegin(); }
  [[nodiscard]] iterator end() { return entries_.end(); }
  [[nodiscard]] const_iterator end() const { return entries_.end(); }
  [[nodiscard]] const_iterator cend() const { return entries_.cend(); }
  [[nodiscard]] bool empty() const { return entries_.empty(); }
  [[nodiscard]] size_type size() const noexcept { return entries_.size(); }

private:
  std::set<FileEntry> entries_;
  
};

}


#endif
