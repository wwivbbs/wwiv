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
#include "common/menus/menu_generator.h"

#include "common/value/uservalueprovider.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/acs/acs.h"
#include "sdk/menus/menu_set.h"

using namespace wwiv::sdk;
using namespace wwiv::sdk::menus;
using namespace wwiv::strings;

namespace wwiv::common::menus {

/**
 * Returns a string to display for a key.
 * Examples:
 *   'A' for a key 'A'
 *   '/A' for a key '/A'
 *   '//KEY' for a key 'KEY'
 */
static std::string display_key(const std::string& item_key) {
  if (item_key.size() == 1) {
    return item_key;
  }
  if (item_key.size() == 2 && item_key.front() == '/') {
    return item_key;
  }
  if (item_key.size() == 2) {
    return fmt::format("//{}", item_key);
  }
  if (starts_with(item_key, "//")) {
    return item_key;
  }
  return fmt::format("//{}", item_key);
}

static std::string generate_menu_item_line(generated_menu_56_t g, const std::string& key,
                                           const std::string& text, int max_width) {
  std::ostringstream ss;
  ss << g.color_item_braces << "(" << g.color_item_key << display_key(key) << g.color_item_braces
     << ")" << g.color_item_text;
  if (key.size() == 1 && !text.empty() &&
      to_upper_case_char(text.front()) == to_upper_case_char(key.front())) {
    ss << text.substr(1);
  } else {
    ss << " " << text;
  }
  ss << " ";
  const auto line = trim_to_size_ignore_colors(ss.str(), max_width);
  const auto len = size_without_colors(line);
  return fmt::format("{}{}", line, std::string(max_width - len, ' '));
}

static bool GenerateMenuLine(const Config& config, const menu_item_56_t& mi,
                             const generated_menu_56_t& g, bool& just_nled,
                             const std::vector<const wwiv::sdk::value::ValueProvider*>& providers,
                             menu_type_t typ, int num_cols, int screen_width,
                             std::ostringstream& ss) {
  if (mi.item_key.empty()) {
    return false;
  }
  if (auto [result, debug_lines] = acs::check_acs(config, mi.acs, providers); !result) {
    return false;
  }
  if (!g.show_empty_text && StringTrim(mi.item_text).empty()) {
    return false;
  }
  if (!mi.visible) {
    return false;
  }
  just_nled = false;
  const auto col_width =
      typ == menu_type_t::short_menu ? screen_width / num_cols : screen_width - 1;
  const auto key = display_key(mi.item_key);
  const auto& text = typ == menu_type_t::short_menu ? mi.item_text : mi.help_text;
  ss << generate_menu_item_line(g, key, text, col_width);
  return true;
}


std::vector<std::string> GenerateMenuLines(
                  const Config& config, const wwiv::sdk::menus::MenuSet56& menu_set,
                  const menu_56_t& menu, const sdk::User& user,
                  const std::vector<const wwiv::sdk::value::ValueProvider*>& providers,
                  menu_type_t typ) {
  std::vector<std::string> out;
  out.emplace_back("|#0");

  auto lines_displayed = 0;
  const auto& g = menu.generated_menu;
  const auto& title = menu.title;
  const auto num_cols = typ == menu_type_t::short_menu ? g.num_cols : 1;
  const auto screen_width = user.screen_width() - num_cols + 1;
  const auto col_width =
      typ == menu_type_t::short_menu ? screen_width / num_cols : screen_width - 1;
  if (!title.empty()) {
    const auto title_len = size_without_colors(title);
    const auto pad_len = (screen_width - title_len) / 2;
    out.emplace_back(fmt::format("{}{}{}|#0", std::string(pad_len, ' '), g.color_title, title));
  }
  std::ostringstream ss;
  if (menu.num_action != menu_numflag_t::none) {
    ss << generate_menu_item_line(g, "#", "Change Sub/Dir #", col_width);
    ++lines_displayed;
  }
  auto just_nled = false;
  for (const auto& mi : menu.items) {
    if (!GenerateMenuLine(config, mi, g, just_nled, providers, typ, num_cols, screen_width, ss)) {
      continue;
    }
    if (++lines_displayed % num_cols == 0) {
      out.emplace_back(ss.str());
      ss.str({});
      just_nled = true;
    }
  }

  std::set<std::string> keys;
  for (const auto& m : menu.items) {
    keys.insert(m.item_key);
  }
  for (const auto& mi : menu_set.menu_set.items) {
    if (wwiv::stl::contains(keys, mi.item_key)) {
      continue;
    }
    if (!GenerateMenuLine(config, mi, g, just_nled, providers, typ, num_cols, screen_width, ss)) {
      continue;
    }
    if (++lines_displayed % num_cols == 0) {
      out.emplace_back(ss.str());
      ss.str({});
      just_nled = true;
    }
  }

  if (!just_nled) {
    out.emplace_back(ss.str());
    ss.str({});
  }
  for (auto i = 0; i < g.num_newlines_at_end; i++) {
    out.emplace_back("");
  }
  return out;
}

} // namespace wwiv::common::menus
