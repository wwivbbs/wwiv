/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "bbs/execexternal.h"

#include "bbs/bbs.h"
#include "bbs/dropfile.h"
#include "bbs/exec.h"
#include "bbs/instmsg.h"
#include "bbs/wqscn.h"
#include "bbs/basic/basic.h"
#include "core/log.h"
#include "local_io/local_io.h"

using namespace wwiv::core;

static int ExecuteExternalProgramNoScript(const std::string& commandLine, int nFlags) {
  LOG(INFO) << "ExecuteExternalProgram: " << commandLine;
  // forget it if the user has hung up
  if (!(nFlags & EFLAG_NOHUP)) {
    if (a()->CheckForHangup()) {
      LOG(ERROR) << "Not executing program.  User Hung Up";
      return -1;
    }
  }
  [[maybe_unused]] auto _ = create_chain_file();

  // get ready to run it
  if (a()->sess().IsUserOnline()) {
    a()->WriteCurrentUser();
    write_qscn(a()->sess().user_num(), a()->sess().qsc, false);
  }

  // extra processing for net programs
  if (nFlags & EFLAG_NETPROG) {
    write_inst(INST_LOC_NET, a()->net_num() + 1, INST_FLAGS_NONE);
  }

  auto exec_dir = a()->bbspath();
  if (nFlags & EFLAG_TEMP_DIR) {
    exec_dir = a()->sess().dirs().temp_directory();
  } else if (nFlags & EFLAG_BATCH_DIR) {
    exec_dir = a()->sess().dirs().batch_directory();
  } else if (nFlags & EFLAG_QWK_DIR) {
    exec_dir = a()->sess().dirs().qwk_directory();
  }
  if (!(nFlags & EFLAG_NO_CHANGE_DIR)) {
    if (!File::set_current_directory(exec_dir)) {
      LOG(ERROR) << "Unable to set working directory to: " << exec_dir;
    }
  }

  // Some LocalIO implementations (Curses) needs to disable itself before
  // we fork some other process.
  bout.localIO()->DisableLocalIO();
  const auto return_code = exec_cmdline(commandLine, nFlags);

  // Re-engage the local IO engine if needed.
  bout.localIO()->ReenableLocalIO();
  File::set_current_directory(a()->bbspath());

  // Reread the user record.
  if (a()->sess().IsUserOnline()) {
    a()->ReadCurrentUser();
    read_qscn(a()->sess().user_num(), a()->sess().qsc, false, true);
    a()->UpdateTopScreen();
  }

  // return to caller
  return return_code;
}

int ExecuteExternalProgram(const std::string& command_line, int flags) {
  if (command_line.front() != '@') {
    return ExecuteExternalProgramNoScript(command_line, flags);
  }

  if (flags & EFLAG_NOSCRIPT) {
    LOG(WARNING) << "Not running '" << command_line << "' as a script since EFLAG_NOSCRIPT is specified.";
    return ExecuteExternalProgramNoScript(command_line, flags);
  }
 
  // Let's see if we need to run a basic script.
  const std::string BASIC_PREFIX = "@basic:";
  if (!wwiv::strings::starts_with(command_line, BASIC_PREFIX)) {
    return 1;
  }

  const auto cmd = command_line.substr(BASIC_PREFIX.size());
  LOG(INFO) << "Running basic script: " << cmd;
  return wwiv::bbs::basic::RunBasicScript(cmd) ? 0 : 1;
}