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
#ifndef INCLUDED_BBS_BASIC_DEBUG_STATE_H
#define INCLUDED_BBS_BASIC_DEBUG_STATE_H

#include "bbs/basic/debug_model.h"
#include <nlohmann/json_fwd.hpp>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>


namespace wwiv::bbs::basic {

class Breakpoints {
public:
  void clear();
  std::optional<Breakpoint> add(const std::string& module, int line);
  void step(int callstack_depth);
  bool remove(int id);
  bool removeAllAtLine(const std::string& module, int line);
  std::vector<Breakpoint>& all() { return breakpoints_; }
  const std::vector<Breakpoint>& all() const { return breakpoints_; }
  std::vector<Breakpoint> visible() const;


private:
  std::vector<Breakpoint> breakpoints_;
  int id_{0};
};

enum class RunningState { 
  // Running, don't interrupt until a breakpoint is hit
  RUNNING, 
  // Will take one step
  STEPPING, 
  // Will take one step at the same stack frame
  STEPPING_OVER,
  // Stopped after a break or step
  STOPPED, 
  // Trying to HALT
  HALTING, 
  // Done halting
  HALTED, 
  // Who knows.
  UNKNOWN 
};

class DebugState {
public:
  void SetCallStackFrames(char** frames, int fc);
  std::vector<std::string> stack_frames() const;

  void SetLocation(int pos, int row, int col);
  DebugLocation location() const;

  Breakpoints& breakpoints();
  bool breakpoint_hit(int line);
  void SetRunningState(RunningState state);
  RunningState running_state() const;
  void SetCurrentModuleName(const std::string& module);

  /// Sets the initial and current module name and text
  void SetInitialModule(const std::string& module, const std::string& text);
  void AddSourceModule(const std::string& module, const std::string& text);
  std::string current_module_name() const;
  std::optional<std::string> module_source(const std::string& module) const;

  // Variables
  void clear_vars();
  void set_ast(void** a);
  void** ast();
  std::vector<Variable> vars() const;
  void add_var(const Variable& v);
  void set_frame(int f) { frame_ = f; }
  int frame() const { return frame_; }
  void set_num_stack_frames(int f) { num_stack_frames_ = f; }
  int num_stack_frames() const { return num_stack_frames_; }

  // Breakpoints

  std::string to_string() const;
  std::string to_json() const;

private:
  mutable std::mutex mu_;
  std::vector<std::string> frames_;
  std::string current_function_;
  DebugLocation location_;
  RunningState running_state_{RunningState::UNKNOWN};
  std::map<std::string, std::string> source_map_;
  std::string current_module_;
  std::string initial_module_;
  // Vars support
  void** ast_{ nullptr };
  std::vector<Variable> vars_;
  Breakpoints breakpoints_;
  int frame_{ 0 };
  int num_stack_frames_{ 0 };
};



}

#endif
