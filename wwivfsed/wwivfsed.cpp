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
#include "wwivfsed/wwivfsed.h"

#include "common/bgetch.h"
#include "common/common_events.h"
#include "common/exceptions.h"
#include "common/full_screen.h"
#include "common/message_editor_data.h"
#include "common/null_remote_io.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "common/remote_socket_io.h"
#include "core/command_line.h"
#include "core/eventbus.h"
#include "fsed/commands.h"
#include "fsed/common.h"
#include "fsed/fsed.h"
#include "fsed/model.h"
#include "fsed/line.h"
#include "fsed/view.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/null_local_io.h"
#include "sdk/filenames.h"
#include <cstdlib>
#include <exception>
#include <string>

// isatty
#ifdef _WIN32
#include "local_io/local_io_win32.h"
#include <io.h>
#else
#include "local_io/local_io_curses.h"
#include "localui/curses_io.h"
#include <unistd.h>
#endif

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv::wwivfsed {

FsedApplication::FsedApplication(bool local, LocalIO* local_io, wwiv::common::RemoteIO* remote_io)
    : local_(local), context_(local_io), local_io_(local_io), remote_io_(remote_io),
      fake_macro_context_(&context_) {
  bout.SetLocalIO(local_io_.get());
  bout.SetComm(remote_io_.get());
  bout.set_context_provider([this]() -> wwiv::common::Context& { return this->context(); });
  bout.set_macro_context_provider(
      [this]() -> wwiv::common::MacroContext& { return fake_macro_context_; });

  bin.SetLocalIO(local_io_.get());
  bin.SetComm(remote_io_.get());
  bin.set_context_provider([this]() -> wwiv::common::Context& { return this->context(); });

  if (local_) {
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
  u.SetScreenChars(80);
  u.SetScreenLines(25);
  u.SetStatusFlag(User::ansi);
  u.SetStatusFlag(User::status_color);
  u.SetStatusFlag(User::extraColor);
  u.SetStatusFlag(User::fullScreenReader);

  const auto colors = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
  int i = 0;
  for (const auto c : colors) {
    u.SetColor(i++, c);
  }

  bus().add_handler<CheckForHangupEvent>([this]() { 
    if (local_) {
      return;
    }
    if (!context_.sess_.hangup() && !remote_io_->connected()) {
      bus().invoke<HangupEvent>();
    }
  });
  bus().add_handler<HangupEvent>([this]() { 
    if (context_.sess_.hangup()) {
      // already hungup
      return;
    }
    VLOG(1) << "Invoked Hangup()";
    context_.sess_.hangup(true);
    throw wwiv::common::hangup_error(""); // username
  });
}

FsedApplication::~FsedApplication() {
#ifndef _WIN32
  // CursesIO.
  delete curses_out;
  curses_out = nullptr;
#endif
  remote_io_->close(false);
}

int FsedApplication::Run(CommandLine& cmdline) { 
  auto remote_opened = remote_io_->open(); 
  if (!remote_opened) {
    // Remote side disconnected.
    LOG(ERROR) << "Remote side disconnected.";
    return 1;
  }

  auto remaining = cmdline.remaining();
  std::filesystem::path path = (remaining.empty()) ? "INPUT.MSG" : remaining.front();

  bool ok = wwiv::fsed::fsed(context(), path);
  return ok ? 0 : 1;
}


} // namespace wwiv::wwivfsed
static LocalIO* CreateLocalIO(bool local) {
  if (local && !isatty(fileno(stdin))) {
    LOG(ERROR) << "WTF - No isatty?";
  }
  if (local) {
#if defined(_WIN32)
    return new Win32ConsoleIO();
#else
    if (local) {
      CursesIO::Init(fmt::sprintf("WWIVfsed %s", full_version()));
      return new CursesLocalIO(curses_out->GetMaxY());
    }
#endif
  }
  return new NullLocalIO();
}

static RemoteIO* CreateRemoteIO(int handle, bool local) { 
  if (local) {
    return new NullRemoteIO();
  }
  return new RemoteSocketIO(handle, false); 
}

int main(int argc, char** argv) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument({"socket_handle", 'H', "Socket Handle from BBS."});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.add_argument(BooleanCommandLineArgument{"local", 'L', "Run the door locally.", false});
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
    const auto local = cmdline.barg("local");
    auto handle = cmdline.iarg("socket_handle");
    if (!local && handle == 0) {
      LOG(ERROR) << "Invalid Socket Handle Provided: " << handle;
      return EXIT_FAILURE;
    }
    auto app = std::make_unique<wwiv::wwivfsed::FsedApplication>(
      local, CreateLocalIO(local), CreateRemoteIO(handle, local));
    return app->Run(cmdline);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught top level exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
