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
#ifndef INCLUDED_BBS_QWK_QWK_UI_H
#define INCLUDED_BBS_QWK_QWK_UI_H

#include "bbs/qwk/qwk_struct.h"
#include "sdk/qwk_config.h"
#include <string>

namespace wwiv::bbs::qwk {

std::string qwk_which_zip();

/**
 * Gets the archiver # to use for QWK.  
 * 
 * If allow_ask_each_time, then add "ask each time" to the list.
 * Also update "abort" if the caller enters (Q) to quit.
 */
int select_qwk_archiver(bool& abort, bool allow_ask_each_time);

std::string qwk_which_protocol();
void modify_bulletins(sdk::qwk_config& qwk_cfg);
bool get_qwk_max_msgs(uint16_t *max_msgs, uint16_t *max_per_sub);


}

#endif