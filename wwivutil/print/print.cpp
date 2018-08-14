/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include "wwivutil/print/print.h"

#include "core/command_line.h"
#include "core/log.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/local_io_curses.h"
#include "local_io/local_io_win32.h"
#include "localui/curses_io.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/localio_screen.h"
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::ansi;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

PrintCommand::PrintCommand() : UtilCommand("print", "prints a textfile") {}

std::string PrintCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage: " << std::endl << std::endl;
  ss << "  print : Prints a textfile." << std::endl;
  return ss.str();
}

int PrintCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << GetHelp() << endl;
    return 2;
  }
  TextFile tf(remaining().front(), "rt");
  auto s = tf.ReadFileIntoString();

  bool need_pause = false;
  if (!barg("ansi")) {
    std::cout << s << std::endl;
  } else {
    std::unique_ptr<LocalIO> io;
    auto io_type = sarg("io");
#ifdef _WIN32
    if (io_type == "win32") {
      io = std::make_unique<Win32ConsoleIO>();
    }
#endif
    if (!io) {
      need_pause = true;
      CursesIO::Init("wwivutil");
      // Use 25 lines (80x25) by default.
      // TODO(rushfan): We should try to inspect the screen size if possible
      // and use that instead.  Don't know how to do that on *NIX though.
      io = std::make_unique<CursesLocalIO>(25);
    }
    LocalIOScreen screen(io.get(), 80);
    AnsiCallbacks cb;
    cb.move_ = [](int x, int y) { VLOG(2) << "moved: x: " << x << "; y:" << y; };
    Ansi ansi(&screen, cb, 0x07);
    screen.clear();
    for (const auto c : s) {
      ansi.write(c);
    }
    if (need_pause) {
      io->GetChar();
    }
  }
  return 0;
}

bool PrintCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument{"ansi", "Display the file as an ANSI file.", true});
  add_argument({"io", "What type of IO to use, win32 | curses",
#ifdef _WIN32
                "win32"});
#else
                "curses"});
#endif
  return true;
}

} // namespace wwivutil
} // namespace wwiv
