/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                  Copyright (C)2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_INSTANCE_H
#define INCLUDED_SDK_INSTANCE_H

#include <filesystem>
#include <string>
#include "sdk/config.h"
#include "sdk/vardec.h"

namespace wwiv::sdk {

class Instance final {
public:
  using size_type = int;

  Instance() = delete;
  Instance(const Instance&) = delete;
  Instance(Instance&&) = delete;
  explicit Instance(const Config& config);
  ~Instance() = default;

  [[nodiscard]] bool IsInitialized() const { return initialized_; }

  size_type size();
  instancerec at(size_type pos);

  bool upsert(size_type pos, const instancerec& ir);

  explicit operator bool() const noexcept { return IsInitialized(); }


private:
  [[nodiscard]] const std::filesystem::path& fn_path() const;

  bool initialized_;
  std::string datadir_;
  const std::filesystem::path path_;
};

std::string instance_location(const instancerec& ir);

}


#endif 
