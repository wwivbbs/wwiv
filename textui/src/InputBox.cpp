#include "StdAfx.h"
#include "InputBox.h"

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
		GetContentPanel()->WriteAt( 1, 1, GetText() );
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
				if(m_maskCharacter.begin() != m_maskCharacter.end())
				{
					m_text += m_maskCharacter;
				}
				else
				{
					m_text += toupper((char)key);
				}
			}
        }
    }
    return true;
}