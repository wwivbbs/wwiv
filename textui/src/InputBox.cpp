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

UIInputBox::UIInputBox( UIView* parent, int height, int width, int starty, int startx,
	std::string prompt, std::string title, std::string maskCharacter,
	int colorPair, bool erase, bool box ) : 
		UIWindow( parent, height, width, starty, startx, colorPair, erase, box )
{
	m_maskCharacter = maskCharacter;
	m_prompt = prompt;
	SetTitle( title );
}

bool UIInputBox::Run()
{
    GetContentPanel()->SetAttribute( UIColors::MENU_SELECTED_COLOR );
    GetContentPanel()->WriteCentered( GetHeight() - 3, " [ OK ] " );
	GetContentPanel()->SetAttribute( UIColors::TEXT_COLOR );
	GetContentPanel()->WriteAt( 1, 0, m_prompt );
    while ( true )
    {
		if(m_maskCharacter.length() > 0 && GetText().length() > 0)
		{
			GetContentPanel()->WriteAt( 1, 1, m_maskCharacter );
			for(size_t i = 1; i < GetText().length(); i++)
			{
				GetContentPanel()->Write( m_maskCharacter );
			}
		}
		else
		{
			GetContentPanel()->WriteAt( 1, 1, GetText() );
		}
	    Paint();
        int key = GetKey();
        switch ( key )
        {
        case '\n':
            return true;
        case 0x1b:
            return false;
		case KEY_BACKSPACE:
			m_text.erase(m_text.end(), m_text.end());
			break;
		default:
			if(isalpha(key) || isdigit(key))
			{
				m_text += toupper((char)key);
			}
        }
    }
    return true;
}

