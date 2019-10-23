/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
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
static constexpr uint32_t GAT_NUMBER_ELEMENTS = 2048;
static constexpr uint32_t GAT_SECTION_SIZE = GAT_NUMBER_ELEMENTS * sizeof(gati_t);
static constexpr uint32_t MSG_BLOCK_SIZE = 512;
static constexpr uint32_t GATSECLEN = GAT_SECTION_SIZE + GAT_NUMBER_ELEMENTS * MSG_BLOCK_SIZE;
static constexpr uint8_t STORAGE_TYPE = 2;
#define MSG_STARTING(section__) ((section__) * GATSECLEN + GAT_SECTION_SIZE)


class Type2Text {
public:
  explicit Type2Text(std::filesystem::path text_filename);

  std::vector<gati_t> load_gat(wwiv::core::File& file, size_t section);
  void save_gat(wwiv::core::File& f, size_t section, const std::vector<gati_t>& gat);
  bool readfile(const messagerec* msg, std::string* out);
  bool savefile(const std::string& text, messagerec* message_record);
  bool remove_link(messagerec& msg);

private:
  std::optional<wwiv::core::File> OpenMessageFile() const;
  const std::filesystem::path path_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_TYPE2_TEXT_H__
