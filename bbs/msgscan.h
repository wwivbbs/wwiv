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
/**************************************************************************/
#ifndef __INCLUDED_BBS_MSGSCAN_H__
#define __INCLUDED_BBS_MSGSCAN_H__

enum class MsgScanOption {
  SCAN_OPTION_READ_PROMPT,
  SCAN_OPTION_LIST_TITLES,
  SCAN_OPTION_READ_MESSAGE
};

void scan(int msgnum, MsgScanOption scan_option, bool &next_sub, bool title_scan);

#endif  // __INCLUDED_BBS_MSGSCAN_H__
