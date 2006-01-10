#pragma once
#include "window.h"
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
