/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_MENU_H__
#define __INCLUDED_SDK_MENU_H__

#include <cstdint>

#define MENU
constexpr uint16_t MENU_VERSION = 0x0100;

constexpr uint8_t MENU_FLAG_DELETED = 0x01;
constexpr uint8_t MENU_FLAG_MAINMENU = 0x02;

constexpr uint8_t MENU_NUMFLAG_NOTHING = 0;
constexpr uint8_t MENU_NUMFLAG_SUBNUMBER = 1;
constexpr uint8_t MENU_NUMFLAG_DIRNUMBER = 2;
constexpr uint8_t MENU_NUMFLAG_LAST = 3;

constexpr uint8_t MENU_LOGTYPE_KEY = 0;
constexpr uint8_t MENU_LOGTYPE_NONE = 1;
constexpr uint8_t MENU_LOGTYPE_COMMAND = 2;
constexpr uint8_t MENU_LOGTYPE_DESC = 3;
constexpr uint8_t MENU_LOGTYPE_LAST = 4;

constexpr uint8_t MENU_HELP_DONTFORCE = 0;
constexpr uint8_t MENU_HELP_FORCE = 1;
constexpr uint8_t MENU_HELP_ONENTRANCE = 2;
constexpr uint8_t MENU_HELP_LAST = 3;

constexpr uint16_t MENU_HIDE_REGULAR = 2;
constexpr uint16_t MENU_HIDE_BOTH = 3;

constexpr size_t MENU_MAX_KEYS = 10;

#pragma pack(push, 1)

struct MenuHeader {
  char szSig[10];      /* Menu Signature WWIV431\x1a */
  char unused[54];

  uint16_t  nVersion;
  uint16_t  nEmpty;
  uint8_t   nFlags;

  uint8_t   nums;     /* What does a number do?  Set sub#, Dir#, nothing? */
  uint8_t   nLogging;     /* Types of logging, Key, None, command, desc       */

  uint8_t   nForceHelp;   /* force, dont force, on entrance only              */
  uint8_t   nAllowedMenu; /* Can pulldown, regular or both menus be used?     */

  uint8_t  nTitleColor, nMainBorderColor, nMainBoxColor, nMainTextColor,
           nMainTextHLColor, nMainSelectedColor, nMainSelectedHLColor;

  uint8_t  nItemBorderColor, nItemBoxColor, nItemTextColor, nItemTextHLColor,
           nItemSelectedColor, nItemSelectedHLColor;

  char   szMenuTitle[21];
  char   misc2[60];
  char   szPassWord[21];     /* required for entry of menu */
  uint16_t nMinSL, nMinDSL;    /* required for entry of menu */
  uint16_t uAR, uDAR;          /* required for entry of menu */
  uint16_t uRestrict;          /* not allowed restrictions   */
  bool nSysop, nCoSysop;   /* Must be either sysop or co */
  char   misc3[30];
  char   szScript[101];      /* Gets executed on entry     */
  char   szExitScript[101];  /* Executed on rtn from menu  */
  char unused_padding[109];
};

struct MenuRec {
  uint8_t nFlags;   /* AFLAG_????? */

  char szKey[MENU_MAX_KEYS + 1]; /* Keystrock to execute menu item   */
  char szExecute[101];           /* Command to execute               */
  char szMenuText[41];           /* Menu description                 */
  char unused_szPDText[41];      /* Pulldown menu text               */

  char szHelp[81];               /* Help for this item               */
  char szSysopLog[51];           /* Msg to put in the log            */

  char szInstanceMessage[81];

  /* Security */
  uint16_t nMinSL, iMaxSL;
  uint16_t nMinDSL, iMaxDSL;
  uint16_t uAR, uDAR;        /* Must match all specified to be able to run     */
  uint16_t uRestrict;        /* If any of these restrictions, you cant execute */
  bool nSysop, nCoSysop;  /* true and false, does it take a co/sysop to run */
  char szPassWord[21];

  uint16_t nHide;            /* Hide text from PD/Regular/both or no menus */
  uint16_t unused_nPDFlags;  /* special characteristis for pulldowns       */

  char unused_data[92];
};

#pragma pack(pop)

static_assert(sizeof(MenuRec) == sizeof(MenuHeader), "sizeof(MenuRec) == sizeof(MenuHeader)");


#endif  // __INCLUDED_SDK_MENU_H__
