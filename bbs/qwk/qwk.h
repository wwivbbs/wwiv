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
#ifndef INCLUDED_BBS_QWK_QWK_H
#define INCLUDED_BBS_QWK_QWK_H

#include "bbs/qwk/qwk_struct.h"
#include "core/datetime.h"
#include "sdk/vardec.h"
#include <optional>
#include <string>


#define BULL_SIZE     81
#define BNAME_SIZE    13

namespace wwiv::bbs::qwk {

/* File: qwk.c */

void build_qwk_packet();
void qwk_gather_sub(uint16_t bn, struct qwk_junk *qwk_info);
void qwk_start_read(int msgnum, struct qwk_junk *qwk_info);
void make_pre_qwk(int msgnum, struct qwk_junk *qwk_info);
void put_in_qwk(postrec *m1, const char *fn, int msgnum, struct qwk_junk *qwk_info);
int get_qwk_max_msgs(uint16_t *max_msgs, uint16_t *max_per_sub);
void qwk_nscan();
void finish_qwk(struct qwk_junk *qwk_info);


// Main QWK functions
void qwk_menu();
void qwk_sysop();
void config_qwk_bw();

}
#endif  // _QWK_H_
