/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2023, WWIV Software Services                  */
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
#include "httplib.h"

#include "basic.h"
#include "debugger.h"
#include "fmt/format.h"
#include <nlohmann/json.hpp>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace wwiv::bbs::basic {


bool Debugger::WaitForDebuggerToAttach() {
  int count = 0;
  while (true) {
    if (attached()) {
      return true;
    }
    wwiv::os::sleep_for(1s);
    if (++count % 5 == 0) {
    }
    if (count > 120) {
      return false;
    }
  }
}

std::string dump_headers(const httplib::Headers& headers) {
  std::string s;
  char buf[BUFSIZ];

  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const auto& x = *it;
    snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

static std::string log(const httplib::Request& req, const httplib::Response& res) {
  std::string s;

  s += "================================\n";
  s += fmt::format("{} {} {}", req.method, req.version, req.path);

  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    s += fmt::format("{:c}{}={}", (it == req.params.begin()) ? '?' : '&', it->first,
      it->second);
  }
  s += "\n";
  s += dump_headers(req.headers);
  s += "--------------------------------\n";

  s += fmt::format("{} {}\n", res.status, res.version);
  s += dump_headers(res.headers);
  s += "\n";

  if (!res.body.empty()) {
    s += res.body;
  }

  s += "\n";
  return s;
}

// Having this here in a c++ file means we can delete basic without knowing
// the complete types of httplib in basic.h
Debugger ::~Debugger() {
  // Stop the server then join the thread to wait for it to exit.
  if (svr_) {
    svr_->stop();
    srv_thread_.join();
  }
}

DebugState& Debugger::current_debug_state() {
  std::lock_guard lock(mu_);
  return debug_state_;
}

bool Debugger::attached() const {
  std::lock_guard lock(mu_);
  return attached_;
}

bool Debugger::StartServers(int port) {
  using namespace std::placeholders;
  svr_ = std::make_unique<httplib::Server>();

  svr_->Get("/debug/status", std::bind(&Debugger::status, this, _1, _2));

  // std::bind gives 'error C2668: 'httplib::Server::Post': ambiguous call to overloaded function'
  // here
  svr_->Post("/debug/v1/attach",
             [this](const httplib::Request& req, httplib::Response& res) { Attach(req, res); });
  svr_->Post("/debug/v1/detach",
             [this](const httplib::Request& req, httplib::Response& res) { Detach(req, res); });

  // These should be POST too
  svr_->Post("/debug/v1/stepover",
             [this](const httplib::Request& req, httplib::Response& res) { stepover(req, res); });
  svr_->Post("/debug/v1/tracein",
             [this](const httplib::Request& req, httplib::Response& res) { tracein(req, res); });
  svr_->Post("/debug/v1/run",
             [this](const httplib::Request& req, httplib::Response& res) { run(req, res); });
  svr_->Post("/debug/v1/stop",
             [this](const httplib::Request& req, httplib::Response& res) { stop(req, res); });

  svr_->Get("/debug/v1/source", std::bind(&Debugger::source, this, _1, _2));
  svr_->Get("/debug/v1/stack", std::bind(&Debugger::stack, this, _1, _2));
  svr_->Get("/debug/v1/state", std::bind(&Debugger::state, this, _1, _2));
  svr_->Get("/debug/v1/watch", std::bind(&Debugger::watch, this, _1, _2));
  svr_->Get("/debug/v1/breakpoints", std::bind(&Debugger::breakpoints, this, _1, _2));

  svr_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
    VLOG(1) << log(req, res);
  });
  srv_thread_ = std::thread(
      [&](int p) {
        svr_->listen("0.0.0.0", p);
        DetachImpl();
      },
      port);
  return true;
}


void Debugger::Attach(const httplib::Request&, httplib::Response& res) {
  if (attached()) {
    res.status = 400;
    res.set_content("Already attached", "text/plain");
    return;
  }
  res.set_content("attached", "text/plain");

  std::lock_guard lock(mu_);
  attached_ = true;
}

void Debugger::DetachImpl() {
  std::lock_guard lock(mu_);
  attached_ = false;
}

void Debugger::Detach(const httplib::Request&, httplib::Response& res) {
  if (!attached()) {
    res.status = 400;
    res.set_content("Not Attached", "text/plain");
    return;
  }
  res.set_content("detached", "text/plain");
  std::lock_guard lock(mu_);
  attached_ = false;
  // restore back to the running state.
  debug_state_.SetRunningState(RunningState::RUNNING);
}


void Debugger::state(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content(debug_state_.to_json(), "application/json");
}

void Debugger::stack(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  const auto frames = debug_state_.stack_frames();
  std::string result;
  result.reserve(frames.size() * 80);
  for (const auto& f : frames) {
    result.append(f);
  }
  res.set_content(result, "text/plain");
}

void Debugger::stepover(const httplib::Request&, httplib::Response& res) {
  {
    std::lock_guard lock(mu_);
    // realistically this should set a breakpoint on next line then run
    debug_state_.SetRunningState(RunningState::STEPPING);
  }

  // try to let the next step happenbefore we get the location.
  const auto last_step = step_count().load();
  wwiv::os::sleep_for(15ms);
  for (int i = 0; i < 200; i++) {
    const auto current_step = step_count().load();
    wwiv::os::sleep_for(10ms);
    if (current_step > last_step) {
      break;
    }
  }
  {
    res.set_content(debug_state_.to_json(), "text/plain");
  }
}

void Debugger::run(const httplib::Request&, httplib::Response&) {
  std::lock_guard lock(mu_);
  debug_state_.SetRunningState(RunningState::RUNNING);
}

void Debugger::stop(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  // Grab snapshot before stopping
  res.set_content(debug_state_.to_json(), "application/json");
  debug_state_.SetRunningState(RunningState::HALTING);
}

void Debugger::tracein(const httplib::Request&, httplib::Response& res) { 
  std::lock_guard lock(mu_);
  debug_state_.SetRunningState(RunningState::STEPPING);

  res.set_content(debug_state_.to_json(), "application/json");
}

void Debugger::source(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  const auto source = debug_state_.module_source(debug_state_.current_module_name()).value_or("");
  res.set_content(source, "text/plain");
}

void Debugger::status(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content("ok", "text/plain");
}

void Debugger::watch(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  nlohmann::json j = debug_state_.vars();
  res.set_content(j.dump(4), "application/json");
}

void Debugger::breakpoints(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content("none", "text/plain");
}

// Handlers for my_basic


static std::string to_string(struct mb_interpreter_t* bas, void** ast, mb_value_t v) {
  switch (v.type) {
  case MB_DT_INT:
    return std::to_string(v.value.integer);
  case MB_DT_REAL:
    return std::to_string(v.value.float_point);
  case MB_DT_STRING:
    if (v.value.string != nullptr) {
      return v.value.string;
    }
    return {};
    break;
  case MB_DT_LIST: {
    int count = 0;
    if (mb_count_coll(bas, ast, v, &count) != MB_FUNC_OK) {
      return {};
    }
    std::string result;
    for (auto i = 0; i < count; i++) {
      mb_value_t idx;
      mb_make_int(idx, i);
      mb_value_t val{};
      if (mb_get_coll(bas, ast, v, idx, &val) == MB_FUNC_OK) {
        if (!result.empty()) {
          result += ", ";
        }
        result.append(to_string(bas, ast, val));
      }
    }
    return fmt::format("[{}]", result);
  } break;
  default:
    return fmt::format("$$$ UNKNOWN VALUE FROM TYPE: {}$$", v.type);
  }
}

static void var_collector(struct mb_interpreter_t* bas, const char* n, mb_value_t v) {
  const auto* data = get_wwiv_script_userdata(bas);
  if (!data->basic->debugger_attached()) {
    return;
  }
  auto& debug_state = data->basic->debugger()->current_debug_state();
  Variable var;
  var.name = (n != nullptr) ? n : "";
  var.type = mb_get_type_string(v.type);
  var.frame = debug_state.frame();
  auto** ast = debug_state.ast();
  var.value = to_string(bas, ast, v);
  debug_state.add_var(var);
}

int _on_prev_stepped(struct mb_interpreter_t* bas, void** ast, const char* src, int pos,
  uint16_t row, uint16_t col) {

  const auto* data = get_wwiv_script_userdata(bas);
  if (!data->basic->debugger_attached()) {
    return MB_FUNC_OK;
  }
  auto& debug_state = data->basic->debugger()->current_debug_state();
  if (src) {
    debug_state.SetCurrentModuleName(src);
  }
  VLOG(1) << "pos: " << pos << "; source_file: " << ((src) ? src : "(null)") << "; row: " << row
    << "; col: " << col;

  // Update State
  auto num_stack_frames = mb_debug_count_stack_frames(bas);
  char* frames[16];
  memset(frames, 0, sizeof(frames));
  const auto fc = std::min<int>(num_stack_frames, 16);
  mb_debug_get_stack_trace(bas, frames, fc);
  debug_state.SetCallStackFrames(frames, fc);
  debug_state.SetLocation(pos, row, col);

  // Update variables
  debug_state.clear_vars();
  debug_state.set_ast(ast);
  for (int i = 0; i <= num_stack_frames; i++) {
    debug_state.set_frame(i);
    mb_get_vars(bas, ast, var_collector, i);
  }

  // Update step count last.
  data->basic->debugger()->step_count()++;

  // See what to do now
  // TODO: Move this into running state.
  for (;;) {
    switch (const auto s = debug_state.running_state()) {
    case RunningState::UNKNOWN:
      debug_state.SetRunningState(RunningState::STOPPED);
      break;
    case RunningState::HALTING:
      debug_state.SetRunningState(RunningState::HALTED);
      return MB_FUNC_ERR;
    case RunningState::HALTED:
      return MB_FUNC_ERR;
    case RunningState::RUNNING:
      return MB_FUNC_OK;
    case RunningState::STEPPING:
      debug_state.SetRunningState(RunningState::STOPPED);
      return MB_FUNC_OK;
    case RunningState::STOPPED:
      // Add way to bail
      if (data->in->bkbhit()) {
        if (data->in->bgetchraw() == 0x03) {
          data->out->pl("Exiting debugger...");
          return MB_FUNC_ERR;
        }
      }
      wwiv::os::sleep_for(10ms);
      break;
    default:
      break;
    }
  }
}

int _on_post_stepped(struct mb_interpreter_t* bas, void** ast, const char* source_file,
  int pos, uint16_t row, uint16_t col) {
  // here is where the logic of (a) was a breakpoint hit lives.
  mb_unrefvar(bas);
  mb_unrefvar(ast);
  mb_unrefvar(source_file);
  mb_unrefvar(pos);
  mb_unrefvar(row);
  mb_unrefvar(col);

  return MB_FUNC_OK;
}

} // namespace wwiv::bbs::basic
