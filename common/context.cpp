/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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
#include "common/context.h"

#include "common/common_events.h"
#include "core/eventbus.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "local_io/local_io.h"
#include "sdk/config.h"
#include "sdk/menus/menu_set.h"
#include <chrono>
#include <filesystem>
#include <string>

namespace wwiv::common {

using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk::menus;
using namespace wwiv::strings;
using namespace std::chrono;

Dirs::Dirs(const std::filesystem::path& bbsdir)
    : Dirs(FilePath(bbsdir, "temp").string(), FilePath(bbsdir, "batch").string(),
           FilePath(bbsdir, "batch").string(), FilePath(bbsdir, "gfiles").string(),
           FilePath(bbsdir, "scratch").string()) {}

std::filesystem::path Dirs::current_menu_gfiles_directory() const noexcept {
  return FilePath(current_menu_directory_, "gfiles");
}

std::filesystem::path Dirs::current_menu_script_directory() const noexcept {
  return FilePath(current_menu_directory_, "scripts");
}

SessionContext::SessionContext(LocalIO* io)
    : SessionContext(io, File::current_directory()) {}

SessionContext::SessionContext(LocalIO* io, const std::filesystem::path& root_directory)
    : irt_{}, io_(io), dirs_(root_directory),
      current_menu_set_(std::make_unique<MenuSet56>()) {}

// This has to be here since ~MenuSet56 can be found here.
SessionContext::~SessionContext() = default;

void SessionContext::InitalizeContext(const wwiv::sdk::Config& c) {
  const auto qscan_length = c.qscn_len() / sizeof(uint32_t);
  qscan_ = std::make_unique<uint32_t[]>(qscan_length);
  qsc = qscan_.get();
  qsc_n = qsc + 1;
  qsc_q = qsc_n + (c.max_dirs() + 31) / 32;
  qsc_p = qsc_q + (c.max_subs() + 31) / 32;
  ResetQScanPointers(c);
}

void SessionContext::ResetQScanPointers(const wwiv::sdk::Config& c) {
  memset(qsc, 0, c.qscn_len());
  *qsc = 999;
  memset(qsc_n, 0xff, ((c.max_dirs() + 31) / 32) * 4);
  memset(qsc_q, 0xff, ((c.max_subs() + 31) / 32) * 4);
}

void SessionContext::reset() {
  SetCurrentReadMessageArea(-1);
  set_current_user_sub_conf_num(0);
  set_current_user_dir_conf_num(0);
  SetUserOnline(false);
  SetTimeOnlineLimited(false);

  chat_reason("");
  num_screen_lines(io_->GetDefaultScreenBottom() + 1);
  hangup(false);
  chatting(chatting_t::none);
  using_modem(false);
  clear_irt();
  outcom(false);
  incom(false);
  okmacro(true);
}

void SessionContext::reset_local_io(LocalIO* io) { 
  DCHECK_NE(io, nullptr);
  io_ = io; 
}

void SessionContext::irt(const std::string& irt) { 
  to_char_array(irt_, irt); 
}

void SessionContext::SetLogonTime() { system_logon_time_ = std::chrono::system_clock::now(); }

seconds SessionContext::duration_used_this_session() const {
  return duration_cast<seconds>(std::chrono::system_clock::now() - system_logon_time_);
}

int SessionContext::bps() const noexcept {
  const auto f = file_bps();
  return f > 0 ? f : system_bps();
}

const MenuSet56& SessionContext::current_menu_set() const noexcept {
  /* To listen to this use code like
  bus().add_handler<UpdateMenuSet>(
      [this](const UpdateMenuSet& u) {
        current_menu_set(u.name);
      });
   */
  DCHECK(current_menu_set_.get());
  return *current_menu_set_;
}

void SessionContext::load_menu_set(const std::filesystem::path& menusdir,
                                   const std::string& menuset) {
  if (current_menu_set_->menu_set.name != menuset) {
    current_menu_set_ = std::make_unique<MenuSet56>(FilePath(menusdir, menuset));
    bus().invoke<UpdateMenuSet>(UpdateMenuSet{menuset});
    dirs().current_menu_directory(current_menu_set_->menuset_dir());
  }
}

std::optional<sdk::value::Value> MapValueProvider::value(const std::string& name) const {
  if (const auto& iter = map_.find(name); iter != std::end(map_)) {
    return sdk::value::Value(iter->second);
  }
  return std::nullopt;
}

const std::map<std::string, std::string>& MapValueProvider::map() const {
  return map_;
}

bool Context::add_context_variable(const std::string& prefix, std::map<std::string, std::string> map) {
  value_providers_.emplace_back(std::make_unique<MapValueProvider>(prefix, std::move(map)));
  return true;
}

bool Context::remove_context_variable(const std::string& prefix) {
  for (auto it = std::begin(value_providers_); it != std::end(value_providers_); ++it) {
    if (prefix == it->get()->prefix()) {
      value_providers_.erase(it);
      return true;
    }
  }
  return false;
}

bool Context::clear_context_variables() {
  value_providers_.clear();
  return_values_.clear();
  return true;
}

}
