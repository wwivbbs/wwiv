/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_ALLOW_H__
#define __INCLUDED_SDK_ALLOW_H__

#include <string>
#include <vector>

#include "sdk/config.h"
//#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace files {


struct allow_entry_t {
  char a[13];
};

class Allow {
public:
  explicit Allow(const wwiv::sdk::Config& config);
  virtual ~Allow();

  bool Add(const std::string& filename);
  bool Remove(const std::string& filename);
  bool Load();
  bool Save();
  bool IsAllowed(const std::string& filename);

  const std::vector<allow_entry_t>& allow_vector() const { return allow_; }
  std::size_t size() const { return allow_.size(); }
  void set_save_on_exit(bool save_on_exit) { save_on_exit_ = save_on_exit; }
  bool save_on_exit() const { return save_on_exit_;  }

private:
  const std::string data_directory_;
  bool loaded_{false};
  bool save_on_exit_{false};
  std::vector<allow_entry_t> allow_;
};

} // namespace files
} // namespace sdk
} // namespace wwiv

#endif  // __INCLUDED_SDK_ALLOW_H__
