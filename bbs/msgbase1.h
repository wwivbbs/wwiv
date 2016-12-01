/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_MSGBASE1_H__
#define __INCLUDED_BBS_MSGBASE1_H__

#include "sdk/vardec.h"
#include "sdk/subxtr.h"

void send_net_post(postrec* pPostRecord, const wwiv::sdk::subboard_t& sub);
void post();
void grab_user_name(messagerec*m, const std::string& file_name, int network_number);
void scan(int nMessageNumber, int nScanOptionType, int *nextsub, bool bTitleScan);
void qscan(int nBeginSubNumber, int *pnNextSubNumber);
void nscan(int nStartingSubNum = 0);
void ScanMessageTitles();
void remove_post();

#endif  // __INCLUDED_BBS_MSGBASE1_H__