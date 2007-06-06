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

#if !defined(TUI_POPUPMENU_H)
#define TUI_POPUPMENU_H

#include "Window.h"
#include "MenuItem.h"

typedef std::vector<UIMenuItem*> MenuItems;

class UIPopupMenu :
    public UIWindow
{
private:
    MenuItems m_menuItems;
    MenuItems::size_type m_currentItem;
    std::string::size_type m_itemWidth;
public:
    UIPopupMenu( UIView* parent, MenuItems items, int startx, int starty );
    void SetMenuItems( MenuItems menuItems ) { m_menuItems = menuItems; }
    virtual ~UIPopupMenu();
    virtual void Paint();
    virtual bool ProcessKeyEvent( int key );

private: // static utility functions
    static int GetLongestMenuItemLength( MenuItems& items );



};

#endif //#if !defined(TUI_POPUPMENU_H)
