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

#ifndef INCLUDED_SDK_FILES_MENUS_CEREAL_H
#define INCLUDED_SDK_FILES_MENUS_CEREAL_H

#include "core/cereal_utils.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/uuid_cereal.h"
#include "sdk/menus/menu.h"

namespace cereal {
using namespace wwiv::sdk::menus;

template <class Archive>
std::string save_minimal(Archive const&, const menu_numflag_t& t) {
  return to_enum_string<menu_numflag_t>(t, {"none", "subs", "dirs"});
}
template <class Archive>
void load_minimal(Archive const&, menu_numflag_t& t, const std::string& s) {
  t = from_enum_string<menu_numflag_t>(s, {"none", "subs", "dirs"});
}


template <class Archive>
std::string save_minimal(Archive const&, const menu_help_display_t& t) {
  return to_enum_string<menu_help_display_t>(t, {"always", "never", "on_entrance", "user_choice"});
}
template <class Archive>
void load_minimal(Archive const&, menu_help_display_t& t, const std::string& s) {
  t = from_enum_string<menu_help_display_t>(s, {"always", "never", "on_entrance", "user_choice"});
}


template <class Archive>
std::string save_minimal(Archive const&, const menu_logtype_t& t) {
  return to_enum_string<menu_logtype_t>(t, {"key", "command", "description", "none"});
}
template <class Archive>
void load_minimal(Archive const&, menu_logtype_t& t, const std::string& s) {
  t = from_enum_string<menu_logtype_t>(s, {"key", "command", "description", "none"});
}

}

namespace wwiv::sdk::menus {

template <class Archive>
void serialize (Archive& ar, menu_action_56_t& d) {
  SERIALIZE(d, cmd);
  SERIALIZE(d, data);
  SERIALIZE(d, acs);
}

template <class Archive>
void serialize(Archive & ar, menu_item_56_t& s) {
  SERIALIZE(s, item_key);
  SERIALIZE(s, item_text);
  SERIALIZE(s, visible);
  SERIALIZE(s, help_text);
  SERIALIZE(s, log_text);
  SERIALIZE(s, instance_message);
  SERIALIZE(s, acs);
  SERIALIZE(s, password);
  SERIALIZE(s, actions);
}


template <class Archive>
void serialize(Archive & ar, generated_menu_56_t& s) {
  SERIALIZE(s, num_cols);
  SERIALIZE(s, color_title);
  SERIALIZE(s, color_item_key);
  SERIALIZE(s, color_item_text);
  SERIALIZE(s, color_item_braces);
  SERIALIZE(s, show_empty_text);
  SERIALIZE(s, num_newlines_at_end);
}

template <class Archive>
void serialize(Archive & ar, menu_56_t& s) {
  SERIALIZE(s, cls);
  SERIALIZE(s, num_action);
  SERIALIZE(s, logging_action);
  SERIALIZE(s, help_type);
  SERIALIZE(s, generated_menu);
  SERIALIZE(s, title);
  SERIALIZE(s, acs);
  SERIALIZE(s, password);
  SERIALIZE(s, enter_actions);
  SERIALIZE(s, exit_actions);
  SERIALIZE(s, items);
}

template <class Archive>
void serialize (Archive& ar, menu_command_help_t& d) {
  SERIALIZE(d, cat);
  SERIALIZE(d, cmd);
  SERIALIZE(d, help);
}


template <class Archive> void serialize(Archive& ar, menu_set_t& m) {
  SERIALIZE(m, name);
  SERIALIZE(m, description);
  SERIALIZE(m, acs);
  SERIALIZE(m, items);
}

}


#endif
