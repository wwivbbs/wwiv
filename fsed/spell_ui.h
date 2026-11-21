/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#ifndef INCLUDED_FSED_SPELL_UI_H
#define INCLUDED_FSED_SPELL_UI_H

namespace wwiv::common {
class Context;
}

namespace wwiv::fsed {

class FsedModel;
class FsedView;

/** What started the spell check. */
enum class spell_check_trigger_t {
  // The user picked Save from the menu.
  save,
  // The user picked Spell check (C) from the menu.
  menu,
};

/**
 * Runs the spell check on the message in the editor.
 *
 * For save it only runs when the sysop and user have spell check on save
 * enabled and a dictionary is available, otherwise it does nothing.
 *
 * Returns false only if the user pressed ESC during the check to go back to
 * the editor instead of saving.
 */
bool fsed_spell_check(common::Context& ctx, FsedModel& ed, FsedView& view,
                      spell_check_trigger_t trigger);

} // namespace wwiv::fsed

#endif
