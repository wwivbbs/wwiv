/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014 WWIV Software Services               */
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
#include "regcode.h"

#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "ifcns.h"
#include "input.h"
#include "init.h"
#include "wwivinit.h"

void edit_registration_code() {
  textattr(3);
  app->localIO->LocalCls();
  char regnum[15];
  sprintf(regnum, "%ld", syscfg.wwiv_reg_number);
  Printf("Registration Number  : %s\n", regnum);
  nlx(2);
  textattr(14);
  Printf("<ESC> when done.\n");

  bool done = false;
  int i1 = 0;
  int cp = 0;
  do {
    textattr(3);
    app->localIO->LocalGotoXY(23, cp);
    switch (cp) {
    case 0:
      editline(regnum, 7, NUM_ONLY, &i1, "");
      syscfg.wwiv_reg_number = atoi(regnum);
      sprintf(regnum, "%-7d", syscfg.wwiv_reg_number);
      Puts(regnum);
      break;
    }
    cp = GetNextSelectionPosition(0, 0, cp, i1);
    done = (i1 == DONE);
  } while (!done && !hangup);
  save_config();
}