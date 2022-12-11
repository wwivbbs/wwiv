/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2022, WWIV Software Services                  */
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

// WWIV5 LNet
#include "core/command_line.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/version.h"
#include "lnet/lnet.h"
#include "net_core/net_cmdline.h"

#include <iostream>
#include <string>

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk;

void ShowCommandLineHelp(const NetworkCommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl;
  exit(1);
}

int main(int argc, char** argv) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  auto at_exit = finally(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");

  const NetworkCommandLine net_cmdline(cmdline, 'l');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested() ||
      net_cmdline.cmdline().remaining().empty()) {
    ShowCommandLineHelp(net_cmdline);
    return 1;
  }
  try {
    auto semaphore =
        SemaphoreFile::try_acquire(net_cmdline.semaphore_path(), net_cmdline.semaphore_timeout());
    LNet lnet(net_cmdline);
    return lnet.Run();
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [lnet]: Unable to Acquire Network Semaphore: " << e.what();
  }
  return 1;
}