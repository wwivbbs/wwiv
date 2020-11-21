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
#ifndef INCLUDED_BBS_MENUS_MENUSPEC_H
#define INCLUDED_BBS_MENUS_MENUSPEC_H

#include <string>

namespace wwiv::bbs::menus {

int MenuDownload(const std::string& pszDirFName, const std::string& pszFName, bool bFreeDL,
                 bool bTitle);
int MenuDownload(const std::string& dir_and_fname, bool bFreeDL, bool bTitle);
bool MenuRunDoorName(const std::string& name, bool bFree);
bool MenuRunDoorNumber(int nDoorNumber, bool bFree);
void ChangeSubNumber();
void ChangeDirNumber();
void SetMsgConf(char conf_designator);
void SetDirConf(char conf_designator);
void EnableConf();
void DisableConf();
void SetNewScanMsg();

}

#endif