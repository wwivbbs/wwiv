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
#ifndef __INCLUDED_BBS_BBSUTL_H__
#define __INCLUDED_BBS_BBSUTL_H__


bool inli(std::string* outBuffer, std::string* rollOver, std::string::size_type nMaxLen, bool bAddCRLF = true,
  bool bAllowPrevious = false, bool bTwoColorChatMode = false, bool clear_previous_line = false);
bool inli(char *pszBuffer, char *pszRollover, std::string::size_type nMaxLen, bool bAddCRLF = true,
  bool bAllowPrevious = false, bool bTwoColorChatMode = false, bool clear_previous_line = false);
bool so();
bool cs();
bool lcs();
bool checka();
bool checka(bool *abort);
bool checka(bool *abort, bool *next);
void pla(const std::string& text, bool *abort);
void plal(const std::string& text, std::string::size_type limit, bool *abort);
bool sysop2();
bool checkcomp(const char *pszComputerType);
int  check_ansi();
bool set_language_1(int n);
bool set_language(int n);
// todo(rush): make this a C++11 enum
char *mmkey(int dl, int area = 0 /* mmkeyNoArea */, bool bListOption = false);
const char *YesNoString(bool bYesNo);


#endif  // __INCLUDED_BBS_BBSUTL_H__