#include "StdAfx.h"
#include "PopupMenu.h"
#include "Desktop.h"
#include "Command.h"

using std::string;
using std::max;
using std::setw;


const int PADDING_X = 2;
const int PADDING_Y = 4;

UIPopupMenu::UIPopupMenu( UIView* parent, MenuItems items, int startx, int starty ) :
    UIWindow( parent, max<int>( 4, static_cast<int>( items.size() + PADDING_X ) ), 
              max<int>(4, UIPopupMenu::GetLongestMenuItemLength(items) + PADDING_Y ), 
              starty, startx, UIColors::MENU_COLOR, true, true ), 
    m_currentItem( 0 ), m_menuItems( items )
{
    SetID( "popupMenu" );
    UIView::SetActiveView( this );
    m_itemWidth = GetWidth() - PADDING_Y;
    Paint();
}


UIPopupMenu::~UIPopupMenu()
{
    UIView::SetActiveView( GetParent() );
}


void UIPopupMenu::Paint()
{
    GetContentPanel()->SetAttribute( UIColors::MENU_COLOR );
    GetContentPanel()->Erase();
    int y = 0;
    for( MenuItems::const_iterator iter = m_menuItems.begin(); iter != m_menuItems.end(); ++iter )
    {
        const string text = (*iter)->GetText();
        GetContentPanel()->SetAttribute( ( y == m_currentItem ) ? UIColors::MENU_SELECTED_COLOR : UIColors::MENU_COLOR );
        std::stringstream line;
        line << std::left << setw( static_cast<std::streamsize> ( m_itemWidth ) ) << text;
        GetContentPanel()->WriteAt( 1, y++, line.str() );
    }
    UIView::Paint();
}

bool UIPopupMenu::ProcessKeyEvent( int key )
{
    if ( m_menuItems.empty() )
    {
        return false;
    }

    MenuItems::size_type numItems = m_menuItems.size();
    switch ( key )
    {
    case KEY_UP:
        m_currentItem = ( m_currentItem + numItems - 1 ) % numItems;
        break;
    case KEY_DOWN:
        m_currentItem = ( m_currentItem + 1 ) % numItems;
        break;
    case '\n':
        {
            const UIMenuItem* menuItem = m_menuItems.at( m_currentItem );
            // Set status bar text with the description
            UIDesktop::GetDesktop()->SetStatusBarText( menuItem->GetDescription() );
            UICommand* command = menuItem->GetCommand();
            if ( command != NULL )
            {
                command->Execute();
            }
        }
        break;
    default:
        return false;
    }
    Paint();
    return true;
}


int UIPopupMenu::GetLongestMenuItemLength( MenuItems& items )
{
    string::size_type longestLength = 0;
    for( MenuItems::const_iterator iter = items.begin(); iter != items.end(); ++iter )
    {
        const string::size_type length = (*iter)->GetText().length();
        longestLength = max<string::size_type>( longestLength, length );

    }
    return static_cast<int>( longestLength );
}

