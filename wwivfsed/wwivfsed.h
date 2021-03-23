/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2021, WWIV Software Services            */
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
#ifndef INCLUDED_WWIVFSED_WWIVFSED_H
#define INCLUDED_WWIVFSED_WWIVFSED_H

#include "common/context.h"
#include "common/macro_context.h"
#include "common/message_editor_data.h"
#include "common/remote_io.h"
#include "local_io/local_io.h"
#include "sdk/chains.h"
#include "wwivfsed/fsedconfig.h"
#include <filesystem>
#include <string>

namespace wwiv::sdk {
class Chains;
}
namespace wwiv::wwivfsed {

class FakeMacroContext final : public common::MacroContext {
public:
  FakeMacroContext() = delete;
  FakeMacroContext(const FakeMacroContext&) = delete;
  FakeMacroContext(FakeMacroContext&&) = delete;
  FakeMacroContext& operator=(const FakeMacroContext&) = delete;
  FakeMacroContext& operator=(FakeMacroContext&&) = delete;
  explicit FakeMacroContext(common::Context* context) : MacroContext(context) {}
  ~FakeMacroContext() override = default;

  [[nodiscard]] std::string interpret_macro_char(char) const override { return ""; }  
  [[nodiscard]] common::Interpreted interpret_string(const std::string&) const override {
    return {};
  }
  [[nodiscard]] common::Interpreted evaluate_expression(const std::string&) const override {
    return {};
  }
  
};

class FsedContext final : public common::Context {
public:
  FsedContext() = delete;
  FsedContext(const FsedContext&) = delete;
  FsedContext(FsedContext&&) = delete;
  FsedContext& operator=(const FsedContext&) = delete;
  FsedContext& operator=(FsedContext&&) = delete;
  explicit FsedContext(local::io::LocalIO* local_io);
  ~FsedContext() override = default;
  [[nodiscard]] sdk::Config& config() override { return config_; }
  [[nodiscard]] sdk::User& u() override { return user_; }
  [[nodiscard]] common::SessionContext& session_context() override { return sess_; }
  [[nodiscard]] bool mci_enabled() const override { return false; }
  [[nodiscard]] const std::vector<editorrec>& editors() const override { return editors_; }
  [[nodiscard]] const sdk::Chains& chains() const override { return chains_; }

  sdk::User user_;
  common::SessionContext sess_;
  sdk::Config config_;
  std::vector<editorrec> editors_;
  sdk::Chains chains_;
};

class FsedApplication final {
public:
  FsedApplication() = delete;
  FsedApplication(const FsedApplication&) = delete;
  FsedApplication(FsedApplication&&) = delete;
  FsedApplication& operator=(const FsedApplication&) = delete;
  FsedApplication& operator=(FsedApplication&&) = delete;
  explicit FsedApplication(std::unique_ptr<FsedConfig> config);
  ~FsedApplication();
  int Run();


private:
  // Creates a MessageEditorData by reading the message editor
  // information files from the BBS.
  common::MessageEditorData CreateMessageEditorData();
  bool LoadQuotesWWIV(const wwiv::common::MessageEditorData& data);
  bool LoadQuotesQBBS(const wwiv::common::MessageEditorData& data);
  bool DoFsed();

  [[nodiscard]] common::Context& context() { return context_; }
  [[nodiscard]] const wwiv::common::Context& context() const { return context_; }
  [[nodiscard]] FsedConfig& config() const { return *config_; }

  const std::unique_ptr<FsedConfig> config_;
  std::unique_ptr<local::io::LocalIO> local_io_;
  std::unique_ptr<common::RemoteIO> remote_io_;
  FsedContext context_;
  FakeMacroContext fake_macro_context_;
};

}


#endif