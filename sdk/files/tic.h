/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_FILES_TIC_H
#define INCLUDED_SDK_FILES_TIC_H

#include "core/datetime.h"
#include "sdk/fido/fido_address.h"
#include "sdk/files/dirs.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::sdk::net {
class Network;
}

namespace wwiv::sdk::files {

class Tic {
public:
  explicit Tic(std::filesystem::path path);
  Tic() = delete;
  ~Tic() = default;

  // True if everything is valid (file exists, PW correct, and CRC matches)
  [[nodiscard]] bool IsValid() const;
  [[nodiscard]] bool crc_valid() const;
  [[nodiscard]] bool size_valid() const;
  [[nodiscard]] bool exists() const;
  [[nodiscard]] int size() const;
  [[nodiscard]] core::DateTime date() const;
  [[nodiscard]] std::filesystem::path fpath() const;

  std::string area;
  std::string area_description;
  fido::FidoAddress origin;
  fido::FidoAddress from;
  fido::FidoAddress to;
  std::string file;
  std::string lfile;
  core::DateTime date_;
  std::string desc;
  std::vector<std::string> ldesc;
  std::string created;
  std::string magic;
  std::string replaces;
  std::string crc;
  std::string ftn_path;
  std::string seen_by;
  std::string pw;
private:
  std::filesystem::path path_;
  bool valid_;
  int size_{};
  friend class TicParser;
};  // class

class TicParser {
public:
  explicit TicParser(std::filesystem::path dir);
  [[nodiscard]] std::optional<Tic> parse(const std::string& filename) const;
  [[nodiscard]] std::optional<Tic> parse(const std::string& filename, const std::vector<std::string>& lines) const;

private:

  const std::filesystem::path dir_;
};

// Helper classes

std::optional<directory_t> FindFileAreaForTic(const files::Dirs& dirs, const Tic& tic,
                                              const sdk::net::Network& net);

} 

#endif
