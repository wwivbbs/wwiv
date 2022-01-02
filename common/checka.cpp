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
/*                                                                        */
/**************************************************************************/
#include "common/input.h"

#include "core/eventbus.h"
#include "common/common_events.h"
#include "common/output.h"
#include "local_io/keycodes.h"

namespace wwiv::common {

using namespace wwiv::local::io;

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool Input::checka() {
  auto ignored_abort = false;
  auto ignored_next = false;
  return checka(&ignored_abort, &ignored_next);
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool Input::checka(bool* abort) {
  auto ignored_next = false;
  return checka(abort, &ignored_next);
}

/**
 * Checks to see if user aborted whatever he/she was doing.
 * Sets next to true if control-N was hit, for zipping past messages quickly.
 * Sets abort to true if control-C/X or Q was pressed.
 * returns the value of abort
 */
bool Input::checka(bool* abort, bool* next) {
  if (nsp() == -1) {
    *abort = true;
    bout.clear_lines_listed();
    clearnsp();
  }
  while (bkbhit() && !*abort && !sess().hangup()) {
    core::bus().invoke<CheckForHangupEvent>();
    const auto ch = bgetch();
    switch (ch) {
    case CN:
      bout.clear_lines_listed();
      *next = true;
    case CC:
    case SPACE:
    case CX:
    case 'Q':
    case 'q':
      bout.clear_lines_listed();
      *abort = true;
      break;
    case 'P':
    case 'p':
    case CS:
      bout.clear_lines_listed();
      (void) getkey();
      break;
    }
  }
  return *abort;
}

} // namespace wwiv::common