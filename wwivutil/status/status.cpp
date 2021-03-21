/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "wwivutil/status/status.h"

#include "sdk/status.h"
#include "core/command_line.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

static int show_qscan(const Config& config) {
  StatusMgr mgr(config.datadir(), [](int) {});
  const auto status = mgr.get_status();
  std::cout << "QScan Pointer : " << status->qscanptr() << std::endl;
  return 0;
}

static int set_qscan(const Config& config, uint32_t qscan) {
  StatusMgr mgr(config.datadir(), [](int) {});
  mgr.Run([=](Status& s) {
    s.qscanptr(qscan);
  });

  const auto status = mgr.get_status();
  std::cout << "QScan Pointer : " << status->qscanptr() << std::endl;
  return 0;
}

class StatusQScanCommand final : public UtilCommand {
public:
  StatusQScanCommand(): UtilCommand("qscan", "Sets or Gets the qscan high water mark.") {}
  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  get : Gets the qscptr information." << std::endl << std::endl;
    return ss.str();
  }
  int Execute() override {
    if (remaining().empty()) {
      std::cout << GetUsage() << GetHelp() << std::endl;
      return 2;
    }
    const auto set_or_get = ToStringLowerCase(remaining().front());

    if (set_or_get == "get") {
      return show_qscan(*this->config()->config());
    }
    if (set_or_get == "set") {
      if (remaining().size() < 2) {
        std::cout << "qscan set requires a value" << std::endl;
        return 2;
      }
      const auto s = stl::at(remaining(), 1);
      const auto v = to_number<uint32_t>(s);
      if (v <= 0) {
        std::cout << "invalid value: '" << s << "' (" << v << ")" << std::endl;
        return 1;
      }
      std::cout << "Setting qscanptr to : " << v << std::endl;
      set_qscan(*this->config()->config(), v);
    }
    return 1;
  }

  bool AddSubCommands() override {
    return true;
  }

};

class StatusDumpCommand final : public UtilCommand {
public:
  StatusDumpCommand(): UtilCommand("dump", "Displays status.dat.") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  dump : Displays status.dat." << std::endl << std::endl;
    return ss.str();
  }

  int Execute() override {
    StatusMgr mgr(config()->config()->datadir());
    const auto s = mgr.get_status();
    std::cout << "num_users:     " << s->num_users() << std::endl;
    std::cout << "caller_num:    " << s->caller_num() << std::endl;
    std::cout << "days_active:   " << s->days_active() << std::endl;
    std::cout << "qscanptr:      " << s->qscanptr() << std::endl;
    return 0;
  }

  bool AddSubCommands() override {
    return true;
  }

};

bool StatusCommand::AddSubCommands() {
  add(std::make_unique<StatusQScanCommand>());
  add(std::make_unique<StatusDumpCommand>());
  return true;
}


}  // namespace 
