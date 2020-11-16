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
#ifndef INCLUDED_SDK_NAMES_H
#define INCLUDED_SDK_NAMES_H

#include "sdk/config.h"
#include "sdk/vardec.h"
#include <string>
#include <vector>

namespace wwiv::sdk {
class UserManager;

class Names final {
public:
  explicit Names(const wwiv::sdk::Config& config);
  ~Names();

  [[nodiscard]] std::string UserName(uint32_t user_number) const;
  [[nodiscard]] std::string UserName(uint32_t user_number, uint32_t system_number) const;
  bool Add(const std::string& name, uint32_t user_number);
  bool Remove(uint32_t user_number);
  bool Load();
  bool Save();
  bool Rebuild(const UserManager& um);
  [[nodiscard]] int FindUser(const std::string& search_string);

  [[nodiscard]] const std::vector<smalrec>& names_vector() const { return names_;  }
  [[nodiscard]] int size() const { return static_cast<int>(names_.size()); }
  void set_save_on_exit(bool save_on_exit) { save_on_exit_ = save_on_exit; }
  [[nodiscard]] bool save_on_exit() const { return save_on_exit_;  }

private:
  /*
   * Adds a new entry to the end vs. in the right spot.  This method
   * should only be used when adding many items, as Save will sort.
   */
  bool AddUnsorted(const std::string& name, uint32_t user_number);

  const std::string data_directory_;
  bool loaded_{false};
  bool save_on_exit_{false};
  std::vector<smalrec> names_;
};


}

#endif
