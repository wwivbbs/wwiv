/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_SR_H__
#define __INCLUDED_BBS_SR_H__

#include "sdk/vardec.h"

void calc_CRC(unsigned char b);
char gettimeout(double d, bool *abort);
int  extern_prot(int nProtocolNum, const char *pszFileNameToSend, bool bSending);
bool ok_prot(int nProtocolNum, xfertype xt);
char *prot_name(int nProtocolNum);
int  get_protocol(xfertype xt);
void ascii_send(const char *pszFileName, bool *sent, double *percent);
void maybe_internal(const char *pszFileName, bool *xferred, double *percent, bool bSend, int prot);
void send_file(const char *pszFileName, bool *sent, bool *abort, const char *sfn, int dn, long fs);
void receive_file(const char *pszFileName, int *received, const char *sfn, int dn);
char end_batch1();
void endbatch();


#endif  // __INCLUDED_BBS_SR_H__