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
