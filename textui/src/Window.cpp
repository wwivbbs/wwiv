#include "TextUI.h"

using std::max;

UIWindow::UIWindow( UIView* parent, int height, int width, int starty, int startx, int colorPair, bool erase, bool box ) : UIView()
{
    WINDOW *w = newwin( max<int>(4, height), max<int>(4, width), max(0, starty), max(0, startx ) );
    SetParent( parent );
    SetPeer( w );
    wbkgdset( w, COLOR_PAIR( colorPair ) );
    if ( erase )
    {
        werase( w );
    }
    if ( box )
    {
        Box( UIColors::LINE_COLOR );
    }
    touchwin( w );
    wrefresh( w );
    m_panel = new UISubWindow( this, height - 2, width - 2, starty + 1, startx + 1, colorPair, erase );
    parent->AddChild( this );
}

UIView* UIWindow::GetContentPanel()
{
    return m_panel;
}

void UIWindow::SetTitle( std::string title )
{
    int x = 2;
    const int width = GetWidth();
    std::string::size_type maxWidth = width - 4;
    std::stringstream s;
    if ( title.length() > maxWidth )
    {
        s << " " << title.substr( 0, maxWidth ) << " ";
    }
    else
    {
        s << " " << title << " ";
        x = static_cast<int>( width - 1 - s.str().length() );
    }
    SetAttribute( UIColors::MENU_SELECTED_COLOR );
    WriteAt( x, 0, s.str() );
}

UIWindow::~UIWindow()
{
    UIView::~UIView();
    GetParent()->RemoveChild( this );
    delete m_panel;
    GetParent()->Paint();
}
