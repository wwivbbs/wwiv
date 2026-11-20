/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2022, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FILES_DIZ_H__
#define __INCLUDED_SDK_FILES_DIZ_H__

#include <filesystem>
#include <optional>
#include <string>

namespace wwiv::sdk::files {

class Diz final {
public:
  explicit Diz(std::string description, std::string extended_description);
  Diz() = delete;
  ~Diz() = default;

  [[nodiscard]] std::string description() const noexcept;
  [[nodiscard]] std::string extended_description() const noexcept;
private:
  const std::string description_;
  const std::string extended_description_;
};

class DizParser final {
public:
  explicit DizParser(bool firstline_as_desc);
  DizParser() = delete;
  ~DizParser() = default;

  [[nodiscard]] std::optional<Diz> parse(const std::filesystem::path& path) const;
private:
  bool firstline_as_desc_;
};

/**
 * Returns true if 'filename' is FILE_ID.DIZ or DESC.SDI, ignoring case.
 */
[[nodiscard]] bool is_diz_filename(const std::string& filename);

/**
 * Removes every FILE_ID.DIZ or DESC.SDI in 'dir', whatever its case.
 *
 * Archivers keep the case stored in the archive, so on a case sensitive
 * filesystem an extracted description may be file_id.diz rather than
 * FILE_ID.DIZ. Returns the number of files removed.
 */
int remove_diz_files(const std::filesystem::path& dir);

/**
 * Finds a FILE_ID.DIZ (preferred) or DESC.SDI in 'dir', whatever its case.
 */
[[nodiscard]] std::optional<std::filesystem::path> find_diz_file(const std::filesystem::path& dir);
}

#endif