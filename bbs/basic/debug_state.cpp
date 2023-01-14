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

// Breakpoints
void Breakpoints::clear() {
  breakpoints_.clear();
}

std::optional<Breakpoint> Breakpoints::add(const std::string& module, int line) {
  Breakpoint b{};
  b.id = ++id_;
  b.module = module;
  b.line = line;
  breakpoints_.push_back(b);
  return { b };
}

void Breakpoints::step(int depth) {
  Breakpoint b{};
  b.id = ++id_;
  b.typ = Breakpoint::Type::step;
  b.auto_delete = false;
  b.visible = false;
  b.callstack_depth = depth;
  breakpoints_.emplace_back(b);
}


bool Breakpoints::remove(int line) {
  bool any_removed{ false };
  for (auto it = std::begin(breakpoints_); it != std::end(breakpoints_); ++it) {
    if (it->line == line) {
      it = breakpoints_.erase(it);
    }
  }
  return any_removed;
}

std::vector<Breakpoint> Breakpoints::visible() const {
  std::vector<Breakpoint> result;
  std::copy_if(breakpoints_.begin(), breakpoints_.end(), std::back_inserter(result), 
    [](const Breakpoint& b) {return b.visible; });
  return result;
}


bool DebugState::breakpoint_hit(int line) {
  auto& bs = breakpoints_.all();
  for (auto it = std::begin(bs); it != std::end(bs); ++it) {
    if (wwiv::strings::iequals(it->module, current_module_) && 
        it->line == line && 
        it->typ == Breakpoint::Type::line) {
      it->hit_count++;
      if (it->auto_delete) {
        breakpoints_.all().erase(it);
      }
      return true;
    } else if (it->typ == Breakpoint::Type::step && num_stack_frames_ <= it->callstack_depth) {
      // we're at the same level or lower, remove the breakpoint.
      breakpoints_.all().erase(it);
      return true;
    }
    // todo: implement function breakpoints here.
  }
  return false;
}

// DebugState

void DebugState::SetCallStackFrames(char** f, int fc) {
  std::lock_guard lock(mu_);
  frames_.clear();
  for (int i = 0; i < fc && f[i] != nullptr; i++) {
    frames_.emplace_back(f[i]);
  }
  if (!frames_.empty()) {
    // The root module has no stack frame.
    current_function_ = frames_.front();
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
  return location_;
}

Breakpoints& DebugState::breakpoints() {
  std::lock_guard lock(mu_);
  return breakpoints_;
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
  std::lock_guard lock(mu_);
  return fmt::format("{} {} {} {}\n", location_.module, location_.pos, location_.row, location_.col);
}

std::string DebugState::to_json() const {
  std::lock_guard lock(mu_);
  nlohmann::json j;
  j["version"] = "0.0";
  j["location"] = location_;
  j["breakpoints"] = breakpoints_.all();
  j["vars"] = vars_;
  j["stack"] = frames_;
  // Current module
  j["initial_module"] = initial_module_;
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

