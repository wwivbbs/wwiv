/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_MENUS_MENU_SET_H
#define INCLUDED_SDK_MENUS_MENU_SET_H

#include "core/stl.h"
#include "sdk/menus/menu.h"
#include <map>
#include <string>

namespace wwiv::sdk::menus {


class MenuDescriptions {
public:
  explicit MenuDescriptions(const std::filesystem::path& menupath);
  ~MenuDescriptions();
  [[nodiscard]] std::string description(const std::string& name) const;

private:
  const std::filesystem::path menupath_;
  std::map<std::string, std::string, wwiv::stl::ci_less> descriptions_;
};

class MenuSet56 final {
public:
  explicit MenuSet56(std::filesystem::path dir);
  MenuSet56& operator=(const MenuSet56&);
  /** Creates a dummy menuset */
  MenuSet56();
  ~MenuSet56();
  [[nodiscard]] bool Load();
  [[nodiscard]] bool Save();
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  void set_initialized(bool i) { initialized_ = i; }

  menu_set_t menu_set{};
  const std::filesystem::path& menuset_dir() const noexcept { return menuset_dir_;  }

private:
  std::filesystem::path menuset_dir_;
  bool initialized_{false};
};
}  // namespace

#endif
