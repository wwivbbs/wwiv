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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "bbs/basic/next/debug_server.h"
#include "bbs/basic/next/debug_session.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace wwiv::bbs::basic;
using json = nlohmann::json;
using testing::HasSubstr;

namespace {

// Stands in for a script: runs statements (module, line, depth) through
// OnStatement on a thread of its own, recording each one that runs.
class FakeScript {
public:
  struct Stmt {
    std::string module;
    int line;
    int depth;
    int column{1};
  };

  FakeScript(DebugSession& s, std::vector<Stmt> stmts) : session_(s), stmts_(std::move(stmts)) {}
  ~FakeScript() { Join(); }

  void Start() {
    thread_ = std::thread([this] {
      for (const auto& st : stmts_) {
        if (session_.OnStatement(st.module, st.line, st.column, st.depth, snapshot, cancel)) {
          halted = true;
          break;
        }
        ran.fetch_add(1);
      }
      session_.Finished();
    });
  }
  void Join() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  std::atomic<int> ran{0};
  std::atomic<bool> halted{false};
  std::atomic<bool> cancelled{false};
  std::function<DebugSession::Snapshot()> snapshot = [] {
    return DebugSession::Snapshot{{"f (t.bas:2)", "t.bas:5"}, {{"x", "INTEGER", "1", 0}}};
  };
  std::function<bool()> cancel = [this] { return cancelled.load(); };

private:
  DebugSession& session_;
  std::vector<Stmt> stmts_;
  std::thread thread_;
};

} // namespace

TEST(DebugSessionTest, AttachPausesAtTheFirstStatement) {
  DebugSession s("t.bas");
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}});
  ASSERT_TRUE(s.Attach());
  EXPECT_FALSE(s.Attach());
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  EXPECT_EQ(DebugSession::State::PAUSED, s.state());
  EXPECT_EQ(1, s.line());
  EXPECT_EQ(0, script.ran.load());
  EXPECT_EQ("x", s.snapshot().vars.at(0).name);

  const auto n = s.StepIn();
  ASSERT_TRUE(s.WaitForPause(n, 5s));
  EXPECT_EQ(2, s.line());
  EXPECT_EQ(1, script.ran.load());

  s.Run();
  script.Join();
  EXPECT_EQ(2, script.ran.load());
  EXPECT_EQ(DebugSession::State::FINISHED, s.state());
}

TEST(DebugSessionTest, NotAttached_RunsWithoutStopping) {
  DebugSession s("t.bas");
  EXPECT_EQ(DebugSession::State::WAITING, s.state());
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}});
  script.Start();
  script.Join();
  EXPECT_EQ(2, script.ran.load());
}

TEST(DebugSessionTest, Breakpoint) {
  DebugSession s("t.bas");
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}, {"T.BAS", 3, 0}, {"t.bas", 4, 0}});
  ASSERT_TRUE(s.Attach());
  const auto b = s.AddBreakpoint("t.bas", 3);
  ASSERT_TRUE(b.has_value());
  // The same breakpoint again is the same one.
  EXPECT_EQ(b->id, s.AddBreakpoint("T.bas", 3)->id);
  EXPECT_FALSE(s.AddBreakpoint("t.bas", 0).has_value());
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  const auto n = s.Run();
  ASSERT_TRUE(s.WaitForPause(n, 5s));
  EXPECT_EQ(3, s.line());
  EXPECT_EQ(1, s.breakpoints().at(0).hit_count);
  EXPECT_TRUE(s.RemoveBreakpoint(b->id));
  EXPECT_FALSE(s.RemoveBreakpoint(b->id));
  s.Run();
  script.Join();
  EXPECT_EQ(4, script.ran.load());
}

TEST(DebugSessionTest, Breakpoint_FiresOncePerLine) {
  DebugSession s("t.bas");
  // Two statements on line 2 (IF x THEN y), then line 2 again from a loop.
  FakeScript script(s, {{"t.bas", 1, 0},
                        {"t.bas", 2, 0, 1},
                        {"t.bas", 2, 0, 9},
                        {"t.bas", 3, 0},
                        {"t.bas", 2, 0, 1}});
  ASSERT_TRUE(s.Attach());
  s.AddBreakpoint("t.bas", 2);
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  auto n = s.Run();
  ASSERT_TRUE(s.WaitForPause(n, 5s));
  EXPECT_EQ(1, script.ran.load());
  n = s.Run();
  ASSERT_TRUE(s.WaitForPause(n, 5s));
  EXPECT_EQ(2, s.line());
  EXPECT_EQ(4, script.ran.load());
  s.Run();
  script.Join();
  EXPECT_EQ(2, s.breakpoints().at(0).hit_count);
}

TEST(DebugSessionTest, Breakpoint_FiresEachTimeAroundALoop) {
  DebugSession s("t.bas");
  // WHILE on line 2 with a one-statement body on line 3: nothing else runs
  // between one pass's line 3 and the next's.
  FakeScript script(s, {{"t.bas", 1, 0},
                        {"t.bas", 2, 0},
                        {"t.bas", 3, 0, 3},
                        {"t.bas", 3, 0, 3},
                        {"t.bas", 3, 0, 3},
                        {"t.bas", 4, 0}});
  ASSERT_TRUE(s.Attach());
  s.AddBreakpoint("t.bas", 3);
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  for (int pass = 0; pass < 3; pass++) {
    const auto n = s.Run();
    ASSERT_TRUE(s.WaitForPause(n, 5s));
    EXPECT_EQ(3, s.line());
    EXPECT_EQ(2 + pass, script.ran.load());
  }
  s.Run();
  script.Join();
  EXPECT_EQ(6, script.ran.load());
  EXPECT_EQ(3, s.breakpoints().at(0).hit_count);
}

TEST(DebugSessionTest, StepOver_SkipsCalls) {
  DebugSession s("t.bas");
  FakeScript script(s, {{"t.bas", 5, 0}, {"t.bas", 2, 1}, {"t.bas", 3, 1}, {"t.bas", 6, 0}});
  ASSERT_TRUE(s.Attach());
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  const auto n = s.StepOver();
  ASSERT_TRUE(s.WaitForPause(n, 5s));
  EXPECT_EQ(6, s.line());
  EXPECT_EQ(3, script.ran.load());
  s.Run();
}

TEST(DebugSessionTest, StepOver_StopsAfterReturning) {
  DebugSession s("t.bas");
  // Stepping over the last statement of a function stops in its caller.
  FakeScript script(s, {{"t.bas", 2, 1}, {"t.bas", 6, 0}});
  ASSERT_TRUE(s.Attach());
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  const auto n = s.StepOver();
  ASSERT_TRUE(s.WaitForPause(n, 5s));
  EXPECT_EQ(6, s.line());
  s.Run();
}

TEST(DebugSessionTest, Halt) {
  DebugSession s("t.bas");
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}});
  ASSERT_TRUE(s.Attach());
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  s.Halt();
  script.Join();
  EXPECT_TRUE(script.halted.load());
  EXPECT_EQ(0, script.ran.load());
}

TEST(DebugSessionTest, CancelWhilePaused_Halts) {
  DebugSession s("t.bas");
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}});
  ASSERT_TRUE(s.Attach());
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  script.cancelled = true;
  script.Join();
  EXPECT_TRUE(script.halted.load());
}

TEST(DebugSessionTest, Detach_RunsOnWithoutBreakpoints) {
  DebugSession s("t.bas");
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}, {"t.bas", 3, 0}});
  ASSERT_TRUE(s.Attach());
  s.AddBreakpoint("t.bas", 3);
  script.Start();
  ASSERT_TRUE(s.WaitForPause(0, 5s));
  EXPECT_TRUE(s.Detach());
  EXPECT_FALSE(s.Detach());
  script.Join();
  EXPECT_EQ(3, script.ran.load());
  EXPECT_TRUE(s.breakpoints().empty());
}

TEST(DebugSessionTest, HaltThenDetach_StillHalts) {
  DebugSession s("t.bas");
  ASSERT_TRUE(s.Attach());
  // Stop arrives while the script is between statements, then the client
  // detaches before the next one.
  s.Halt();
  EXPECT_TRUE(s.Detach());
  FakeScript script(s, {{"t.bas", 1, 0}, {"t.bas", 2, 0}});
  script.Start();
  script.Join();
  EXPECT_TRUE(script.halted.load());
  EXPECT_EQ(0, script.ran.load());
}

TEST(DebugSessionTest, WaitForAttach) {
  DebugSession s("t.bas");
  EXPECT_FALSE(s.WaitForAttach(150ms, {}));
  EXPECT_FALSE(s.WaitForAttach(10s, [] { return true; }));
  std::thread t([&] { s.Attach(); });
  EXPECT_TRUE(s.WaitForAttach(10s, {}));
  t.join();
}

TEST(DebugSessionTest, WaitForDetach) {
  DebugSession s("t.bas");
  // Not attached: returns at once.
  s.WaitForDetach(10s, {});
  ASSERT_TRUE(s.Attach());
  s.Finished();
  s.WaitForDetach(150ms, {});
  s.WaitForDetach(10s, [] { return true; });
  std::thread t([&] { s.Detach(); });
  s.WaitForDetach(10s, {});
  t.join();
  EXPECT_FALSE(s.attached());
}

TEST(DebugSessionTest, ModuleNames_BackslashesAreSlashes) {
  DebugSession s("t.bas");
  s.AddSource("lib/u.bas", "x = 1\n");
  EXPECT_EQ("x = 1\n", s.source("lib\\u.bas").value_or("(none)"));
  EXPECT_EQ("LIB/u.bas", s.AddBreakpoint("LIB\\u.bas", 2)->module);
}

TEST(DebugSessionTest, Source) {
  DebugSession s("t.bas");
  s.AddSource("t.bas", "print 1\n");
  s.AddSource("lib/u.bas", "x = 1\n");
  EXPECT_TRUE(s.has_source("T.BAS"));
  // The current module, before the first statement, is the initial one.
  EXPECT_EQ("print 1\n", s.source("").value_or("(none)"));
  EXPECT_EQ("x = 1\n", s.source("LIB/U.bas").value_or("(none)"));
  EXPECT_FALSE(s.source("nosuch.bas").has_value());
}

// The whole thing: a script run by NextEngine with --debug, driven over HTTP
// as wwivdbg does.
class DebuggerEndToEndTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    std::filesystem::create_directories(helper.config().scriptdir());
    // A port that's free now.
    httplib::Server probe;
    port_ = probe.bind_to_any_port("127.0.0.1");
    probe.stop();
    ASSERT_GT(port_, 0);
    helper.context().session_context().debug_wwivbasic_port(port_);
  }

  void CreateScript(const std::string& name, const std::string& text) {
    TextFile f(wwiv::core::FilePath(helper.config().scriptdir(), name), "wt");
    ASSERT_TRUE(f.IsOpen());
    f.Write(text);
  }

  httplib::Result Post(httplib::Client& c, const std::string& what,
                       const httplib::Params& params = {}) {
    return c.Post("/debug/v1/" + what, params);
  }

  // Attaches, retrying until the server is up (it starts with the script).
  httplib::Result Attach(httplib::Client& c) {
    httplib::Result res;
    for (int i = 0; i < 100; i++) {
      res = Post(c, "attach");
      if (res) {
        break;
      }
      std::this_thread::sleep_for(50ms);
    }
    return res;
  }

  // Polls the state until the script is paused; returns the last state.
  json WaitPaused(httplib::Client& c) {
    json state;
    for (int i = 0; i < 100; i++) {
      state = json::parse(c.Get("/debug/v1/state")->body);
      if (state.at("state") == "paused") {
        break;
      }
      std::this_thread::sleep_for(50ms);
    }
    return state;
  }

  BbsHelper helper{};
  int port_{0};
};

TEST_F(DebuggerEndToEndTest, BreakpointStepAndVariables) {
  CreateScript("lib.bas", "def twice(n)\n  r = n * 2\n  return r\nenddef\n");
  CreateScript("dbg.bas", R"(import "lib.bas"
x = 1
y = twice(x)
print "y=" + str(y)
)");
  std::atomic<bool> ok{false};
  std::thread script([&] {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    ok = engine.RunScript("dbg.bas");
  });

  httplib::Client c("127.0.0.1", port_);
  c.set_read_timeout(10s);
  auto res = Attach(c);
  ASSERT_TRUE(res) << "the debugger didn't start";
  ASSERT_EQ(200, res->status) << res->body;
  EXPECT_EQ("dbg.bas", json::parse(res->body).at("initial_module").get<std::string>());

  // Attaching again is an error.
  EXPECT_EQ(400, Post(c, "attach")->status);

  // A breakpoint inside the imported function.
  res = Post(c, "breakpoint", {{"module", "lib.bas"}, {"typ", "line"}, {"line", "3"}});
  ASSERT_EQ(200, res->status) << res->body;
  const auto bp = json::parse(res->body);
  EXPECT_EQ(3, bp.at("line").get<int>());
  EXPECT_EQ("lib.bas", bp.at("module").get<std::string>());

  // Wait for the first pause, then run to the breakpoint.
  ASSERT_EQ("paused", WaitPaused(c).at("state"));
  ASSERT_EQ(200, Post(c, "run")->status);
  auto state = WaitPaused(c);
  ASSERT_EQ("paused", state.at("state")) << state.dump();
  EXPECT_EQ("lib.bas", state.at("location").at("module").get<std::string>());
  EXPECT_EQ(3, state.at("location").at("row").get<int>());
  EXPECT_EQ(std::vector<std::string>({"twice (lib.bas:3)", "dbg.bas:3"}),
            state.at("stack").get<std::vector<std::string>>());
  std::vector<std::string> vars;
  for (const auto& v : state.at("vars")) {
    vars.push_back(fmt::format("{}:{}={}", v.at("frame").get<int>(),
                               v.at("name").get<std::string>(), v.at("value").get<std::string>()));
  }
  EXPECT_EQ(std::vector<std::string>({"0:n=1", "0:r=2", "1:x=1"}), vars);
  EXPECT_EQ(1, state.at("breakpoints").at(0).at("hit_count").get<int>());

  auto src = c.Get("/debug/v1/source?module=lib.bas");
  ASSERT_EQ(200, src->status);
  EXPECT_THAT(src->body, HasSubstr("def twice(n)"));
  // Without a module, the source of the current one.
  EXPECT_THAT(c.Get("/debug/v1/source")->body, HasSubstr("def twice(n)"));

  // Step over the RETURN: back in the script, at the PRINT after the call.
  res = Post(c, "stepover");
  ASSERT_EQ(200, res->status);
  state = json::parse(res->body);
  EXPECT_EQ("dbg.bas", state.at("location").at("module").get<std::string>());
  EXPECT_EQ(4, state.at("location").at("row").get<int>());

  ASSERT_EQ(200,
            c.Delete("/debug/v1/breakpoint/" + std::to_string(bp.at("id").get<int>()))->status);
  ASSERT_EQ(200, Post(c, "detach")->status);
  script.join();
  EXPECT_TRUE(ok.load());
  EXPECT_THAT(helper.io()->captured(), HasSubstr("y=2"));
}

TEST_F(DebuggerEndToEndTest, ImportFromSubdirectory_ModuleIsRelativeToScriptdir) {
  std::filesystem::create_directories(helper.config().scriptdir() / "lib");
  CreateScript("lib/util.bas", "def f()\n  return 7\nenddef\n");
  CreateScript("sub.bas", "import \"./lib/../lib/util.bas\"\nprint str(f())\n");
  std::atomic<bool> ok{false};
  std::thread script([&] {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    ok = engine.RunScript("sub.bas");
  });
  httplib::Client c("127.0.0.1", port_);
  c.set_read_timeout(10s);
  ASSERT_TRUE(Attach(c));
  // As a Windows client might name it.
  auto res = Post(c, "breakpoint", {{"module", "lib\\util.bas"}, {"typ", "line"}, {"line", "2"}});
  ASSERT_EQ(200, res->status) << res->body;
  ASSERT_EQ("paused", WaitPaused(c).at("state"));
  ASSERT_EQ(200, Post(c, "run")->status);
  const auto state = WaitPaused(c);
  ASSERT_EQ("paused", state.at("state")) << state.dump();
  EXPECT_EQ("lib/util.bas", state.at("location").at("module").get<std::string>());
  EXPECT_EQ(2, state.at("location").at("row").get<int>());
  ASSERT_EQ(200, Post(c, "detach")->status);
  script.join();
  EXPECT_TRUE(ok.load());
}

TEST_F(DebuggerEndToEndTest, Stop) {
  CreateScript("stop.bas", "print \"one\"\nprint \"two\"\n");
  std::atomic<bool> ok{true};
  std::thread script([&] {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    ok = engine.RunScript("stop.bas");
  });
  httplib::Client c("127.0.0.1", port_);
  auto res = Attach(c);
  ASSERT_TRUE(res);
  ASSERT_EQ(1, WaitPaused(c).at("location").at("row").get<int>());
  // Paused at line 1: step once, then stop before line 2 runs.
  res = Post(c, "tracein");
  ASSERT_EQ(200, res->status);
  EXPECT_EQ(2, json::parse(res->body).at("location").at("row").get<int>());
  ASSERT_EQ(200, Post(c, "stop")->status);
  // The server stays up after the script ends, so the client can see that
  // it did, until the client detaches.
  json state;
  for (int i = 0; i < 100; i++) {
    const auto r = c.Get("/debug/v1/state");
    ASSERT_TRUE(r) << "the server went away before the client saw the script finish";
    state = json::parse(r->body);
    if (state.at("state") == "finished") {
      break;
    }
    std::this_thread::sleep_for(50ms);
  }
  EXPECT_EQ("finished", state.at("state"));
  ASSERT_EQ(200, Post(c, "detach")->status);
  script.join();
  EXPECT_FALSE(ok.load());
  const auto out = helper.io()->captured();
  EXPECT_THAT(out, HasSubstr("one"));
  EXPECT_THAT(out, testing::Not(HasSubstr("two")));
}

TEST_F(DebuggerEndToEndTest, KeysTypedWhileWaiting_AreTypedAhead) {
  CreateScript("keys.bas", "print \"ran\"\n");
  // Sent by the caller before any debugger attaches: Ctrl-C skips the wait,
  // and the keys around it are the script's (or the BBS's) to read.
  auto* remote = static_cast<TestRemoteIO*>(helper.io()->remote_io());
  helper.sess().incom(true);
  remote->keys = std::string("ab\x03" "cd");
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_TRUE(engine.RunScript("keys.bas"));
  const auto out = helper.io()->captured();
  EXPECT_THAT(out, HasSubstr("No debugger attached"));
  EXPECT_THAT(out, HasSubstr("ran"));
  std::string keys;
  while (bin.bkbhit() && keys.size() < 10) {
    keys.push_back(bin.bgetch());
  }
  // Ctrl-C was the debugger's; the rest come back in the order they were sent.
  EXPECT_EQ("abcd", keys);
}
