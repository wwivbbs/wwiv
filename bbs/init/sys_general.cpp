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
#include "sys_general.h"

bool SystemGeneral::Execute()
{
    UIDesktop *desktop = UIDesktop::GetDesktop();
    UIMenu *menu = new UIMenu( "General Information" );
	menu->Add(new UIMenuItem( "System Password", NULL ) );
	menu->Add(new UIMenuItem( "Sysop Name", NULL ) );
	menu->Add(new UIMenuItem( "Sysop high time", NULL ) );
	menu->Add(new UIMenuItem( "Sysop low time", NULL ) );
	menu->Add(new UIMenuItem( "System Name", NULL ) );
	menu->Add(new UIMenuItem( "System Phone", NULL ) );
	menu->Add(new UIMenuItem( "Closed System", NULL ) );
	menu->Add(new UIMenuItem( "Newuser PW", NULL ) );
	menu->Add(new UIMenuItem( "Newuser restrict", NULL ) );
	menu->Add(new UIMenuItem( "Newuser SL", NULL ) );
	menu->Add(new UIMenuItem( "Newuser SSL", NULL ) );
	menu->Add(new UIMenuItem( "Network high time", NULL ) );
	menu->Add(new UIMenuItem( "Network low time", NULL ) );
	menu->Add(new UIMenuItem( "Up/Download ratio", NULL ) );
	menu->Add(new UIMenuItem( "Post/Call ratio", NULL ) );
	menu->Add(new UIMenuItem( "Max waiting", NULL ) );
	menu->Add(new UIMenuItem( "Max users", NULL ) );
	menu->Add(new UIMenuItem( "Caller number", NULL ) );
	menu->Add(new UIMenuItem( "Days active", NULL ) );
	UIView *currentActiveView = desktop->GetActiveView();
    UIPopupMenu *popup = menu->ShowPopupMenu( desktop, 20, 5 );

    while( true )
    {
        int key = GetKey();
		if(key == 0x1b) break;
		popup->ProcessKeyEvent(key);
    }

    menu->HidePopupMenu();

    delete menu;

	desktop->SetActiveView(currentActiveView);

    return false;
}

int SystemGeneral::GetKey()
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