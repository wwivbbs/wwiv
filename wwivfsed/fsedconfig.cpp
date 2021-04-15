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
#include "wwivfsed/fsedconfig.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "common/null_remote_io.h"
#include "common/remote_socket_io.h"
#include "core/strings.h"
#include "local_io/null_local_io.h"
// isatty
#ifdef _WIN32
#include "local_io/local_io_win32.h"
#include <io.h>
#else
// Needed on Linux to init curses with the title/
#include "core/version.h"
#include "local_io/local_io_curses.h"
#include "localui/curses_io.h"
using namespace wwiv::local::ui;
#include <unistd.h>
#endif

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;

namespace wwiv::wwivfsed {

FsedConfig::FsedConfig(const CommandLine& cmdline) 
  : root_(cmdline.program_path().parent_path()), help_path_(FilePath(root_, "gfiles")) {
  auto path = FilePath(root_, "wwivfsed.ini");
  IniFile ini(path, {"WWIVFSED"});
  if (ini.IsOpen()) {
    VLOG(1) << "Using wwivfsed.ini: '" << path.string() << "'";
    auto bt = ini.value<std::string>("bbs_type", "wwiv");
    bbs_type_ = strings::iequals(bt, "wwiv") ? bbs_type_t::wwiv : bbs_type_t::qbbs;
    std::filesystem::path p = ini.value<std::string>("help_path", "gfiles");
    if (p.is_absolute()) {
      help_path_ = p;
    } else {
      help_path_ = FilePath(root_, p);
    }
  }

  if (!File::Exists(help_path_)) {
    LOG(WARNING) << "Help Path does not exist '" << help_path_ << "'";
  }
  local_ = cmdline.barg("local");
  pause_ = cmdline.barg("pause");
  file_ = cmdline.barg("file");
  if (file_) {
    // Always local if we're editing a file
    local_ = true;
  }
  socket_handle_ = cmdline.iarg("socket_handle");
  if (!cmdline.remaining().empty()) {
    file_path_ = cmdline.remaining().front();
  }

}

LocalIO* FsedConfig::CreateLocalIO() const {
  if (local_ && !isatty(fileno(stdin))) {
    LOG(ERROR) << "WTF - No isatty?";
  }
  if (local_) {
#if defined(_WIN32)
    return new Win32ConsoleIO();
#else
    if (local_) {
      CursesIO::Init(fmt::sprintf("WWIVfsed %s", full_version()));
      return new CursesLocalIO(curses_out->GetMaxY(), curses_out->GetMaxX());
    }
#endif
  }
  return new NullLocalIO();
}

RemoteIO* FsedConfig::CreateRemoteIO(local::io::LocalIO* local_io) const {
  if (local_) {
    return new NullRemoteIO(local_io);
  }
  return new RemoteSocketIO(socket_handle_, false);
}

std::filesystem::path FsedConfig::help_path() const { return help_path_; }

std::filesystem::path FsedConfig::file_path() const {
  if (!file_path_.empty()) {
    return file_path_;
  }

  return bbs_type() == bbs_type_t::wwiv ? "input.msg" : "msgtmp";
}


}

