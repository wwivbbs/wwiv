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

using std::max;
using std::string;

const int PADDING_X = 4;
const int PADDING_Y = 2;

UIMenu::UIMenu( std::string text ) : UIMenuItem( text, nullptr ), m_screenPosition( -1 ), m_popupMenu( nullptr ), m_currentMenuItem( 0 )
{
}


UIMenu::UIMenu( const UIMenu& m ) : UIMenuItem( m ), m_screenPosition( -1 ), m_popupMenu( nullptr ), m_currentMenuItem( 0 )
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




