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

UIMessageBox::UIMessageBox( UIView* parent, int height, int width, int starty, int startx, bool okButton  ) :
    UIWindow( parent, height, width, starty, startx, UIColors::WINDOW_COLOR, true, true ), 
        m_done( false ), m_okButton( okButton ), m_currentLine( 1 )
{
    m_maxHeight = GetContentPanel()->GetHeight() - 1;
    if ( okButton )
    {
        --m_maxHeight;
    }
}

UIMessageBox::~UIMessageBox()
{
}

bool UIMessageBox::AddText( std::string text, bool centered, int colorPair )
{
    if ( m_currentLine >= m_maxHeight )
    {
        return false;
    }
    UIView *v = GetContentPanel();
    v->SetAttribute( colorPair );
    if ( centered )
    {
        v->WriteCentered( m_currentLine++, text );
    }
    else
    {
        v->WriteAt( 1, m_currentLine++, text );
    }
    return true;
}

bool UIMessageBox::Run()
{
    if ( m_okButton )
    {
        GetContentPanel()->SetAttribute( UIColors::MENU_SELECTED_COLOR );
        GetContentPanel()->WriteCentered( GetHeight() - 3, " [ OK ] " );
    }
    Paint();
    while ( true )
    {
        int key = GetKey();
        switch ( key )
        {
        case '\n':
            return true;
        case 0x1b:
            return false;
        }
    }
    return true;
}
