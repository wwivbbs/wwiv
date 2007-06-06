/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2007, WWIV Software Services                */
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

#if !defined(TUI_MENU_H)
#define TUI_MENU_H

#include "Component.h"
#include "MenuItem.h"
#include "PopupMenu.h"

class UIMenu :
    public UIMenuItem
{
private:
    MenuItems m_menuItems;
    int m_screenPosition;
    UIPopupMenu* m_popupMenu;
    int m_currentMenuItem;
public:
    UIMenu();
    UIMenu( std::string text );
    UIMenu( const UIMenu& m );
    const UIMenu& operator= (const UIMenu& right );
    virtual ~UIMenu();

    void Add( UIMenuItem* menuItem );
    void RemoveAll();

    void SetScreenPosition( int x ) { m_screenPosition = x; }
    int GetScreenPosition() { return m_screenPosition; }

    UIPopupMenu* ShowPopupMenu( UIView* parent, int x, int y );
    UIPopupMenu* GetPopupMenu() { return m_popupMenu; }
    void HidePopupMenu() { delete m_popupMenu; m_popupMenu = NULL; }

};

#endif //#if !defined(TUI_MENU_H)
