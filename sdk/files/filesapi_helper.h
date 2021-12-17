/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_SDK_TEST_FILES_FILESAPI_HELPER_H__
#define __INCLUDED_SDK_TEST_FILES_FILESAPI_HELPER_H__

#include "core/log.h"
#include "core/strings.h"
#include "sdk/files/files.h"
#include "sdk/files/files_ext.h"

inline uploadsrec ul(const std::string& fn, const std::string& desc, uint32_t size, daten_t daten) {
  uploadsrec u{};
  wwiv::strings::to_char_array(u.filename, wwiv::sdk::files::align(fn));
  wwiv::strings::to_char_array(u.description, desc);
  u.numbytes = size;
  u.daten = daten;
  return u;
}

inline uploadsrec ul(const std::string& fn, const std::string& desc, uint32_t size) {
  const auto now = wwiv::core::DateTime::now();
  return ul(fn, desc, size, now.to_daten_t());
}

class FilesApiHelper {
public:
  explicit FilesApiHelper(wwiv::sdk::files::FileApi* api) : api_(api) {}
  ~FilesApiHelper() = default;

  std::unique_ptr<wwiv::sdk::files::FileArea> CreateAndPopulate(const std::string& name,
                                                                const std::initializer_list<wwiv::sdk::files::FileRecord> files) {

    if (api_->Exist(name)) {
      LOG(ERROR) << "Area '" << name << "' already exists";
      return {};
    }
    if(!api_->Create(name)) {
      LOG(ERROR) << "Failed to create area: '" << name << "'";
      return {};
    }
    auto area = api_->Open(name);
    if (!area) {
      LOG(ERROR) << "Failed to open area: '" << name << "'";
      return {};
    }

    for (const auto& f : files) {
      area->AddFile(f);
    }
    area->Save();
    return area;
  }

private:
  wwiv::sdk::files::FileApi* api_;
};

#endif  // __INCLUDED_SDK_TEST_FILES_FILESAPI_HELPER_H__
