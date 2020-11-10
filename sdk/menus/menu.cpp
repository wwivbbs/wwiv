/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "sdk/menus/menu.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/acs/expr.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/menus/menus_cereal.h"
#include <string>
#include <utility>
#include <vector>

using cereal::make_nvp;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk::menus {

Menu430::Menu430(std::filesystem::path menu_dir, std::string menu_set, std::string menu_name)
    : menu_dir_(std::move(menu_dir)), menu_set_(std::move(menu_set)),
      menu_name_(std::move(menu_name)) {
  initialized_ = Load();
}

bool Menu430::Load() {
  const auto menu_dir = FilePath(menu_dir_, menu_set_);
  const auto menu_name = StrCat(menu_name_, ".mnu");
  const auto path = FilePath(menu_dir, menu_name);
  File menu_file(path);
  if (!menu_file.Open(File::modeBinary | File::modeReadOnly, File::shareDenyNone)) {
    // Unable to open menu
    return false;
  }

  // Read the header (control) record into memory
  menu_file.Read(&header, sizeof(menu_header_430_t));

  const auto num_items = static_cast<int>(menu_file.length() / sizeof(menu_rec_430_t));

  for (auto nn = 1; nn < num_items; nn++) {
    menu_rec_430_t menu{};
    menu_file.Read(&menu, sizeof(menu_rec_430_t));
    recs.emplace_back(menu);
  }
  initialized_ = true;
  return true;
}

menu_action_56_t CreateActionFrom43Execute(const std::string& exec) {
  menu_action_56_t a{};
  if (!contains(exec, ' ')) {
    a.cmd = exec;
    return a;
  }
  const auto space = exec.find(' ');
  a.cmd = exec.substr(0, space);
  auto data = StringTrim(exec.substr(space + 1));
  if (data.size() > 2) {
    if (data.back() == ')') {
      data.pop_back();
    }
    if (data.front() == '(') {
      data = data.substr(1);
    }
  }
  a.data = data;
  return a;
}

Menu56::Menu56(std::filesystem::path menu_dir, std::string menu_set, std::string menu_name)
    : menu_dir_(std::move(menu_dir)), menu_set_(std::move(menu_set)),
      menu_name_(std::move(menu_name)) {
  initialized_ = Load();
}

bool Menu56::Load() {
  const auto dir = FilePath(menu_dir_, menu_set_);
  const auto name = StrCat(menu_name_, ".mnu.json");
  JsonFile f(FilePath(dir, name), "menu", menu, 1);
  return f.Load();

}

bool Menu56::Save() {
  const auto dir = FilePath(menu_dir_, menu_set_);
  const auto name = StrCat(menu_name_, ".mnu.json");
  JsonFile f(FilePath(dir, name), "menu", menu, 1);
  return f.Save();
}

std::optional<Menu56> Create56MenuFrom43(const Menu430& m4) {
  const auto dir = FilePath(m4.menu_dir_, m4.menu_set_);
  const auto name = StrCat(m4.menu_name_, ".mnu.json");
  const auto path = FilePath(dir, name);
  if (backup_file(path, 3)) {
    File::Remove(path);
  }
  Menu56 m5(m4.menu_dir_, m4.menu_set_, m4.menu_name_);

  auto& h = m5.menu;
  const auto& oh = m4.header;
  h.logging_action = oh.nLogging;
  h.help_type = oh.nForceHelp;

  h.color_title = oh.nTitleColor;
  h.color_item_key = oh.nItemTextHLColor;
  h.color_item_braces = oh.nItemBorderColor;
  h.color_item_text = oh.nItemTextColor;

  h.title = oh.szMenuTitle;
  {
    acs::AcsExpr ae;
    ae.min_dsl(oh.nMinDSL).dar_int(oh.uDAR).min_sl(oh.nMinSL).ar_int(oh.uAR);
    ae.sysop(oh.nSysop).cosysop(oh.nCoSysop);
    h.acs = ae.get();
  }
  h.password = oh.szPassWord;

  // Add the items now.
  for (const auto& o : m4.recs) {
    menu_item_56_t i{};
    i.item_key = o.szKey;
    i.item_text = o.szMenuText;
    i.help_text = o.szHelp;
    i.log_text = o.szSysopLog;
    i.instance_message = o.szInstanceMessage;

    {
      // TODO(rushfan): Add support for co-sysop, sysop, restrict into ACS.
      acs::AcsExpr ae;
      ae.min_sl(o.nMinSL).max_sl(o.iMaxSL).min_dsl(o.nMinDSL).max_dsl(o.iMaxDSL).ar_int(o.uAR).dar_int(o.uDAR);
      ae.sysop(o.nSysop).cosysop(o.nCoSysop);
      i.acs = ae.get();
    }

    i.password = o.szPassWord;
    i.actions.emplace_back(CreateActionFrom43Execute(o.szExecute));

    if (oh.szScript[0]) {
      h.enter_actions.emplace_back(CreateActionFrom43Execute(oh.szScript));
    }
    if (oh.szExitScript[0]) {
      h.exit_actions.emplace_back(CreateActionFrom43Execute(oh.szScript));
    }
    h.items.emplace_back(i);
  }

  h.num_action = oh.nums;


  m5.set_initialized(true);
  return {m5};
}

}
