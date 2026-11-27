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
#include "httplib.h"

#include "bbs/basic/next/debug_server.h"

#include "core/log.h"
#include "core/strings.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <string>

namespace wwiv::bbs::basic {

using namespace std::chrono_literals;
using json = nlohmann::json;

namespace {

constexpr char kJson[] = "application/json";
constexpr char kText[] = "text/plain";
// How long stepover and tracein wait for the step to finish before
// answering with the state as it is.
constexpr auto kStepWait = 5s;

std::string StateName(DebugSession::State s) {
  switch (s) {
  case DebugSession::State::WAITING:
    return "waiting";
  case DebugSession::State::RUNNING:
    return "running";
  case DebugSession::State::PAUSED:
    return "paused";
  case DebugSession::State::FINISHED:
    return "finished";
  }
  return "unknown";
}

json BreakpointJson(const DebugSession::Breakpoint& b) {
  return json{{"id", b.id},     {"module", b.module},  {"typ", "line"},
              {"line", b.line}, {"function_name", ""}, {"hit_count", b.hit_count}};
}

void Error(httplib::Response& res, int status, const std::string& msg) {
  res.status = status;
  res.set_content(msg, kText);
}

} // namespace

DebugServer::DebugServer(DebugSession& session) : session_(session) {}

DebugServer::~DebugServer() {
  if (svr_) {
    svr_->stop();
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

std::string DebugServer::StateJson(const DebugSession& session) {
  const auto snap = session.snapshot();
  json vars = json::array();
  for (const auto& v : snap.vars) {
    vars.push_back({{"name", v.name}, {"type", v.type}, {"value", v.value}, {"frame", v.frame}});
  }
  json bps = json::array();
  for (const auto& b : session.breakpoints()) {
    bps.push_back(BreakpointJson(b));
  }
  json j;
  j["version"] = "1";
  j["state"] = StateName(session.state());
  j["initial_module"] = session.initial_module();
  j["location"] = {
      {"module", session.module()}, {"pos", 0}, {"row", session.line()}, {"col", session.column()}};
  j["stack"] = snap.stack;
  j["vars"] = vars;
  j["breakpoints"] = bps;
  return j.dump(2);
}

bool DebugServer::Start(const std::string& host, int port) {
  svr_ = std::make_unique<httplib::Server>();
  auto& s = *svr_;

  // Commands other than attach need an attached client.
  auto attached = [this](httplib::Response& res) {
    if (!session_.attached()) {
      Error(res, 400, "Not Attached");
      return false;
    }
    return true;
  };
  auto state = [this](httplib::Response& res) { res.set_content(StateJson(session_), kJson); };

  s.Get("/debug/status",
        [](const httplib::Request&, httplib::Response& res) { res.set_content("ok", kText); });
  s.Post("/debug/v1/attach", [this](const httplib::Request&, httplib::Response& res) {
    if (!session_.Attach()) {
      Error(res, 400, "Already attached");
      return;
    }
    const json j{{"attached", true}, {"initial_module", session_.initial_module()}};
    res.set_content(j.dump(2), kJson);
  });
  s.Post("/debug/v1/detach", [this](const httplib::Request&, httplib::Response& res) {
    if (!session_.Detach()) {
      Error(res, 400, "Not Attached");
      return;
    }
    res.set_content("detached", kText);
  });
  s.Post("/debug/v1/breakpoint",
         [this, attached](const httplib::Request& req, httplib::Response& res) {
           if (!attached(res)) {
             return;
           }
           if (req.has_param("typ") && req.get_param_value("typ") != "line") {
             Error(res, 400, "only line breakpoints are supported");
             return;
           }
           const auto line =
               req.has_param("line") ? strings::to_number<int>(req.get_param_value("line")) : 0;
           const auto module = req.has_param("module") ? req.get_param_value("module") : "";
           if (const auto b = session_.AddBreakpoint(module, line)) {
             res.set_content(BreakpointJson(*b).dump(2), kJson);
             return;
           }
           Error(res, 400, "missing or invalid line number");
         });
  s.Delete(R"(/debug/v1/breakpoint/(\d+))",
           [this, attached](const httplib::Request& req, httplib::Response& res) {
             if (!attached(res)) {
               return;
             }
             if (!session_.RemoveBreakpoint(strings::to_number<int>(req.matches[1].str()))) {
               Error(res, 404, "no such breakpoint");
               return;
             }
             res.set_content("breakpoint deleted", kText);
           });
  s.Post("/debug/v1/stepover",
         [this, attached, state](const httplib::Request&, httplib::Response& res) {
           if (attached(res)) {
             session_.WaitForPause(session_.StepOver(), kStepWait);
             state(res);
           }
         });
  s.Post("/debug/v1/tracein",
         [this, attached, state](const httplib::Request&, httplib::Response& res) {
           if (attached(res)) {
             session_.WaitForPause(session_.StepIn(), kStepWait);
             state(res);
           }
         });
  s.Post("/debug/v1/run", [this, attached, state](const httplib::Request&, httplib::Response& res) {
    if (attached(res)) {
      session_.Run();
      state(res);
    }
  });
  s.Post("/debug/v1/stop",
         [this, attached, state](const httplib::Request&, httplib::Response& res) {
           if (attached(res)) {
             state(res);
             session_.Halt();
           }
         });
  s.Get("/debug/v1/source", [this](const httplib::Request& req, httplib::Response& res) {
    const auto module = req.has_param("module") ? req.get_param_value("module") : "";
    if (const auto text = session_.source(module)) {
      res.set_content(*text, kText);
      return;
    }
    Error(res, 404, "no such module");
  });
  s.Get("/debug/v1/state",
        [state](const httplib::Request&, httplib::Response& res) { state(res); });
  s.Get("/debug/v1/watch", [this](const httplib::Request&, httplib::Response& res) {
    json vars = json::array();
    for (const auto& v : session_.snapshot().vars) {
      vars.push_back({{"name", v.name}, {"type", v.type}, {"value", v.value}, {"frame", v.frame}});
    }
    res.set_content(vars.dump(2), kJson);
  });
  s.set_logger([](const httplib::Request& req, const httplib::Response& res) {
    VLOG(1) << "debugger: " << req.method << " " << req.path << " -> " << res.status;
  });

  if (!s.bind_to_port(host, port)) {
    LOG(ERROR) << "Unable to start the script debugger on " << host << ":" << port;
    return false;
  }
  thread_ = std::thread([this] { svr_->listen_after_bind(); });
  return true;
}

} // namespace wwiv::bbs::basic
