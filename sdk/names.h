/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
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
#ifndef __INCLUDED_SDK_NAMES_H__
#define __INCLUDED_SDK_NAMES_H__

#include <string>
#include <vector>

#include "sdk/config.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {


class Names {
public:
  explicit Names(wwiv::sdk::Config& config);
  virtual ~Names();

  std::string UserName(uint32_t user_number) const;
  std::string UserName(uint32_t user_number, uint32_t system_number) const;
  bool Add(const std::string name, uint32_t user_number);
  bool Remove(uint32_t user_number);
  bool Load();
  bool Save();
  int FindUser(const std::string& username);

  const std::vector<smalrec>& names_vector() const { return names_;  }
  std::size_t size() const { return names_.size(); }
  void set_save_on_exit(bool save_on_exit) { save_on_exit_ = save_on_exit; }
  bool save_on_exit() const { return save_on_exit_;  }

private:
  const std::string data_directory_;
  bool loaded_ = false;
  bool save_on_exit_ = false;
  std::vector<smalrec> names_;
};


}
}

#endif  // __INCLUDED_SDK_NAMES_H__
