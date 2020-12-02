/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2020, WWIV Software Services             */
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

#ifndef INCLUDED_SDK_GFILES_H
#define INCLUDED_SDK_GFILES_H

#include "core/stl.h"
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::sdk {

struct gfile_t {
  // description of file
  std::string description;
  // filename of file
  std::string filename;
  // date added
  daten_t daten;
};

struct gfile_dir_t {
  // g-file section name
  std::string name;
  // g-file database filename
  std::string filename;
  // ACS for accessing this area.
  std::string acs;
  // max files for directory
  int maxfiles{500};
  // The gfiles for this area.
  std::vector<gfile_t> files;
};


class GFiles final {
public:
  typedef gfile_dir_t& reference;
  typedef const gfile_dir_t& const_reference;
  explicit GFiles(std::filesystem::path datadir, int max_backups);
  ~GFiles();

  bool LoadLegacy();
  bool Load();
  bool Save();

  [[nodiscard]] const gfile_dir_t& dir(std::size_t n) const { return stl::at(dirs_, n); }
  [[nodiscard]] const gfile_dir_t& dir(const std::string& filename) const;
  gfile_dir_t& dir(std::size_t n) { return dirs_[n]; }
  gfile_dir_t& dir(const std::string& filename);

  const gfile_dir_t& operator[](std::size_t n) const { return dir(n); }
  const gfile_dir_t& operator[](const std::string& filename) const { return dir(filename); }
  gfile_dir_t& operator[](std::size_t n) { return dir(n); }
  gfile_dir_t& operator[](const std::string& filename) { return dir(filename); }

  [[nodiscard]] bool exists(const std::string& filename) const;

  void set_dir(int n, gfile_dir_t s) { dirs_[n] = std::move(s); }
  [[nodiscard]] const std::vector<gfile_dir_t>& dirs() const { return dirs_; }
  bool insert(int n, gfile_dir_t r);
  bool erase(int n);
  [[nodiscard]] int size() const { return stl::size_int(dirs_); }

  static bool LoadFromJSON(const std::filesystem::path& dir, const std::string& filename,
                           std::vector<gfile_dir_t>& entries);
  bool SaveToJSON(const std::filesystem::path& dir, const std::string& filename,
                  const std::vector<gfile_dir_t>& entries);

  bool set_dirs(const std::vector<gfile_dir_t>& dirs);

private:
  const std::filesystem::path datadir_;
  const int max_backups_;
  std::vector<gfile_dir_t> dirs_;
};

} // namespace wwiv::sdk

#endif
