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
#include "bbs/bbsovl3.h"

#include "bbs/bbs.h"
#include "common/output.h"
#include "local_io/keycodes.h"

using namespace wwiv::core;
using namespace wwiv::local::io;

bool do_sysop_command(int nCommandID) {
  unsigned int nKeyStroke;
  bool bNeedToRedraw = false;

  switch (nCommandID) {
  // Commands that cause screen to need to be redrawn go here
  case COMMAND_F1:
  case COMMAND_CF1:
  case COMMAND_CF9:
  case COMMAND_F10:
  case COMMAND_CF10:
    bNeedToRedraw = true;
    nKeyStroke = nCommandID - 256;
    break;

  // Commands that don't change the screen around
  case COMMAND_SF1:
  case COMMAND_F2:
  case COMMAND_SF2:
  case COMMAND_CF2:
  case COMMAND_F3:
  case COMMAND_SF3:
  case COMMAND_CF3:
  case COMMAND_F4:
  case COMMAND_SF4:
  case COMMAND_CF4:
  case COMMAND_F5:
  case COMMAND_SF5:
  case COMMAND_CF5:
  case COMMAND_F6:
  case COMMAND_SF6:
  case COMMAND_CF6:
  case COMMAND_F7:
  case COMMAND_SF7:
  case COMMAND_CF7:
  case COMMAND_F8:
  case COMMAND_SF8:
  case COMMAND_CF8:
  case COMMAND_F9:
  case COMMAND_SF9:
  case COMMAND_SF10:
    bNeedToRedraw = false;
    nKeyStroke = nCommandID - 256;
    break;

  default:
    nKeyStroke = 0;
    break;
  }

  if (nKeyStroke) {
    if (bNeedToRedraw) {
      bout.cls();
    }

    a()->handle_sysop_key(static_cast<uint8_t>(nKeyStroke));

    if (bNeedToRedraw) {
      bout.cls();
    }
  }
  return bNeedToRedraw;
}

void ListAllColors() {
  bout.nl();
  for (int i = 0; i < 128; i++) {
    if ((i % 26) == 0) {
      bout.nl();
    }
    bout.SystemColor(i);
    bout.bprintf("%3d", i);
  }
  bout.Color(0);
  bout.nl();
}
