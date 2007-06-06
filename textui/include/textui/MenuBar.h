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

#if !defined(TUI_MENUBAR_H)
#define TUI_MENUBAR_H

#include "view.h"
#include "SubWindow.h"
#include "Menu.h"

class UIMenuBar :
    public UISubWindow
{
public:
    typedef std::vector<UIMenu*> Menus;
private:
    Menus m_menus;
    bool m_alwaysVisible;
    Menus::size_type m_currentMenu;
public:
    UIMenuBar( UIView *parent, int height, int width, int y, int x );
    ~UIMenuBar();

    void Paint();
    void DrawMenu( UIMenu* menu );
    void AddMenu( UIMenu* menu );
    void RemoveAll();
    Menus& GetMenus();

    void SetAlwaysVisible( bool alwaysVisible ) { m_alwaysVisible = alwaysVisible; }
    bool IsAlwaysVisible() { return m_alwaysVisible; }
    void ClearAllSelected();
    virtual bool ProcessKeyEvent( int key );
};

#endif //#if !defined(TUI_MENUBAR_H)