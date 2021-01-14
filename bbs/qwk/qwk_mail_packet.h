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
#ifndef INCLUDED_BBS_QWK_QWK_MAIL_PACKET_H
#define INCLUDED_BBS_QWK_QWK_MAIL_PACKET_H

#include "bbs/qwk/qwk_struct.h"
#include <cstdint>


struct postrec;

namespace wwiv::bbs::qwk {


void build_qwk_packet();
void qwk_gather_sub(uint16_t bn, qwk_state *qwk_info);
void qwk_start_read(int msgnum, qwk_state *qwk_info);
void make_pre_qwk(int msgnum, qwk_state *qwk_info);
void put_in_qwk(postrec *m1, const char *fn, int msgnum, qwk_state *qwk_info);
void qwk_nscan();
void finish_qwk(qwk_state *qwk_info);

}

#endif