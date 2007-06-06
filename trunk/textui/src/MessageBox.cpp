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
