/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
#include "bbs/exec.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/stuffin.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/log.h"
#include "fmt/format.h"

#include <cctype>
#include <process.h>
#include <string>


int exec_cmdline(wwiv::bbs::CommandLine& cmdline, int flags) {
  if (a()->sess().ok_modem_stuff() && a()->sess().using_modem()) {
    VLOG(1) <<"Closing remote IO";
    bout.remoteIO()->close(true);
  }

  const auto cmd = cmdline.cmdline();
  auto res = system(cmd.c_str());

  if (a()->sess().ok_modem_stuff() && a()->sess().using_modem()) {
    VLOG(1) << "Reopening comm (on createprocess error)";
    bout.remoteIO()->open();
  }
  if (res != 0) {
    LOG(ERROR) << "error on system: " << res << "; errno: " << errno;
  }
  return res;
}

