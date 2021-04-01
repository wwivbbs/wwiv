/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "bbs/utility.h"
#include "common/context.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/file.h"
#include "core/strings.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

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

/**
 * \verbatim 
 * Creates a commandline by expanding the replaceable parameters offered by
 * WWIV.
 * 
 * If you add one, please update these docs:
 * - http://docs.wwivbbs.org/en/latest/chains/parameters
 * 
 * Replaceable parameters:
 * ~~~~~~~~~~~~~~~~~~~~~~
 *  Param    Description                       Example
 *  ----------------------------------------------------------------------------
 *  %%       A single '%'                          "%"
 *  %1-%5    Specified passed-in parameter
 *  %A       callinfo full pathname                "c:\wwiv\temp\callinfo.bbs"
 *  %B       Full path to BATCH instance directory "C:\wwiv\e\1\batch"
 *  %C       chain.txt full pathname               "c:\wwiv\temp\chain.txt"
 *  %D       doorinfo full pathname                "c:\wwiv\temp\dorinfo1.def"
 *  %E       door32.sys full pathname              "C:\wwiv\temp\door32.sys"
 *  %H       Socket Handle                         "1234"
 *  %I       Full Path to TEMP instance directory  "C:\wwiv\e\1\temp"
 *  %K       gfiles comment file for archives      "c:\wwiv\gfiles\comment.txt"
 *  %M       Modem BPS rate                         "38400"
 *  %N       Instance number                       "1"
 *  %O       pcboard full pathname                 "c:\wwiv\temp\pcboard.sys"
 *  %P       Com port number                       "1"
 *  %R       door full pathname                    "c:\wwiv\temp\door.sys"
 *  %S       Com port BPS rate                     "38400"
 *  %T       Time remaining (min)                  "30"
 *  %U       Username                              "Rushfan #1"
 *
 * \endverbatim 
*/
std::string stuff_in(const std::string& commandline, const std::string& arg1, const std::string& arg2,
                     const std::string& arg3, const std::string& arg4, const std::string& arg5) {
  std::vector<std::string> flags = {arg1, arg2, arg3, arg4, arg5};

  auto iter = begin(commandline);
  std::ostringstream os;
  while (iter != end(commandline)) {
    if (*iter == '%') {
      ++iter;
      const auto ch = to_upper_case<char>(*iter);
      switch (ch) {
      // fixed strings
      case '%':
        os << "%";
        break;
      // replaceable parameters
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
        os << flags.at(ch - '1');
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
      }
      ++iter;
    } else {
      os << *iter++;
    }
  }
  return os.str();
}
