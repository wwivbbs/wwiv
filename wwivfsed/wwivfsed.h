/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#ifndef __INCLUDED_WWIVFSED_WWIVFSED_H__
#define __INCLUDED_WWIVFSED_WWIVFSED_H__

#include "common/context.h"
#include "common/macro_context.h"
#include "common/message_editor_data.h"
#include "common/remote_io.h"
#include "core/command_line.h"
#include "fsed/model.h"
#include "fsed/line.h"
#include "local_io/local_io.h"
#include <filesystem>
#include <vector>
#include <string>

namespace wwiv::wwivfsed {

class FakeMacroContext : public wwiv::common::MacroContext {
public:
  FakeMacroContext(wwiv::common::Context* context) : MacroContext(context) {}
  ~FakeMacroContext() = default;

  std::string interpret(char) const override { return ""; }
};

class FsedContext final : public wwiv::common::Context {
public:
  explicit FsedContext(LocalIO* local_io) : sess_(local_io) {}
  ~FsedContext() = default;
  wwiv::sdk::User& u() override { return user_; }
  wwiv::common::SessionContext& session_context() override { return sess_; }
  bool mci_enabled() const override { return false; };

  wwiv::sdk::User user_;
  wwiv::common::SessionContext sess_;
};

class FsedApplication final {
public:
  explicit FsedApplication(bool local, LocalIO* local_io, wwiv::common::RemoteIO* remote_io);
  ~FsedApplication();

  int Run(wwiv::core::CommandLine& cmdline);

  [[nodiscard]] wwiv::common::Context& context() { return context_; }
  [[nodiscard]] const wwiv::common::Context& context() const { return context_; }

private:
  FsedContext context_;
  std::unique_ptr<LocalIO> local_io_;
  std::unique_ptr<wwiv::common::RemoteIO> remote_io_;
  FakeMacroContext fake_macro_context_;
  bool local_{false};
};
//bool fsed(wwiv::common::Context&ctx, const std::filesystem::path& path);

}


#endif