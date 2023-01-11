/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/basic/debug_state.h"
#include "bbs/basic/util.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <mutex>
#include <vector>

namespace wwiv::bbs::basic {

void DebugState::SetCallStackFrames(char** f, int fc) {
  std::lock_guard lock(mu_);
  frames_.clear();
  for (int i = 0; i < fc && f[i] != nullptr; i++) {
    frames_.emplace_back(f[i]);
  }
}

std::vector<std::string> DebugState::stack_frames() const {
  std::lock_guard lock(mu_);
  return frames_;
}

void DebugState::SetLocation(int pos, int row, int col) {
  std::lock_guard lock(mu_);
  location_.module = current_module_;
  location_.pos = pos;
  location_.row = row;
  location_.col = col;
}

DebugLocation DebugState::location() const { 
  std::lock_guard lock(mu_);
  return this->location_;
}

void DebugState::SetRunningState(RunningState state) {
  std::lock_guard lock(mu_);
  running_state_ = state;
}

RunningState DebugState::running_state() const {
  std::lock_guard lock(mu_);
  return running_state_;
}

void DebugState::SetCurrentModuleName(const std::string& module) {
  std::lock_guard lock(mu_);
  current_module_ = module;
}

void DebugState::SetInitialModule(const std::string& module, const std::string& text) {
  std::lock_guard lock(mu_);
  initial_module_ = module;
  current_module_ = module;
  source_map_[module] = text;
}

void DebugState::AddSourceModule(const std::string& module, const std::string& text) {
  std::lock_guard lock(mu_);
  source_map_[module] = text;
}

std::string DebugState::current_module_name() const {
  std::lock_guard lock(mu_);
  return current_module_.empty() ? initial_module_ : current_module_;
}

std::optional<std::string> DebugState::module_source(const std::string& module) const {
  std::lock_guard lock(mu_);
  if (auto it = source_map_.find(module); it != source_map_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string DebugState::to_string() const {
  const auto loc = location();
  return fmt::format("{} {} {} {}\n", loc.module, loc.pos, loc.row, loc.col);
}

std::string DebugState::to_json() const {
  nlohmann::json j; // = debug_state_.vars();
  j["version"] = "0.0";
  j["location"] = location();
  j["vars"] = vars_;
  j["stack"] = stack_frames();
  return j.dump(4);
}


void DebugState::clear_vars() {
  std::lock_guard lock(mu_);
  vars_.clear();
}

void DebugState::set_ast(void** a) {
  std::lock_guard lock(mu_);
  ast_ = a;
}

void** DebugState::ast() {
  std::lock_guard lock(mu_);
  return ast_;
}

std::vector<Variable> DebugState::vars() const {
  std::lock_guard lock(mu_);
  return vars_;
}

void DebugState::add_var(const Variable& v) {
  std::lock_guard lock(mu_);
  vars_.push_back(v);
}


}

