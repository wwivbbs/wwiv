/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*              Copyright (C)2014-2017, WWIV Software Services            */
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
#include "init/regcode.h"

#include <memory>

#include "init/init.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "localui/input.h"
#include "localui/wwiv_curses.h"

using std::unique_ptr;

void edit_registration_code() {
  EditItems items{};
  items.add(new Label(2, 2, 22, "Registration Number:"),
            new NumberEditItem<uint32_t>(25, 2, &syscfg.wwiv_reg_number));
  items.Run("WWIV 4.x Registration");
  save_config();
}
