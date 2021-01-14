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

}

#endif