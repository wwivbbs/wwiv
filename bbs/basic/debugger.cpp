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

#include "debugger.h"
#include "fmt/format.h"
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
  char buf[BUFSIZ];

  s += "================================\n";

  snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(), req.version.c_str(), req.path.c_str());
  s += buf;

  std::string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto& x = *it;
    snprintf(buf, sizeof(buf), "%c%s=%s", (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
             x.second.c_str());
    query += buf;
  }
  snprintf(buf, sizeof(buf), "%s\n", query.c_str());
  s += buf;

  s += dump_headers(req.headers);

  s += "--------------------------------\n";

  snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
  s += buf;
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
    LOG(INFO) << log(req, res);
  });
  srv_thread_ = std::thread(
      [&](int p) {
        svr_->listen("0.0.0.0", p);
        DetachImpl();
      },
      port);
  return true;
}


void Debugger::Attach(const httplib::Request& req, httplib::Response& res) {
  if (attached()) {
    res.status = 400;
    res.set_content("Already attached", "text/plain");
    return;
  }
  auto msg = req.body;
  LOG(INFO) << "Debugger Message: " << msg;
  res.set_content("attached", "text/plain");

  std::lock_guard lock(mu_);
  attached_ = true;
}

void Debugger::DetachImpl() {
  std::lock_guard lock(mu_);
  attached_ = false;
}

void Debugger::Detach(const httplib::Request& req, httplib::Response& res) {
  if (!attached()) {
    res.status = 400;
    res.set_content("Not Attached", "text/plain");
    return;
  }
  auto msg = req.body;
  LOG(INFO) << "Debugger Message: " << msg;
  res.set_content("detached", "text/plain");
  DetachImpl();
}


std::string Debugger::state(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  auto loc = debug_state_.location();
  res.set_content(debug_state_.to_string(), "text/plain");
  return {};
}

std::string Debugger::stack(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  const auto frames = debug_state_.stack_frames();
  std::string result;
  result.reserve(frames.size() * 80);
  for (const auto& f : frames) {
    result.append(f);
  }
  res.set_content(result, "text/plain");
  return {};
}

std::string Debugger::stepover(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  // realistically this should set a breakpoint on next line then run
  debug_state_.SetRunningState(RunningState::STEPPING);
  auto loc = debug_state_.location();
  res.set_content(debug_state_.to_string(), "text/plain");
  return {};
}

std::string Debugger::run(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  // realistically this should set a breakpoint on next line then run
  debug_state_.SetRunningState(RunningState::RUNNING);
  auto loc = debug_state_.location();
  res.set_content(debug_state_.to_string(), "text/plain");
  return {};
}

std::string Debugger::stop(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  // realistically this should set a breakpoint on next line then run
  debug_state_.SetRunningState(RunningState::HALTING);
  auto loc = debug_state_.location();
  res.set_content(debug_state_.to_string(), "text/plain");
  return {};
}

std::string Debugger::tracein(const httplib::Request&, httplib::Response& res) { 
  std::lock_guard lock(mu_);
  debug_state_.SetRunningState(RunningState::STEPPING);

  auto loc = debug_state_.location();
  res.set_content(debug_state_.to_string(), "text/plain");
  return {};
}

std::string Debugger::source(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  const auto source = debug_state_.module_source(debug_state_.current_module_name()).value_or("");
  res.set_content(source, "text/plain");
  return {};
}

std::string Debugger::status(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content("ok", "text/plain");
  return {};
}

std::string Debugger::watch(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  std::string result = "name: \"foo\" value=\"value\"";
  res.set_content(result, "text/plain");
  return {};
}

std::string Debugger::breakpoints(const httplib::Request&, httplib::Response& res) {
  std::lock_guard lock(mu_);
  res.set_content("none", "text/plain");
  return {};
}

} // namespace wwiv::bbs::basic
