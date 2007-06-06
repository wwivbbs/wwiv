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

#include "wwiv.h"
#include "TextUI.h"
#include "sys_paths.h"

bool SystemPaths::Execute()
{
    UIDesktop *desktop = UIDesktop::GetDesktop();
    UIMenu *menu = new UIMenu( "System Paths" );
    UIPopupMenu *popup = menu->ShowPopupMenu( desktop, 0, 0 );

    while( true )
    {
        int key = GetKey();
	if(key == 0x1b) break;
	popup->ProcessKeyEvent(key);
    }

    menu->HidePopupMenu();

    delete menu;
    return false;
}

int SystemPaths::GetKey()
{
    while ( true )
    {
        int key = getch();
        if ( key != ERR )
        {
            return key;
        }
    }
    return ERR;
}

void SystemPaths::ValidatePaths()
{
}
