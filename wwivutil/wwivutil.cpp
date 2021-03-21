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
#include "core/command_line.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "instance/instance.h"
#include "sdk/config.h"
#include "wwivutil/help.h"
#include "wwivutil/acs/acs.h"
#include "wwivutil/conf/conf.h"
#include "wwivutil/config/config.h"
#include "wwivutil/email/email.h"
#include "wwivutil/fido/fido.h"
#include "wwivutil/files/files.h"
#include "wwivutil/fix/fix.h"
#include "wwivutil/messages/messages.h"
#include "wwivutil/menus/menus.h"
#include "wwivutil/net/net.h"
#include "wwivutil/users/users.h"
#include "wwivutil/print/print.h"
#include "wwivutil/status/status.h"
#include "wwivutil/subs/subs.h"
#include <algorithm>
#include <memory>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv::wwivutil {

class WWIVUtil final {
public:
  WWIVUtil() = delete;
  WWIVUtil(const WWIVUtil&) = delete;
  WWIVUtil(WWIVUtil&&) = delete;
  WWIVUtil& operator=(const WWIVUtil&) = delete;
  WWIVUtil& operator=(WWIVUtil&&) = delete;

  WWIVUtil(int argc, char *argv[]) : cmdline_(argc, argv, "net") {
    LoggerConfig config(LogDirFromConfig);
    Logger::Init(argc, argv, config);

    cmdline_.AddStandardArgs();
  }
  ~WWIVUtil() = default;

  int Main() {
    ScopeExit at_exit(Logger::ExitLogger);
    try {
      Add(std::make_unique<acs::AcsCommand>());
      Add(std::make_unique<ConfCommand>());
      Add(std::make_unique<ConfigCommand>());
      Add(std::make_unique<EmailCommand>());
      Add(std::make_unique<fido::FidoCommand>());
      Add(std::make_unique<files::FilesCommand>());
      Add(std::make_unique<FixCommand>());
      Add(std::make_unique<HelpCommand>());
      Add(std::make_unique<InstanceCommand>());
      Add(std::make_unique<MessagesCommand>());
      Add(std::make_unique<MenusCommand>());
      Add(std::make_unique<NetCommand>());
      Add(std::make_unique<PrintCommand>());
      Add(std::make_unique<StatusCommand>());
      Add(std::make_unique<SubsCommand>());
      Add(std::make_unique<UsersCommand>());
      if (!cmdline_.Parse()) {
        return 1;
      }
      auto config = load_any_config(cmdline_.bbsdir());
      if (!config) {
        LOG(ERROR) << "Unable to load config.json or config.dat";
        return 1;
      }
      command_config_ = std::make_shared<Configuration>(std::move(config));
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
  void Add(std::unique_ptr<UtilCommand>&& cmd) {
    auto* c = cmd.get();
    cmdline_.add(std::move(cmd));
    c->AddStandardArgs();
    c->AddSubCommands();
    subcommands_.push_back(c);
  }

  // ReSharper disable once CppMemberFunctionMayBeConst
  void SetConfigs() {
    for (auto* s : subcommands_) {
      s->set_config(command_config_);
    }
    command_config_->set_subcommands(subcommands_);
  }
  CommandLine cmdline_;
  std::shared_ptr<Configuration> command_config_;
  std::vector<UtilCommand*> subcommands_;
};

}  // namespace

int main(int argc, char *argv[]) {
  wwiv::wwivutil::WWIVUtil wwivutil(argc, argv);
  return wwivutil.Main();
}
