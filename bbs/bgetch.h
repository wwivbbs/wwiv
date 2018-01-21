/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_BGETCH_H__
#define __INCLUDED_BBS_BGETCH_H__

#include <functional>

char bgetch(bool allow_extended_input = false);
char bgetchraw();
bool bkbhitraw();
bool bkbhit();

// For bgetch_event, decides if the number pad turns '8' into an arrow etc.. or not
enum class numlock_status_t {
  NUMBERS,
  NOTNUMBERS
};

enum class bgetch_timeout_status_t {
  WARNING,
  CLEAR
};

typedef std::function<void(bgetch_timeout_status_t, int)> bgetch_timeout_callback_fn;
int bgetch_event(numlock_status_t numlock_mode, bgetch_timeout_callback_fn cb);
int bgetch_event(numlock_status_t numlock_mode);


#endif  // __INCLUDED_BBS_BGETCH_H__
