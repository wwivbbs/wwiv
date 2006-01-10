#pragma once
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
