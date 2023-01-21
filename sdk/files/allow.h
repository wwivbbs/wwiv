/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_ALLOW_H
#define INCLUDED_SDK_ALLOW_H

#include <filesystem>
#include <string>
#include <vector>

#include "sdk/config.h"

namespace wwiv::sdk::files {


struct allow_entry_t {
  char a[13];
};

// Helpers for allow_entry_t.  Useful in tests.
bool operator==(const allow_entry_t& lhs, const allow_entry_t& rhs);
allow_entry_t to_allow_entry(const std::string& fn);

class Allow {
public:
  explicit Allow(const wwiv::sdk::Config& config);
  virtual ~Allow();

  bool Add(const std::string& filename);
  bool Remove(const std::string& filename);
  bool Load();
  bool Save();
  bool IsAllowed(const std::string& filename);

  [[nodiscard]] const std::vector<allow_entry_t>& allow_vector() const { return allow_; }
  [[nodiscard]] int size() const;
  void set_save_on_exit(bool save_on_exit) { save_on_exit_ = save_on_exit; }
  [[nodiscard]] bool save_on_exit() const { return save_on_exit_;  }

private:
  const std::filesystem::path data_directory_;
  bool loaded_{false};
  bool save_on_exit_{false};
  std::vector<allow_entry_t> allow_;
};

} // namespace 

#endif  // INCLUDED_SDK_ALLOW_H
