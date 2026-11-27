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
#ifndef INCLUDED_BBS_BASIC_NEXT_DEBUG_SESSION_H
#define INCLUDED_BBS_BASIC_NEXT_DEBUG_SESSION_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::bbs::basic {

// What a debugger client sees of a script and what it can do to it: the
// pause/step/breakpoint state shared between the script's thread
// (OnStatement) and the debug server's (everything else). Thread safe. The
// HTTP API over it is DebugServer (debug_server.h).
//
// Modules are source files named relative to the scripts directory
// ("hello.bas", "lib/util.bas"), compared without regard to case.
class DebugSession {
public:
  struct Variable {
    std::string name;
    std::string type;
    std::string value;
    // Index into the stack, innermost first.
    int frame{0};
  };

  struct Breakpoint {
    int id{0};
    std::string module;
    int line{0};
    int hit_count{0};
  };

  // What the script looks like while it's paused.
  struct Snapshot {
    // Innermost first.
    std::vector<std::string> stack;
    std::vector<Variable> vars;
  };

  enum class State {
    // Waiting for a debugger to attach before the script starts.
    WAITING,
    RUNNING,
    PAUSED,
    FINISHED
  };

  explicit DebugSession(std::string initial_module);

  // Client side (the debug server's thread).

  // Attaches a client; the script pauses at its next statement. False if a
  // client is already attached.
  bool Attach();
  // Detaches the client: breakpoints are removed and the script runs on
  // (unless Halt was called and the script hasn't stopped yet). False if no
  // client is attached.
  bool Detach();
  bool attached() const;

  std::optional<Breakpoint> AddBreakpoint(const std::string& module, int line);
  bool RemoveBreakpoint(int id);
  std::vector<Breakpoint> breakpoints() const;

  // Each returns a pause count to pass to WaitForPause, which waits until
  // the script pauses after the command (or finishes).
  // Runs until a breakpoint.
  uint64_t Run();
  // Stops at the next statement, inside a function the statement calls.
  uint64_t StepIn();
  // Stops at the next statement at the same depth or shallower, so calls
  // (and IMPORTed files' top-level code) run without stopping.
  uint64_t StepOver();
  // Ends the script at its next statement.
  void Halt();
  // True if the script paused (or finished) after `after` within timeout.
  bool WaitForPause(uint64_t after, std::chrono::milliseconds timeout);

  State state() const;
  std::string initial_module() const;
  // The current location's module and 1-based line and column (0 before the
  // first statement).
  std::string module() const;
  int line() const;
  int column() const;
  Snapshot snapshot() const;
  // A module's source, or nullopt if it hasn't been loaded. An empty module
  // means the current one.
  std::optional<std::string> source(const std::string& module) const;

  // Script side (the script's thread).

  // Waits until a client attaches, timeout passes or cancel returns true
  // (checked every 100ms). Returns whether a client attached.
  bool WaitForAttach(std::chrono::milliseconds timeout, const std::function<bool()>& cancel);
  void AddSource(const std::string& module, const std::string& text);
  bool has_source(const std::string& module) const;
  // Called before each statement. If the script should pause there (a
  // breakpoint, or after a step), takes a snapshot and waits until the client
  // continues, detaches or halts, or cancel returns true (checked every
  // 100ms, to notice a hangup). Returns true if the script should stop.
  //
  // Breakpoints fire when a line is entered: a second statement on the same
  // line at the same depth doesn't fire it again, but running the line again
  // (as a loop does) does.
  bool OnStatement(const std::string& module, int line, int column, int depth,
                   const std::function<Snapshot()>& take_snapshot,
                   const std::function<bool()>& cancel);
  // The script has ended.
  void Finished();
  // After Finished, so the client can see that the script finished: waits
  // until the client detaches, timeout passes or cancel returns true
  // (checked every 100ms). Returns at once if no client is attached.
  void WaitForDetach(std::chrono::milliseconds timeout, const std::function<bool()>& cancel);

private:
  enum class Mode { RUN, STEP_IN, STEP_OVER, PAUSE, HALT };

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::string initial_module_;
  bool attached_{false};
  bool finished_{false};
  Mode mode_{Mode::RUN};
  uint64_t pause_count_{0};
  int step_depth_{0};

  std::string module_;
  int line_{0};
  int column_{0};
  int depth_{0};
  // The previous statement, for "breakpoints fire when a line is entered".
  std::string prev_module_;
  int prev_line_{0};
  int prev_column_{0};
  int prev_depth_{-1};

  Snapshot snapshot_;
  std::vector<Breakpoint> breakpoints_;
  int next_breakpoint_id_{1};
  // Keyed by lower-cased module name.
  std::map<std::string, std::string> sources_;
};

} // namespace wwiv::bbs::basic

#endif
