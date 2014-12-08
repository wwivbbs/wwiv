/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <cstdint>
#include <cctype>
#include <memory>

#include "bbs/menu.h"


namespace wwiv {
namespace menus {

static const char *MenuSkipSpaces(const char *pszSrc) {
  while (isspace(pszSrc[0]) && pszSrc[0] != '\0') {
    ++pszSrc;
  }
  return pszSrc;
}

static const char *MenuGetParam(const char *pszSrc, char *pszParam) {
  pszSrc = MenuSkipSpaces(pszSrc);
  pszParam[0] = 0;

  if (pszSrc[0] == 0 || pszSrc[0] == '~') {
    return pszSrc;
  }

  int nLen = 0;
  if (*pszSrc == '"') {
    ++pszSrc;
    nLen = 0;
    while (*pszSrc != '"' && *pszSrc != 0) {
      if (nLen < 50) {
        pszParam[nLen++] = *pszSrc;
      }
      ++pszSrc;
    }
    if (*pszSrc) {                           // We should either be on nullptr or the CLOSING QUOTE
      ++pszSrc;                              // If on the Quote, advance one
    }
    pszParam[nLen] = '\0';
  } else {
    nLen = 0;
    while (*pszSrc != ' ' && *pszSrc != 0 && *pszSrc != '~') {
      if (nLen < 50) {
        pszParam[nLen++] = *pszSrc;
      }
      ++pszSrc;
    }
    pszParam[nLen] = '\0';
  }
  return pszSrc;
}

static const char *MenuDoParenCheck(const char *pszSrc, int bMore, const char *porig) {
  if (pszSrc[0] == ',') {
    if (bMore == 0) {
      MenuSysopLog("Too many paramaters in line of code");
      MenuSysopLog(porig);
    }
    ++pszSrc;
    pszSrc = MenuSkipSpaces(pszSrc);
  } else if (pszSrc[0] == ')') {
    ++pszSrc;
    pszSrc = MenuSkipSpaces(pszSrc);
    if (pszSrc[0] != 0 && pszSrc[0] != '~') {
      MenuSysopLog("Invalid Code, exptected EOL after close parentheses");
      MenuSysopLog(porig);
    }
  } else if (pszSrc[0] == 0 || pszSrc[0] == '~') {
    MenuSysopLog("Unexpected end of line (wanted a close parentheses)");
  }

  return pszSrc;
}

// MenuParseLine(szSrc, szCmd, szParam1, szParam2)
//
// szSrc    = Line to parse
// szCmd    = Returns the command to be executed
// szParam1 = 1st parameter - if it exists
// szParam2 = 2cd paramater - if it exists
//
// return value is where to continue parsing this line
//
// szSrc to be interpreted can be formated like this
// either  cmd param1 param2
// or     cmd(param1, param2)
//   multiple lines are seperated with the ~ key
//   enclose multi word text in quotes
//
const char *MenuParseLine(const char *pszSrc, char *pszCmd, char *pszParam1, char *pszParam2) {
  const char *porig = pszSrc;

  pszCmd[0] = 0;
  pszParam1[0] = 0;
  pszParam2[0] = 0;

  pszSrc = MenuSkipSpaces(pszSrc);
  while (pszSrc[0] == '~' && !hangup) {
    ++pszSrc;
    pszSrc = MenuSkipSpaces(pszSrc);
  }

  int nLen = 0;
  while (isalnum(*pszSrc) && !hangup) {
    if (nLen < 30) {
      pszCmd[nLen++] = *pszSrc;
    }
    ++pszSrc;
  }
  pszCmd[nLen] = 0;

  pszSrc = MenuSkipSpaces(pszSrc);

  bool bParen = false;
  if (*pszSrc == '(') {
    ++pszSrc;
    bParen = true;
    pszSrc = MenuSkipSpaces(pszSrc);
  }
  pszSrc = MenuGetParam(pszSrc, pszParam1);
  pszSrc = MenuSkipSpaces(pszSrc);

  if (bParen) {
    pszSrc = MenuDoParenCheck(pszSrc, 1, porig);
  }

  pszSrc = MenuGetParam(pszSrc, pszParam2);
  pszSrc = MenuSkipSpaces(pszSrc);

  if (bParen) {
    pszSrc = MenuDoParenCheck(pszSrc, 0, porig);
  }

  pszSrc = MenuSkipSpaces(pszSrc);

  if (pszSrc[0] != 0 && pszSrc[0] != '~') {
    MenuSysopLog("Expected EOL");
    MenuSysopLog(porig);
  }
  return pszSrc;
}


}  // namespace menus
}  // namespace wwiv
