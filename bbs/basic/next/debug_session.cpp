/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "bbs/basic/next/debug_session.h"

#include "core/strings.h"

#include <algorithm>
#include <utility>

namespace wwiv::bbs::basic {

using namespace std::chrono_literals;
using wwiv::strings::iequals;
using wwiv::strings::ToStringLowerCase;

DebugSession::DebugSession(std::string initial_module)
    : initial_module_(std::move(initial_module)) {}

bool DebugSession::Attach() {
  std::lock_guard lock(mu_);
  if (attached_) {
    return false;
  }
  attached_ = true;
  // Stop at the next statement, which is the first one if the script is
  // waiting for us.
  mode_ = Mode::STEP_IN;
  cv_.notify_all();
  return true;
}

bool DebugSession::Detach() {
  std::lock_guard lock(mu_);
  if (!attached_) {
    return false;
  }
  attached_ = false;
  breakpoints_.clear();
  mode_ = Mode::RUN;
  cv_.notify_all();
  return true;
}

bool DebugSession::attached() const {
  std::lock_guard lock(mu_);
  return attached_;
}

std::optional<DebugSession::Breakpoint> DebugSession::AddBreakpoint(const std::string& module,
                                                                    int line) {
  if (line <= 0) {
    return std::nullopt;
  }
  std::lock_guard lock(mu_);
  const auto m = module.empty() ? (module_.empty() ? initial_module_ : module_) : module;
  // Setting the same breakpoint twice returns the existing one.
  for (const auto& b : breakpoints_) {
    if (b.line == line && iequals(b.module, m)) {
      return b;
    }
  }
  Breakpoint b{next_breakpoint_id_++, m, line, 0};
  breakpoints_.push_back(b);
  return b;
}

bool DebugSession::RemoveBreakpoint(int id) {
  std::lock_guard lock(mu_);
  return std::erase_if(breakpoints_, [id](const Breakpoint& b) { return b.id == id; }) > 0;
}

std::vector<DebugSession::Breakpoint> DebugSession::breakpoints() const {
  std::lock_guard lock(mu_);
  return breakpoints_;
}

uint64_t DebugSession::Run() {
  std::lock_guard lock(mu_);
  if (mode_ != Mode::HALT) {
    mode_ = Mode::RUN;
  }
  cv_.notify_all();
  return pause_count_;
}

uint64_t DebugSession::StepIn() {
  std::lock_guard lock(mu_);
  if (mode_ != Mode::HALT) {
    mode_ = Mode::STEP_IN;
  }
  cv_.notify_all();
  return pause_count_;
}

uint64_t DebugSession::StepOver() {
  std::lock_guard lock(mu_);
  if (mode_ != Mode::HALT) {
    mode_ = Mode::STEP_OVER;
    step_depth_ = depth_;
  }
  cv_.notify_all();
  return pause_count_;
}

void DebugSession::Halt() {
  std::lock_guard lock(mu_);
  mode_ = Mode::HALT;
  cv_.notify_all();
}

bool DebugSession::WaitForPause(uint64_t after, std::chrono::milliseconds timeout) {
  std::unique_lock lock(mu_);
  return cv_.wait_for(lock, timeout,
                      [&] { return finished_ || (mode_ == Mode::PAUSE && pause_count_ > after); });
}

DebugSession::State DebugSession::state() const {
  std::lock_guard lock(mu_);
  if (finished_) {
    return State::FINISHED;
  }
  if (mode_ == Mode::PAUSE) {
    return State::PAUSED;
  }
  if (line_ == 0 && !attached_) {
    return State::WAITING;
  }
  return State::RUNNING;
}

std::string DebugSession::initial_module() const {
  std::lock_guard lock(mu_);
  return initial_module_;
}

std::string DebugSession::module() const {
  std::lock_guard lock(mu_);
  return module_.empty() ? initial_module_ : module_;
}

int DebugSession::line() const {
  std::lock_guard lock(mu_);
  return line_;
}

int DebugSession::column() const {
  std::lock_guard lock(mu_);
  return column_;
}

DebugSession::Snapshot DebugSession::snapshot() const {
  std::lock_guard lock(mu_);
  return snapshot_;
}

std::optional<std::string> DebugSession::source(const std::string& module) const {
  std::lock_guard lock(mu_);
  const auto& m = module.empty() ? (module_.empty() ? initial_module_ : module_) : module;
  if (const auto it = sources_.find(ToStringLowerCase(m)); it != sources_.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool DebugSession::WaitForAttach(std::chrono::milliseconds timeout,
                                 const std::function<bool()>& cancel) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::unique_lock lock(mu_);
  while (!attached_) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    cv_.wait_for(lock, 100ms);
    if (!attached_ && cancel) {
      lock.unlock();
      const auto stop = cancel();
      lock.lock();
      if (stop) {
        return false;
      }
    }
  }
  return true;
}

void DebugSession::AddSource(const std::string& module, const std::string& text) {
  std::lock_guard lock(mu_);
  sources_.insert_or_assign(ToStringLowerCase(module), text);
}

bool DebugSession::has_source(const std::string& module) const {
  std::lock_guard lock(mu_);
  return sources_.contains(ToStringLowerCase(module));
}

bool DebugSession::OnStatement(const std::string& module, int line, int column, int depth,
                               const std::function<Snapshot()>& take_snapshot,
                               const std::function<bool()>& cancel) {
  std::unique_lock lock(mu_);
  const auto new_line =
      prev_line_ != line || prev_depth_ != depth || !iequals(prev_module_, module);
  prev_module_ = module;
  prev_line_ = line;
  prev_depth_ = depth;
  module_ = module;
  line_ = line;
  column_ = column;
  depth_ = depth;

  if (mode_ == Mode::HALT) {
    return true;
  }
  if (!attached_) {
    return false;
  }

  auto pause = mode_ == Mode::STEP_IN || mode_ == Mode::PAUSE ||
               (mode_ == Mode::STEP_OVER && depth <= step_depth_);
  if (new_line) {
    for (auto& b : breakpoints_) {
      if (b.line == line && iequals(b.module, module)) {
        ++b.hit_count;
        pause = true;
      }
    }
  }
  if (!pause) {
    return false;
  }

  // Taken on the script's thread, since the interpreter isn't thread safe.
  lock.unlock();
  auto snap = take_snapshot ? take_snapshot() : Snapshot{};
  lock.lock();
  snapshot_ = std::move(snap);
  mode_ = Mode::PAUSE;
  ++pause_count_;
  cv_.notify_all();

  while (mode_ == Mode::PAUSE && attached_) {
    cv_.wait_for(lock, 100ms);
    if (mode_ == Mode::PAUSE && cancel) {
      lock.unlock();
      const auto stop = cancel();
      lock.lock();
      if (stop) {
        mode_ = Mode::HALT;
      }
    }
  }
  if (!attached_ && mode_ == Mode::PAUSE) {
    mode_ = Mode::RUN;
  }
  return mode_ == Mode::HALT;
}

void DebugSession::Finished() {
  std::lock_guard lock(mu_);
  finished_ = true;
  cv_.notify_all();
}

} // namespace wwiv::bbs::basic
