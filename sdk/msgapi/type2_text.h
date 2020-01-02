/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_TYPE2_TEXT_H__
#define __INCLUDED_SDK_TYPE2_TEXT_H__

#include "core/file.h"
#include "sdk/msgapi/message_wwiv.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
namespace msgapi {

typedef uint16_t gati_t;
static constexpr int32_t GAT_NUMBER_ELEMENTS = 2048;
static constexpr int32_t GAT_SECTION_SIZE = GAT_NUMBER_ELEMENTS * sizeof(gati_t);
static constexpr int32_t MSG_BLOCK_SIZE = 512;
static constexpr int32_t GATSECLEN = GAT_SECTION_SIZE + GAT_NUMBER_ELEMENTS * MSG_BLOCK_SIZE;
static constexpr uint8_t STORAGE_TYPE = 2;


class Type2Text {
public:
  explicit Type2Text(std::filesystem::path text_filename);

  [[nodiscard]] std::vector<gati_t> load_gat(wwiv::core::File& file, int section);
  void save_gat(core::File& f, size_t section, const std::vector<gati_t>& gat);
  [[nodiscard]] std::optional<std::string> readfile(const messagerec& msg);
  [[nodiscard]] std::optional<messagerec> savefile(const std::string& text);
  [[nodiscard]] bool remove_link(const messagerec& msg);

private:
  [[nodiscard]] std::optional<core::File> OpenMessageFile() const;
  const std::filesystem::path path_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_TYPE2_TEXT_H__
