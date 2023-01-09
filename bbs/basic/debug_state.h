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

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace wwiv::bbs::basic {

struct DebugLocation {
  std::string module;
  int pos;
  int row;
  int col;
};

enum class RunningState { 
  // Running, don't interrupt until a breakpoint is hit
  RUNNING, 
  // Will take one step
  STEPPING, 
  // Stopped after a break or step
  STOPPED, 
  // Trying to HALT
  HALTING, 
  // Done halting
  HALTED, 
  // Who knows.
  UNKNOWN };

class DebugState {
public:
  void SetCallStackFrames(char** frames, int fc);
  std::vector<std::string> stack_frames();
  void SetLocation(int pos, int row, int col);
  DebugLocation location() const;
  void SetRunningState(RunningState state);
  RunningState running_state() const;
  void SetCurrentModuleName(const std::string& module);

  /// Sets the initial and current module name and text
  void SetInitialModule(const std::string& module, const std::string& text);
  void AddSourceModule(const std::string& module, const std::string& text);
  std::string current_module_name() const;
  std::optional<std::string> module_source(const std::string& module) const;

  std::string to_string() const;

private:
  mutable std::mutex mu_;
  std::vector<std::string> frames_;
  DebugLocation location_;
  RunningState running_state_{RunningState::UNKNOWN};
  std::map<std::string, std::string> source_map_;
  std::string current_module_;
  std::string initial_module_;
};



}

#endif
