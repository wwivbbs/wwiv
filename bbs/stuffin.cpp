/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <string>
#include <vector>

#include "core/strings.h"

#include "bbs/dropfile.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "sdk/filenames.h"

using std::string;
using std::vector;

const unsigned int GetTimeLeft();


// Replacable parameters:
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
//  %A       callinfo full pathname            "c:\wwiv\temp\callinfo.bbs"
//  %C       chain.txt full pathname           "c:\wwiv\temp\chain.txt"
//  %D       doorinfo full pathname            "c:\wwiv\temp\dorinfo1.def"
//  %E       door32.sys full pathname          "C:\wwiv\temp\door32.sys"
//  %H       Socket Handle                     "1234"
//  %K       gfiles comment file for archives  "c:\wwiv\gfiles\comment.txt"
//  %M       Modem baud rate                   "38400"
//  %N       Instance number                   "1"
//  %O       pcboard full pathname             "c:\wwiv\temp\pcboard.sys"
//  %P       Com port number                   "1"
//  %R       door full pathname                "c:\wwiv\temp\door.sys"
//  %S       Com port baud rate                "38400"
//  %T       Time remaining (min)              "30"
//

/**
 * @todo Document this
 */

const string stuff_in(const string& commandline, const string& arg1,
                      const string& arg2, const string& arg3,
                      const string& arg4, const string& arg5) {
  vector<string> flags;
  flags.push_back(arg1);
  flags.push_back(arg2);
  flags.push_back(arg3);
  flags.push_back(arg4);
  flags.push_back(arg5);

  auto iter = begin(commandline);
  std::ostringstream os;
  while (iter != end(commandline)) {
    if (*iter == '%') {
      ++iter;
      char ch = wwiv::UpperCase<char>(*iter);
      switch (ch) {
      // fixed strings
      case '%':
        os << "%";
        break;
      // replacable parameters
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
        os << flags.at(ch - '1');
        break;
      // call-specific numbers
      case 'A':
        os << create_filename(CHAINFILE_CALLINFO);
        break;
      case 'C':
        os << create_filename(CHAINFILE_CHAIN);
        break;
      case 'D':
        os << create_filename(CHAINFILE_DORINFO);
        break;
      case 'E':
        os << create_filename(CHAINFILE_DOOR32);
        break;
      case 'H':
        os << a()->remoteIO()->GetDoorHandle();
        break;
      case 'K':
        os << a()->config()->gfilesdir() << COMMENT_TXT;
        break;
      case 'M':
        os << modem_speed;
        break;
      case 'N':
        os << a()->instance_number();
        break;
      case 'O':
        os << create_filename(CHAINFILE_PCBOARD);
        break;
      case 'P':
        os << ((incom) ? a()->primary_port() : 0);
        break;
      case 'R':
        os << create_filename(CHAINFILE_DOOR);
        break;
      case 'S':
        os << com_speed;
        break;
      case 'T':
        os << GetTimeLeft();
        break;
      case 'U':
        os << a()->user()->GetName();
        break;
      }
      ++iter;
    } else {
      os << *iter++;
    }
  }
  return string(os.str());
}

const unsigned int GetTimeLeft() {
  auto d = nsl();
  if (d < 0) {
    d += SECONDS_PER_DAY;
  }
  return static_cast<unsigned int>(d / MINUTES_PER_HOUR);
}