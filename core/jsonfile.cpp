/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2021, WWIV Software Services            */
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
#include "core/jsonfile.h"

#include "deps/cereal/include/cereal/external/rapidjson/document.h"

namespace wwiv::core {

std::optional<std::string> read_json_file(const std::filesystem::path& p) {
  TextFile file(p, "r");
  if (!file.IsOpen()) {
    return std::nullopt;
  }
  const auto text = file.ReadFileIntoString();
  if (text.empty()) {
    return std::nullopt;
  }

  return { text };
}

int json_file_version(const std::filesystem::path& p) {
  if (auto o = read_json_file(p)) {
    rapidjson::Document doc;
    doc.Parse(o.value().c_str());

    if (!doc.IsObject()) {
      LOG(WARNING) << "document is not an object: " << p.string();
      return 0;
    }
  }
  return 0;
}

} // namespace
