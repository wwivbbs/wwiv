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
#include "wwivfsed/wwivfsed.h"

#include "common/common_events.h"
#include "common/exceptions.h"
#include "common/input.h"
#include "common/message_editor_data.h"
#include "common/output.h"
#include "common/quote.h"
#include "common/remote_io.h"
#include "core/command_line.h"
#include "core/eventbus.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fsed/common.h"
#include "fsed/fsed.h"
#include "fsed/model.h"
#include "local_io/local_io.h"
#include "sdk/chains.h"
#include "sdk/filenames.h"
#include "sdk/menus/menu_set.h"
#include "wwivfsed/fsedconfig.h"

#include <cstdlib>
#include <exception>
#include <string>

#ifndef _WIN32
#include "localui/curses_io.h"
using namespace wwiv::local::ui;
#endif


using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::fsed;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv::wwivfsed {

FsedContext::FsedContext(local::io::LocalIO* local_io)
    : sess_(local_io), config_("", {}), chains_(config_) {}
  

FsedApplication::FsedApplication(std::unique_ptr<FsedConfig> config)
    : config_(std::move(config)), 
      local_io_(config_->CreateLocalIO()), 
      remote_io_(config_->CreateRemoteIO(local_io_.get())), 
      context_(local_io_.get()),
      fake_macro_context_(&context_) {
  bout.SetLocalIO(local_io_.get());
  bout.SetComm(remote_io_.get());
  bout.set_context_provider([this]() -> wwiv::common::Context& { return this->context(); });
  bout.set_macro_context_provider(
      [this]() -> wwiv::common::MacroContext& { return fake_macro_context_; });

  bin.SetLocalIO(local_io_.get());
  bin.SetComm(remote_io_.get());
  bin.set_context_provider([this]() -> wwiv::common::Context& { return this->context(); });

  if (config_->local()) {
    context_.sess_.incom(false);
    context_.sess_.outcom(false);
    context_.sess_.using_modem(false);
  } else {
    context_.sess_.incom(true);
    context_.sess_.outcom(true);
    context_.sess_.using_modem(true);
    context_.sess_.ok_modem_stuff(true);
  }
  // Make a dummy user record.
  auto& u = context().u();
  u.screen_width(80);
  u.screen_lines(25);
  u.set_flag(User::flag_ansi);
  u.set_flag(User::status_color);
  u.set_flag(User::extraColor);
  u.set_flag(User::fullScreenReader);

  const auto colors = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
  auto i = 0;
  for (const auto c : colors) {
    u.color(i++, c);
  }

  bus().add_handler<CheckForHangupEvent>([this]() { 
    if (config_->local()) {
      return;
    }
    if (!context_.sess_.hangup() && !remote_io_->connected()) {
      bus().invoke<HangupEvent>(HangupEvent{hangup_type_t::user_disconnected});
    }
  });
  bus().add_handler<HangupEvent>([this]() { 
    if (context_.sess_.hangup()) {
      // already hung-up
      return;
    }
    VLOG(1) << "Invoked Hangup()";
    context_.sess_.hangup(true);
    throw hangup_error(hangup_type_t::user_logged_off, ""); // username
  });

  Dirs dirs(config_->root());
  dirs.gfiles_directory(config_->help_path().string());
  context_.sess_.dirs(dirs);
}

FsedApplication::~FsedApplication() {
#ifndef _WIN32
  // CursesIO.
  delete curses_out;
  curses_out = nullptr;
#endif
  remote_io_->close(false);
}

MessageEditorData FsedApplication::CreateMessageEditorData() {
  
  if (config().bbs_type() == FsedConfig::bbs_type_t::wwiv) {
    // WWIV: editor.inf
    TextFile f(FilePath(File::current_directory(), "editor.inf"), "rt");
    auto lines = f.ReadFileIntoVector();
    lines.resize(9);

    MessageEditorData data(lines.at(3));
    data.msged_flags = to_number<int>(lines.at(6));
    if (data.msged_flags & 2 /* MSGED_FLAG_HAS_REPLY_NAME */) {
      // TODO(rushfan): Figure out how to get to_name from text.
      //data.to_name = lines.at()
    }
    data.title = lines.at(0);
    data.sub_name = lines.at(1);
    LoadQuotesWWIV(data);
    return data;
  }
  // QuickBBS: MSGINF
  TextFile f(FilePath(File::current_directory(), "msginf"), "rt");
  auto lines = f.ReadFileIntoVector();
  lines.resize(6);
  MessageEditorData data(lines.at(0));
  data.to_name = lines.at(1);
  data.title = lines.at(2);
  data.sub_name = lines.at(4);

  LoadQuotesQBBS(data);
  return data;
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool FsedApplication::LoadQuotesWWIV(const MessageEditorData& data) {
  TextFile f(FilePath(File::current_directory(), QUOTES_TXT), "rt");
  if (!f) {
    return false;
  }
  auto orig_lines = f.ReadFileIntoVector();
  std::size_t min_size = 2;
  if (data.msged_flags & 2) {
    min_size++;
  }
  if (data.msged_flags & 4) {
    min_size++;
  }
  if (orig_lines.size() <= min_size) {
    return false;
  }
  std::vector<std::string> lines(std::begin(orig_lines) + min_size, std::end(orig_lines));

  set_quotes_ind(std::make_unique<std::vector<std::string>>(lines));
  return true;
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool FsedApplication::LoadQuotesQBBS(const MessageEditorData&) {
  TextFile f(FilePath(File::current_directory(), MSGTMP), "rt");
  if (!f) {
    return false;
  }
  auto lines = f.ReadFileIntoVector();
  if (lines.size() <= 1) {
    return false;
  }

  set_quotes_ind(std::make_unique<std::vector<std::string>>(lines));
  return true;
}

bool FsedApplication::DoFsed() {
  auto path = config_->file_path();

  // Create MessageEditorData and Load Quotes.
  auto data{CreateMessageEditorData()};

  // Data needs to_name, from_name, sub_name, title
  FsedModel ed(1000);
  if (auto file_lines = read_file(path, ed.maxli()); !file_lines.empty()) {
    ed.set_lines(std::move(file_lines));
  }
  if (config_->file()) {
    data.title = path.string();
  }
  // We don't want control-A,D,F to work natively so key bindings will.
  bin.okskey(false);
  if (auto save = fsed::fsed(context(), ed, data, config_->file()); !save) {
    return false;
  }

  TextFile f(path, "wt");
  if (!f) {
    return false;
  }
  for (const auto& l : ed.to_lines()) {
    f.WriteLine(l);
  }
  return true;
}


int FsedApplication::Run() { 
  if (config_->pause()) {
    local_io_->Puts("pause...");
    local_io_->GetChar();
  }

  if (!config_->local() && config_->socket_handle() == 0) {
    LOG(ERROR) << "Invalid Socket Handle Provided: " << config_->socket_handle();
  }

  if (!remote_io_->open()) {
    // Remote side disconnected.
    LOG(ERROR) << "Remote side disconnected.";
    return 1;
  }

  return DoFsed() ? 0 : 1;
}


} // namespace

int main(int argc, char** argv) {
  LoggerConfig logger_config(LogDirFromConfig);
  Logger::Init(argc, argv, logger_config);

  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument({"socket_handle", 'H', "Socket Handle from BBS."});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.add_argument(BooleanCommandLineArgument{"local", 'L', "Run the door locally.", false});
  cmdline.add_argument(BooleanCommandLineArgument{"file", 'F', "Run locally to edit a file.", false});
  cmdline.add_argument(BooleanCommandLineArgument{"pause", 'Z', "Pause to attach the debugger.", false});
  cmdline.set_no_args_allowed(true);

  if (!cmdline.Parse()) {
    std::cout << cmdline.GetHelp() << std::endl;
    return EXIT_FAILURE;
  }
  if (cmdline.help_requested()) {
    std::cout << cmdline.GetHelp() << std::endl;
    return EXIT_SUCCESS;
  }
  if (cmdline.barg("version")) {
    std::cout << full_version() << std::endl;
    return 0;
  }

  LOG(INFO) << "wwivfsed - WWIV5 Full-Screen Editor.";

  try {
    auto app = std::make_unique<wwiv::wwivfsed::FsedApplication>(
        std::make_unique<wwiv::wwivfsed::FsedConfig>(cmdline));
    return app->Run();
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught top level exception: " << e.what();
  }
  return EXIT_FAILURE;
}
