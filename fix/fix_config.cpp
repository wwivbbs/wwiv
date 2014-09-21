/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-20014, WWIV Software Services            */
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

#include "fix_config.h"

#include <algorithm>
#include <iostream>

namespace wwiv {
namespace fix {

void FixConfiguration::ParseCommandLine(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    char* ss = argv[i];
    if (*ss == '/' || *ss == '-') {
      switch (toupper(ss[1])) {
      case 'Y':
        this->flag_yes = true;
        break;
      case 'D':
        commands_.emplace_back("dirs");
        break;
      case 'U':
        commands_.emplace_back("users");
        break;
      case 'X':
        flag_experimental = true;
        break;
      case '?':
        this->ShowHelp();
        break;
      }
    }
    else {
      std::cerr << "Unknown argument: '" << ss << "'" << std::endl;
    }
  }
}

void FixConfiguration::ShowHelp() {
  std::cerr << "Command Line Usage:\n\n"
    << "\t-Y\t= Force Yes to All Prompts\n"
    << "\t-D\t= Check Directories\n"
    << "\t-U\t= Check User Records\n"
    << "\t-X\t= Use Experimental features\n"
    << "\n";
  exit(0);
}

}  // namespace fix
}  // namespace wwiv
