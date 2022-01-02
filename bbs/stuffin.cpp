/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/stuffin.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/dropfile.h"
#include "bbs/make_abs_cmd.h"
#include "bbs/utility.h"
#include "common/context.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/file.h"
#include "core/strings.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs {
/**
 * Gets the time left in minutes.
 */
static unsigned int GetTimeLeft() {
  auto d = nsl();
  if (d < 0) {
    d += SECONDS_PER_DAY;
  }
  return static_cast<unsigned int>(d / MINUTES_PER_HOUR);
}


std::string CommandLine::stuff_in() const {
  auto commandline = cmd_;
  auto iter = begin(commandline);
  std::ostringstream os;
  while (iter != end(commandline)) {
    if (*iter == '%') {
      ++iter;
      switch (const auto ch = to_upper_case<char>(*iter); ch) {
      // fixed strings
      case '%': // literal %
        os << "%";
        break;
      // call-specific numbers
      case 'A':
        os << create_dropfile_filename(drop_file_t::CALLINFO_BBS);
        break;
      case 'B':
        os << a()->sess().dirs().batch_directory().string();
        break;
      case 'C':
        os << create_dropfile_filename(drop_file_t::CHAIN_TXT);
        break;
      case 'D':
        os << create_dropfile_filename(drop_file_t::DORINFO_DEF);
        break;
      case 'E':
        os << create_dropfile_filename(drop_file_t::DOOR32_SYS);
        break;
      case 'H':
        os << bout.remoteIO()->GetDoorHandle();
        break;
      case 'I':
        os << a()->sess().dirs().temp_directory().string();
        break;
      case 'K':
        os << FilePath(a()->config()->gfilesdir(), COMMENT_TXT).string();
        break;
      case 'M':
        os << a()->modem_speed_;
        break;
      case 'N':
        os << a()->sess().instance_number();
        break;
      case 'O':
        os << create_dropfile_filename(drop_file_t::PCBOARD_SYS);
        break;
      case 'P':
        os << (a()->sess().incom() ? a()->primary_port() : 0);
        break;
      case 'R':
        os << create_dropfile_filename(drop_file_t::DOOR_SYS);
        break;
      // TODO(rushfan): Should we deprecate this? Use modem speed for now.
      case 'S':
        os << a()->modem_speed_;
        break;
      case 'T':
        os << GetTimeLeft();
        break;
      case 'U':
        os << a()->user()->name();
        break;
      // replacable params + everything else.
      default: {
        if (const auto it = args_.find(ch); it != std::end(args_)) {
          os << it->second;
        }
      } break;
      }
      ++iter;
    } else {
      os << *iter++;
    }
  }
  return os.str();
}

std::string CommandLine::cmdline() const { 
  auto cmd = stuff_in();
  if (cmdtype_ == cmdtype_t::absolute) {
    make_abs_cmd(bbs_root_, &cmd);
  }
  return cmd;
}

void CommandLine::add(char key, std::string value) {
  args_.emplace(to_upper_case_char(key), value);
}

}