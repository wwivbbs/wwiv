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
#include "core/textfile.h"
#include "core/strings.h"
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
  std::cout << s << std::endl;

  return 0;
}

bool PrintCommand::AddSubCommands() {
  //    add_argument({"wwiv_version", "WWIV Version that created this config.dat", ""});
//    add_argument({"revision", "Configuration revision number", ""});
  return true;
}

} // namespace wwivutil
} // namespace wwiv
