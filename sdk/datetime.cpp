/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services             */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "sdk/datetime.h"

#include <cstring>
#include <ctime>
#include <string>

#include "core/strings.h"

using std::string;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

//
// This kludge will get us through 2019 and should not interfere anywhere
// else.
//

uint32_t date_to_daten(std::string datet) {
  if (datet.size() != 8) {
    return 0;
  }

  time_t t = time(nullptr);
  struct tm* pTm = localtime(&t);
  pTm->tm_mon   = atoi(datet.c_str()) - 1;
  pTm->tm_mday  = atoi(datet.c_str() + 3);
  // N.B. tm_year is years since 1900
  pTm->tm_year  = atoi(datet.c_str() + 6);         // fixed for 1920-2019
  if (datet[6] < '2') {
    pTm->tm_year += 100;
  }
  pTm->tm_hour = 0;
  pTm->tm_min = 0;
  pTm->tm_sec = 0;
  pTm->tm_isdst = 0;  // Since this is used for arbitrary compare of date strings, this is ok.

  return static_cast<uint32_t>(mktime(pTm));
}

std::string daten_to_mmddyy(time_t t) {
  struct tm* pTm = localtime(&t);
  return StringPrintf("%02d/%02d/%02d", 
    pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year % 100);
}

std::string daten_to_wwivnet_time(time_t t) {
  string human_date = string(asctime(localtime(&t)));
  StringTrimEnd(&human_date);
  return human_date;
}

uint32_t time_t_to_daten(time_t t) {
  return static_cast<uint32_t>(t);
}
}
}
