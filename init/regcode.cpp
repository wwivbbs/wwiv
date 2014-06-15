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
#include <curses.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "ifcns.h"
#include "input.h"
#include "init.h"
#include "wwivinit.h"


void edit_registration_code() {
  textattr(COLOR_CYAN);
  app->localIO->LocalCls();
  Printf("Registration Number  : %d\n", syscfg.wwiv_reg_number);
  nlx(2);
  textattr(COLOR_YELLOW);
  Printf("<ESC> when done.\n");

  EditItems items{ new NumberEditItem<uint32_t>(23, 0, &syscfg.wwiv_reg_number) };
  items.Run();
  save_config();
}
