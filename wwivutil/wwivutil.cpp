/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "wwivutil/config/config.h"
#include "wwivutil/fido/fido.h"
#include "wwivutil/files/files.h"
#include "wwivutil/fix/fix.h"
#include "wwivutil/messages/messages.h"
#include "wwivutil/net/net.h"
#include "wwivutil/print/print.h"
#include "wwivutil/status/status.h"

using std::map;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using wwiv::wwivutil::fido::FidoCommand;

namespace wwiv {
namespace wwivutil {

class WWIVUtil {
public:
  WWIVUtil(int argc, char *argv[]) : cmdline_(argc, argv, "net") {
    Logger::Init(argc, argv);
    cmdline_.AddStandardArgs();
  }
  ~WWIVUtil() {}

  int Main() {
    ScopeExit at_exit(Logger::ExitLogger);
    try {
      Add(std::make_unique<ConfigCommand>());
      Add(std::make_unique<FilesCommand>());
      Add(std::make_unique<MessagesCommand>());
      Add(std::make_unique<NetCommand>());
      Add(std::make_unique<FixCommand>());
      Add(std::make_unique<FidoCommand>());
      Add(std::make_unique<StatusCommand>());
      Add(std::make_unique<PrintCommand>());

      if (!cmdline_.Parse()) { return 1; }
      const std::string bbsdir(cmdline_.arg("bbsdir").as_string());
      Config config(bbsdir);
      if (!config.IsInitialized()) {
        LOG(ERROR) << "Unable to load CONFIG.DAT.";
        return 1;
      }
      command_config_.reset(new Configuration(bbsdir, &config));
      if (!command_config_->initialized()) {
        LOG(ERROR) << "Unable to load NETWORKS.";
        return 1;
      }
      SetConfigs();
      return cmdline_.Execute();
    } catch (const std::exception& e) {
      LOG(ERROR) << e.what();
    }
    return 1;
  }

private:
  void Add(std::unique_ptr<UtilCommand> cmd) {
    UtilCommand* c = cmd.get();
    cmdline_.add(std::move(cmd));
    c->AddStandardArgs();
    c->AddSubCommands();
    subcommands_.push_back(c);
  }

  void SetConfigs() {
    for (auto s : subcommands_) {
      s->set_config(command_config_.get());
    }
  }
  std::vector<UtilCommand*> subcommands_;
  CommandLine cmdline_;
  std::unique_ptr<Configuration> command_config_;
};

}  // namespace wwivutil
}  // namespace wwiv

int main(int argc, char *argv[]) {
  wwiv::wwivutil::WWIVUtil wwivutil(argc, argv);
  return wwivutil.Main();
}
