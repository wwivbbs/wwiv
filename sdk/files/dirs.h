/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2022, WWIV Software Services             */
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

#ifndef INCLUDED_SDK_FILES_DIRS_H
#define INCLUDED_SDK_FILES_DIRS_H

#include "core/stl.h"
#include "core/uuid.h"
#include "sdk/conf/conf_set.h"
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::sdk::files {

struct dir_area_t {
  std::string area_tag;
  core::uuid_t net_uuid;  
};

// 5.5 style directory. 
struct directory_55_t {
  // directory name
  std::string name;
  // direct database filename
  std::string filename;
  // filename path
  std::string path;
  // DSL for directory
  uint8_t dsl;
  // minimum age for directory
  uint8_t age;
  // DAR for directory
  uint16_t dar;
  // max files for directory
  uint16_t maxfiles;
  // file type mask
  uint16_t mask;
  // AREA TAGs (used for FTN File Echos)
  std::vector<dir_area_t> area_tags;
};

// 5.6+ style directory. 
struct directory_t {
  // directory name
  std::string name;
  // direct database filename
  std::string filename;
  // directory path
  std::string path;
  // ACS for accessing this area.
  std::string acs;
  // max files for directory
  uint16_t maxfiles{500};
  // file type mask
  uint16_t mask{};
  // AREA TAGs (used for FTN File Echos)
  std::vector<dir_area_t> area_tags;
  // Conference keys.
  conf_set_t conf;

  friend std::ostream& operator<<(std::ostream& os, const directory_t& f);
};

class Dirs final {
public:
  typedef directory_t& reference;
  typedef const directory_t& const_reference;
  explicit Dirs(std::filesystem::path datadir, int max_backups);
  ~Dirs();

  bool LoadLegacy();
  bool Load();
  bool Save();

  [[nodiscard]] const directory_t& dir(std::size_t n) const { return stl::at(dirs_, n); }
  [[nodiscard]] const directory_t& dir(const std::string& filename) const;
  directory_t& dir(std::size_t n) { return dirs_[n]; }
  directory_t& dir(const std::string& filename);

  const directory_t& operator[](std::size_t n) const { return dir(n); }
  const directory_t& operator[](const std::string& filename) const { return dir(filename); }
  directory_t& operator[](std::size_t n) { return dir(n); }
  directory_t& operator[](const std::string& filename) { return dir(filename); }

  [[nodiscard]] bool exists(const std::string& filename) const;

  void set_dir(int n, directory_t s) { dirs_[n] = std::move(s); }
  [[nodiscard]] const std::vector<directory_t>& dirs() const { return dirs_; }
  bool insert(int n, directory_t r);
  bool erase(int n);
  [[nodiscard]] int size() const { return stl::size_int(dirs_); }

  static bool LoadFromJSON(const std::filesystem::path& dir, const std::string& filename, std::vector<directory_t>& entries);
  static bool SaveToJSON(const std::filesystem::path& dir, const std::string& filename, const std::vector<directory_t>& entries);

  bool set_dirs(const std::vector<directory_t>& dirs);

private:
  const std::filesystem::path datadir_;
  const int max_backups_;
  std::vector<directory_t> dirs_;
};

}


#endif

