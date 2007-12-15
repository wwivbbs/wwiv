/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#ifndef __INCLUDED_COMMON_H__
#define __INCLUDED_COMMON_H__

#ifdef _WIN32
#pragma pack(push, 1)
#elif defined ( __unix__ )
#pragma pack( 1 )
#endif



struct side_menu_colors
{
    int normal_highlight;   // used to all be unsigned.
    int normal_menu_item;
    int current_highlight;
    int current_menu_item;
};



struct search_record
{
    char filemask[13];

    unsigned long nscandate;

    char search[81];

    int alldirs;
    int search_extended;
};


#define MENUTYPE_REGULAR   0
#define MENUTYPE_PULLDOWN  1

#define HOTKEYS_ON  0
#define HOTKEYS_OFF 1


struct user_config
{
    char name[31];          // verify against a user

    unsigned long status;

    unsigned long lp_options;
    unsigned char lp_colors[32];

    char szMenuSet[9];   // Selected AMENU set to use
    char cHotKeys;       // Use hot keys in AMENU
    char cMenuType;      // Use pulldown or regular menus

    char junk[118];   // AMENU took 11 bytes from here
};


#ifdef _WIN32
#pragma pack(pop)
#endif // _WIN32


#endif // __INCLUDED_COMMON_H__
