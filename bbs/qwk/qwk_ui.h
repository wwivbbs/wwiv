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
#ifndef INCLUDED_BBS_QWK_QWK_UI_H
#define INCLUDED_BBS_QWK_QWK_UI_H
#include "bbs/qwk/qwk_struct.h"

#include <string>

namespace wwiv::bbs::qwk {

std::string qwk_which_zip();
int select_qwk_archiver(struct qwk_junk* qwk_info, int ask);
std::string qwk_which_protocol();
unsigned short select_qwk_protocol(struct qwk_junk *qwk_info);
void modify_bulletins(qwk_config& qwk_cfg);


}

#endif