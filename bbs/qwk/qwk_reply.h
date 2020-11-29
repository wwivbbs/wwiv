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
#ifndef INCLUDED_BBS_QWK_QWK_REPLY_H
#define INCLUDED_BBS_QWK_QWK_REPLY_H

#include "bbs/qwk/qwk_struct.h"
#include "core/datetime.h"
#include "sdk/vardec.h"
#include <optional>
#include <string>


#define BULL_SIZE     81
#define BNAME_SIZE    13

namespace wwiv::bbs::qwk {


void qwk_inmsg(const char* text, messagerec* m1, const std::string& aux, const std::string& name,
               const core::DateTime& dt);

void upload_reply_packet();


}
#endif  // _QWK_H_
