/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_MENUS_MENU_DATA_UTIL_H
#define INCLUDED_COMMON_MENUS_MENU_DATA_UTIL_H

#include <map>
#include <set>
#include <string>

namespace wwiv::common::menus {

class menu_data_and_options_t {
public:
  explicit menu_data_and_options_t(const std::string& raw);

  [[nodiscard]] std::set<std::string> opts(const std::string&) const;
  [[nodiscard]] const std::string& data() const;
  [[nodiscard]] auto size() const noexcept { return opts_.size(); }
  [[nodiscard]] bool opts_empty() const noexcept { return opts_.empty(); }
  [[nodiscard]] const std::multimap<std::string, std::string>& opts() const { return opts_; }
private:
  std::string data_;
  std::multimap<std::string, std::string> opts_;
};

}

#endif 