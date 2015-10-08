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
#ifndef __INCLUDED_BBS_MSGBASE_H__
#define __INCLUDED_BBS_MSGBASE_H__

#include <string>
#include "sdk/vardec.h"


void remove_link(messagerec * pMessageRecord, const std::string fileName);
void savefile(char *b, long lMessageLength, messagerec * pMessageRecord, const std::string fileName);
char *readfile(messagerec * pMessageRecord, const std::string fileName, long *plMessageLength);
void LoadFileIntoWorkspace(const char *pszFileName, bool bNoEditAllowed);
bool ForwardMessage(int *pUserNumber, int *pSystemNumber);
std::unique_ptr<File> OpenEmailFile(bool bAllowWrite);
void sendout_email(const std::string& title, messagerec * msg, int anony, int nUserNumber, int nSystemNumber, int an,
  int nFromUser, int nFromSystem, int nForwardedCode, int nFromNetworkNumber);
bool ok_to_mail(int nUserNumber, int nSystemNumber, bool bForceit);
void email(int nUserNumber, int nSystemNumber, bool forceit, int anony, bool force_title = false,
  bool bAllowFSED = true);
void imail(int nUserNumber, int nSystemNumber);
void read_message1(messagerec * pMessageRecord, char an, bool readit, bool *next, const char *pszFileName,
  int nFromSystem, int nFromUser);
void read_message(int n, bool *next, int *val);
void lineadd(messagerec* pMessageRecord, const std::string& sx, const std::string fileName);



#endif  // __INCLUDED_BBS_MSGBASE_H__