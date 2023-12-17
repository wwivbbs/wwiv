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
#include "core/strings.h"
#include "fmt/format.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace wwiv::bbs::basic {

static const char MIME_TYPE_JSON[] = "application/json";
static const char MIME_TYPE_TEXT[] = "text/plain";

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

  svr_->Delete(R"(/debug/v1/breakpoint/(\d+))",
    [this](const httplib::Request& req, httplib::Response& res) { DeleteBreakpoint(req, res); });
  svr_->Post("/debug/v1/breakpoint",
    [this](const httplib::Request& req, httplib::Response& res) { CreateBreakpoint(req, res); });
  svr_->Post("/debug/v1/stepover",
             [this](const httplib::Request& req, httplib::Response& res) { stepover(req, res); });
  svr_->Post("/debug/v1/tracein",
             [this](const httplib::Request& req, httplib::Response& res) { tracein(req, res); });
  svr_->Post("/debug/v1/run",
             [this](const httplib::Request& req, httplib::Response& res) { run(req, res); });
  svr_->Post("/debug/v1/stop",
             [this](const httplib::Request& req, httplib::Response& res) { stop(req, res); });

  svr_->Get("/debug/v1/source", std::bind(&Debugger::source, this, _1, _2));
  svr_->Get("/debug/v1/state", std::bind(&Debugger::state, this, _1, _2));
  svr_->Get("/debug/v1/watch", std::bind(&Debugger::watch, this, _1, _2));

  svr_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
    VLOG(1) << log(req, res);
  });
  srv_thread_ = std::thread(
      [&](int p) {
        svr_->listen("0.0.0.0", p);
        attached_ = false;
        session_id_.clear();
      },
      port);
  return true;
}

static std::string create_debug_session_id() {
  std::random_device rd;
  std::mt19937_64 e2(rd());
  std::uniform_int_distribution<long long int> dist(std::llround(std::pow(2, 61)), std::llround(std::pow(2, 62)));

  const auto result = dist(e2);
  return std::to_string(result);
}

void Debugger::Attach(const httplib::Request&, httplib::Response& res) {
  if (attached()) {
    res.status = 400;
    res.set_content("Already attached", MIME_TYPE_TEXT);
    return;
  }
  nlohmann::json j; 
  j["attached"] = true;
  session_id_ = create_debug_session_id();
  j["session_id"] = session_id_;
  j["initial_module"] = current_debug_state().current_module_name();
  res.set_content(j.dump(4), "application/json");

  std::lock_guard lock(mu_);
  attached_ = true;
}

void Debugger::Detach(const httplib::Request&, httplib::Response& res) {
  if (!attached()) {
    res.status = 400;
    res.set_content("Not Attached", MIME_TYPE_TEXT);
    return;
  }
  res.set_content("detached", MIME_TYPE_TEXT);
  std::lock_guard lock(mu_);
  attached_ = false;
  session_id_.clear();
  // restore back to the running state.
  debug_state_.SetRunningState(RunningState::RUNNING);
}

// Post to create a breakpoint
void Debugger::CreateBreakpoint(const httplib::Request& req, httplib::Response& res) {
  if (!attached()) {
    res.status = 400;
    res.set_content("Not Attached", MIME_TYPE_TEXT);
    return;
  }

  // res.set_content("detached", MIME_TYPE_TEXT);
  int line = -1;
  if (req.has_param("line")) {
    line = wwiv::strings::to_number<int>(req.get_param_value("line"));
    // TODO(rushfan): Need module here too.
  } else {
    res.status = 400;
    res.set_content("missing line number", MIME_TYPE_TEXT);
    return;
  }

  std::string module = this->debug_state_.current_module_name();
  if (req.has_param("module")) {
    module = req.get_param_value("module");
  }

  if (req.has_param("typ")) {
    auto typ = req.get_param_value("module");
    if (typ != "line") {
      res.status = 400;
      res.set_content("only line breakpoints are supported", MIME_TYPE_TEXT);
    }
  }

  std::lock_guard lock(mu_);
  if (auto ob = debug_state_.breakpoints().add(module, line)) {
    res.status = 200;
    nlohmann::json j;
    j = ob.value();
    res.set_content(j.dump(4), "application/json");
    return;
  }

  res.status = 400;
  res.set_content("Adding breakpoint failed", MIME_TYPE_TEXT);
}

void Debugger::DeleteBreakpoint(const httplib::Request& req, httplib::Response& res) {
  if (!attached()) {
    res.status = 400;
    res.set_content("Not Attached", MIME_TYPE_TEXT);
    return;
  }

  const auto id = wwiv::strings::to_number<int>(req.matches[1]);

  {
    std::lock_guard lock(mu_);
    debug_state_.breakpoints().remove(id);
  }

  res.status = 200;
  res.set_content("breakpoint deleted", MIME_TYPE_TEXT);
}
void Debugger::state(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content(debug_state_.to_json(), MIME_TYPE_JSON);
}

void Debugger::stepover(const httplib::Request&, httplib::Response& res) {

  {
    std::lock_guard lock(mu_);
    // realistically this should set a breakpoint on next line then run
    debug_state_.SetRunningState(RunningState::STEPPING_OVER);
  }

  // try to let the next step happen before we get the location.
  const auto last_step = step_count().load();
  wwiv::os::sleep_for(15ms);
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < 5) {
    const auto current_step = step_count().load();
    wwiv::os::sleep_for(10ms);
    if (current_step > last_step) {
      break;
    }
  }
  res.set_content(debug_state_.to_json(), MIME_TYPE_JSON);
}

void Debugger::run(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content(debug_state_.to_json(), MIME_TYPE_JSON);
  debug_state_.SetRunningState(RunningState::RUNNING);
}

void Debugger::stop(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  // Grab snapshot before stopping
  res.set_content(debug_state_.to_json(), MIME_TYPE_JSON);
  debug_state_.SetRunningState(RunningState::HALTING);
}

void Debugger::tracein(const httplib::Request&, httplib::Response& res) { 
  std::lock_guard lock(mu_);
  debug_state_.SetRunningState(RunningState::STEPPING);

  res.set_content(debug_state_.to_json(), MIME_TYPE_JSON);
}

void Debugger::source(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  const auto source = debug_state_.module_source(debug_state_.current_module_name()).value_or("");
  res.set_content(source, MIME_TYPE_TEXT);
}

void Debugger::status(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content("ok", MIME_TYPE_TEXT);
}

void Debugger::watch(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  nlohmann::json j = debug_state_.vars();
  res.set_content(j.dump(4), MIME_TYPE_JSON);
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
    return fmt::format("$$$ UNKNOWN VALUE FROM TYPE: {}$$", static_cast<int>(v.type));
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
  LOG(INFO) << "on_prev_stepped [enter]: row: " << row << "; pos: " << pos;

  const auto* data = get_wwiv_script_userdata(bas);
  if (!data->basic->debugger_attached()) {
    return MB_FUNC_OK;
  }
  auto& debug_state = data->basic->debugger()->current_debug_state();
  if (src) {
    debug_state.SetCurrentModuleName(src);
  }

  // Update State
  debug_state.set_num_stack_frames(mb_debug_count_stack_frames(bas));
  char* frames[16];
  memset(frames, 0, sizeof(frames));
  const auto mfc = std::min<int>(debug_state.num_stack_frames(), 16);
  mb_debug_get_stack_trace(bas, frames, mfc);
  debug_state.SetCallStackFrames(frames, mfc);
  debug_state.SetLocation(pos, row, col);

  // Update variables
  debug_state.clear_vars();
  debug_state.set_ast(ast);
  for (int i = 0; i <= debug_state.num_stack_frames(); i++) {
    debug_state.set_frame(i);
    mb_get_vars(bas, ast, var_collector, i);
  }
  LOG(INFO) << "on_prev_stepped [state updated]: row: " << row << "; pos: " << pos << "; fc: " << mfc;

  // See what to do now
  // TODO: Move this into running state.
  for (;;) {
    switch (const auto s = debug_state.running_state()) {
    case RunningState::UNKNOWN:
      debug_state.SetRunningState(RunningState::STOPPED);
      break;
    case RunningState::HALTING:
      debug_state.SetRunningState(RunningState::HALTED);
      return MB_FUNC_OK;
    case RunningState::HALTED:
      return MB_FUNC_BYE;
    case RunningState::RUNNING:
      // Check for breakpoints.
      if (debug_state.breakpoint_hit(row)) {
        data->basic->debugger()->step_count()++;
        LOG(INFO) << "on_prev_stepped [breakpoint hit]: row: " << row;
        debug_state.SetRunningState(RunningState::STOPPED);
        continue;
      }
      return MB_FUNC_OK;
    // Step In
    case RunningState::STEPPING:
      data->basic->debugger()->step_count()++;
      debug_state.SetRunningState(RunningState::STOPPED);
      return MB_FUNC_OK;
    case RunningState::STEPPING_OVER: {
      // Add a step breakpoint and run.
      const auto fc = debug_state.num_stack_frames();
      debug_state.breakpoints().step(fc);
      debug_state.SetRunningState(RunningState::RUNNING);
      LOG(INFO) << "Adding STEP breakpoint: row: " << row << "; fc: " << fc;
      return MB_FUNC_OK;
    } 
    case RunningState::STOPPED:
      // Add way to bail
      if (data->in->bkbhit()) {
        if (data->in->bgetchraw() == 0x03) {
          data->out->pl("Exiting debugger...");
          return MB_FUNC_BYE;
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
  mb_unrefvar(bas);
  mb_unrefvar(ast);
  mb_unrefvar(source_file);
  mb_unrefvar(pos);
  mb_unrefvar(row);
  mb_unrefvar(col);

  return MB_FUNC_OK;
}

} // namespace wwiv::bbs::basic
