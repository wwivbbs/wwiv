/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015,WWIV Software Services             */
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
#include "bbs/datetime.h"

namespace wwiv {
namespace sdk {

//
// This kludge will get us through 2019 and should not interfere anywhere
// else.
//

time_t date_to_daten(const char *datet) {
  if (strlen(datet) != 8) {
    return 0;
  }

  time_t t = time(nullptr);
  struct tm * pTm = localtime(&t);
  pTm->tm_mon   = atoi(datet) - 1;
  pTm->tm_mday  = atoi(datet + 3);
  // N.B. tm_year is years since 1900
  pTm->tm_year  = atoi(datet + 6);         // fixed for 1920-2019
  if (datet[6] < '2') {
    pTm->tm_year += 100;
  }
  pTm->tm_hour = 0;
  pTm->tm_min = 0;
  pTm->tm_sec = 0;
  pTm->tm_isdst = 0;  // Since this is used for arbitrary compare of date strings, this is ok.

  return mktime(pTm);
}

}
}
