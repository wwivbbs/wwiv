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

#include "TextUI.h"

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
	if (m_menus.empty()) {
		return false;
	}
    UIMenu* previousMenu = m_menus.at( m_currentMenu );
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

    // Since we handled the key, we should make ourselves or the 
    // current menu the active view.
    UIMenu* currentMenu = m_menus.at( m_currentMenu );

	// If we are still on the same menu, don't do anything.
	if (currentMenu == previousMenu) {
		//return false;
	}

    if ( previousMenu != nullptr && previousMenu != currentMenu )
    {
        previousMenu->HidePopupMenu();
    }
	UIView::SetActiveView( currentMenu->GetPopupMenu() );

    return true;
}

