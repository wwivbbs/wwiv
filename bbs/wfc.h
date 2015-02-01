/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_WFC_H__
#define __INCLUDED_BBS_WFC_H__

#include <memory>
#include "initlib/curses_io.h"
#include "initlib/curses_win.h"


namespace wwiv {
namespace wfc {

class ControlCenter {
public: 
  ControlCenter();
  ~ControlCenter();
  void Run();

private:
  // Takes ownership of out to enure it's deleted on exit from the WFC.
  std::unique_ptr<CursesIO> out_scope_;
  std::unique_ptr<CursesWindow> commands_;
  std::unique_ptr<CursesWindow> status_;
  std::unique_ptr<CursesWindow> logs_;
};
}  // namespace wfc
}  // namespace wwiv

void wfc_cls();
void wfc_init();
void wfc_screen();

#endif  // __INCLUDED_BBS_WFC_H__