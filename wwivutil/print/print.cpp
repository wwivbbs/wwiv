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
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/local_io_win32.h"
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

  if (!barg("ansi")) {
    std::cout << s << std::endl;
  } else {
    auto io = std::make_unique<Win32ConsoleIO>();
    io->Cls();
    LocalIOScreen screen(io.get(), 80);
    Ansi ansi(&screen, 0x07);
    for (const auto c : s) {
      ansi.write(c);
    }
  }
  return 0;
}

bool PrintCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument{"ansi", "Display the file as an ANSI file.", true});
  //    add_argument({"wwiv_version", "WWIV Version that created this config.dat", ""});
  return true;
}

} // namespace wwivutil
} // namespace wwiv
