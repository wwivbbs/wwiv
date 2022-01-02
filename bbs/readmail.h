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
/**************************************************************************/
#ifndef INCLUDED_BBS_READMAIL_H
#define INCLUDED_BBS_READMAIL_H

#include "sdk/vardec.h"

#include <vector>

// USED IN READMAIL TO STORE EMAIL INFO
struct tmpmailrec {
  int16_t index; // index into email.dat

  uint16_t fromsys, // originating system
      fromuser;     // originating user

  daten_t daten; // date it was sent

  messagerec msg; // where to find it
};

void readmail(bool newmail_only);
int check_new_mail(int user_number);
// Also used in QWK code.
bool read_same_email(std::vector<tmpmailrec>& mloc, int mw, int rec, mailrec& m, bool del,
                     uint16_t stat);

#endif
