#include "StdAfx.h"
#include "MenuBar.h"
#include "Colors.h"
#include "Desktop.h"

UIMenuBar::UIMenuBar( UIView* parent, int height, int width, int y, int x ) : 
UISubWindow( parent, height, width, y, x, UIColors::MENU_COLOR, true ), m_alwaysVisible( true ), m_currentMenu( 0 )
{
    SetAttribute( UIColors::LINE_COLOR );
    DrawHorizontalLine( 1 );
    Paint();
    m_id = "menuBar";
}


UIMenuBar::~UIMenuBar()
{
    RemoveAll();
}


void UIMenuBar::Paint()
{
    if ( m_menus.empty() )
    {
        return;
    }

    size_t itemsLength = 0;
    for( Menus::const_iterator iter = m_menus.begin(); iter != m_menus.end(); ++iter )
    {
        UIMenu *item = *iter;
        itemsLength += item->GetText().length();
        item->SetSelected( false );
    }
    UIMenu *selectedItem = m_menus.at( m_currentMenu );
    selectedItem->SetSelected( true );
    const size_t space = GetWidth() - itemsLength;
    const size_t eachSpace = space / (m_menus.size() + 1);
    const size_t extra = space % ( m_menus.size() + 1 );
    const size_t extraLeft = extra / 2;
    const size_t extraRight = ( extra / 2 ) + (extra % 2);
    SetAttribute( UIColors::MENU_COLOR );
    size_t lastStartPos = eachSpace + extraLeft;
    for( Menus::const_iterator iter = m_menus.begin(); iter != m_menus.end(); ++iter )
    {
        UIMenu *item = *iter;
        item->SetScreenPosition( static_cast<int>( lastStartPos - 1 ) );
        std::string text = item->GetText();
        SetAttribute( item->IsSelected() ? UIColors::MENU_SELECTED_COLOR : UIColors::MENU_COLOR );
        WriteAt( static_cast<int>(lastStartPos-1), 0, " " );
        WriteAt( static_cast<int>(lastStartPos), 0, text );
        WriteAt( static_cast<int>(lastStartPos+text.length()), 0, " " );
        lastStartPos += ( eachSpace + text.length() );
    }

    if ( this == UIView::GetActiveView() )
    {
        DrawMenu( selectedItem );
    }
    UIView::Paint();
}


void UIMenuBar::DrawMenu( UIMenu* menu )
{
    menu->ShowPopupMenu( this, menu->GetScreenPosition(), GetHeight() );
    UIDesktop::GetDesktop()->Paint();
}


void UIMenuBar::AddMenu( UIMenu* menu )
{
    menu->SetSelected( false );
    m_menus.push_back( menu );
}


void UIMenuBar::RemoveAll()
{
    for( Menus::const_iterator iter = m_menus.begin(); iter != m_menus.end(); ++iter )
    {
        UIMenu *item = *iter;
        delete item;
    }
    m_menus.clear();
}


UIMenuBar::Menus& UIMenuBar::GetMenus()
{
    return m_menus;
}


void UIMenuBar::ClearAllSelected()
{
    for( Menus::const_iterator iter = m_menus.begin(); iter != m_menus.end(); ++iter )
    {
        UIMenu *item = *iter;
        item->SetSelected( false );
    }
}


bool UIMenuBar::ProcessKeyEvent( int key )
{
    UIMenu* lastMenu = m_menus.at( m_currentMenu );
    switch ( key )
    {
    case KEY_LEFT:
        m_currentMenu = ( m_currentMenu + m_menus.size() - 1 ) % m_menus.size();
        break;
    case KEY_RIGHT:
        m_currentMenu = ( m_currentMenu + 1 ) % m_menus.size();
        break;
    default:
        return false;
    }
    // Since we handled the key, we should make ourselves the active view.
    if ( lastMenu != NULL )
    {
        lastMenu->HidePopupMenu();
    }
    UIView::SetActiveView( this );
    //Paint();
    return true;
}