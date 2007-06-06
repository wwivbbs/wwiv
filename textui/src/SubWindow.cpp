#include "TextUI.h"

UISubWindow::UISubWindow( UIView *parent, int height, int width, int starty, int startx, int colorPair, bool erase ) : UIView()
{
    WINDOW *w = subwin( parent->GetPeer(), height, width, starty, startx );
    SetPeer( w );
    wbkgdset( w, COLOR_PAIR( colorPair ) );
    if ( erase )
    {
        werase( w );
    }
    touchwin( w );
    wrefresh( w );
    SetParent( parent );
    parent->AddChild( this );
}

UISubWindow::~UISubWindow(void)
{
    GetParent()->RemoveChild( this );
}
