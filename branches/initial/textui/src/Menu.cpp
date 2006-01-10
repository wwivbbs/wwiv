#include "StdAfx.h"
#include "Menu.h"
#include "Colors.h"

using std::max;
using std::string;

const int PADDING_X = 4;
const int PADDING_Y = 2;

UIMenu::UIMenu( std::string text ) : UIMenuItem( text, NULL ), m_screenPosition( -1 ), m_popupMenu( NULL ), m_currentMenuItem( 0 )
{
}


UIMenu::UIMenu( const UIMenu& m ) : UIMenuItem( m ), m_screenPosition( -1 ), m_popupMenu( NULL ), m_currentMenuItem( 0 )
{
    *this = m;
}


const UIMenu& UIMenu::operator= (const UIMenu& right )
{
    UIMenuItem::operator = ( right );
    SetText( right.GetText() );
    m_popupMenu = right.m_popupMenu;
    m_currentMenuItem = right.m_currentMenuItem;
    m_screenPosition = right.m_screenPosition;
    m_popupMenu = right.m_popupMenu;
    return *this;
}


UIMenu::~UIMenu()
{
    RemoveAll();
}


void UIMenu::Add( UIMenuItem* menuItem )
{
    m_menuItems.push_back( menuItem );
}


void UIMenu::RemoveAll()
{
    for( MenuItems::const_iterator iter = m_menuItems.begin(); iter != m_menuItems.end(); ++iter )
    {
        UIMenuItem *item = *iter;
        delete item;
    }
    m_menuItems.clear();
}


UIPopupMenu* UIMenu::ShowPopupMenu( UIView* parent, int startx, int starty )
{
    delete m_popupMenu;
    m_popupMenu = new UIPopupMenu( parent, m_menuItems, startx, starty );
    return m_popupMenu;
}




