/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_MESSAGE_FIND_H
#define INCLUDED_BBS_MESSAGE_FIND_H

namespace wwiv::common {
class FullScreenView;
}

enum class MsgScanOption;

namespace wwiv::bbs {

struct find_message_result_t {
  // If found:   MsgScanOption::SCAN_OPTION_READ_MESSAGE;
  // otherwise:  MsgScanOption::SCAN_OPTION_READ_PROMPT;
  bool found{false};
  int msgnum{-1};
};

/**
 * Searches for the next message in the current area using last direction and
 * search string.
 */
find_message_result_t FindNextMessageAgain(int msgno);

/**
 * Searches for the next message in the current area using prompting for the
 * direction and search string.
 */
find_message_result_t FindNextMessage(int msgno);

/**
 * Searches for the next message in the current area using prompting for the
 * direction and search string using the FullScreenView's Command Area.
 */
find_message_result_t FindNextMessageFS(common::FullScreenView& fs, int msgno);

}

#endif