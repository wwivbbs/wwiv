/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "wwivutil/wwivutil.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/log.h"
#include "core/file.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/stl.h"
#include "sdk/config.h"
#include "wwivutil/messages/messages.h"
#include "wwivutil/net/net.h"
#include "wwivutil/fix/fix.h"

using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::wwivutil;

int main(int argc, char *argv[]) {
  try {
    Logger::Init(argc, argv);
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.AddStandardArgs();

    UtilCommand* messages = new MessagesCommand();
    cmdline.add(messages);
    AddCommandsAndArgs(messages);

    UtilCommand* net = new NetCommand();
    cmdline.add(net);
    AddCommandsAndArgs(net);

    UtilCommand* fix = new FixCommand();
    cmdline.add(fix);
    AddCommandsAndArgs(fix);

    if (!cmdline.Parse()) { return 1; }
    const std::string bbsdir(cmdline.arg("bbsdir").as_string());
    Config config(bbsdir);
    if (!config.IsInitialized()) {
      LOG << "Unable to load CONFIG.DAT.";
      return 1;
    }
    std::unique_ptr<Configuration> command_config(
        new Configuration(bbsdir, &config));
    messages->set_config(command_config.get());
    net->set_config(command_config.get());
    fix->set_config(command_config.get());
    return cmdline.Execute();
  } catch (std::exception& e) {
    LOG << e.what();
  }
  return 1;
}
