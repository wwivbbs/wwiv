#include "TextUI.h"

UIView* UIView::m_activeView;

UIView::UIView(UIView* view, WINDOW *peer) : m_parent( view ), m_peer( peer )
{
}

UIView::UIView() : m_peer( NULL ), m_parent( NULL )
{
}

UIView::~UIView()
{
    if ( m_peer != stdscr && m_peer != NULL )
    {
        delwin( m_peer );
        m_peer = NULL;
    }
}

void UIView::SetPeer( WINDOW *peer )
{
    m_peer = peer;
}

WINDOW* UIView::GetPeer() const
{
    return m_peer;
}

UIView* UIView::GetParent() const
{
    return m_parent;
}

void UIView::SetParent( UIView* view )
{
    m_parent = view;
}

void UIView::Paint()
{
    touchwin( m_peer );
    wrefresh( m_peer );
    for( Children::const_iterator iter = m_children.begin(); iter != m_children.end(); ++iter )
    {
        UIView* view = *iter;
        view->Paint();
    }
}

int UIView::GetWidth() const
{
    return getmaxx( m_peer );
}

int UIView::GetHeight() const
{
    return getmaxy( m_peer );
}

int UIView::GetWindowX() const
{ 
    return getbegx( m_peer );
}

int UIView::GetWindowY() const
{ 
    return getbegy( m_peer );
}


void UIView::SetXY( int x, int y )
{
    wmove( m_peer, y, x );
}

int UIView::GetX() const
{
    int x = -1, y = -1;
    getyx( m_peer, y, x );
    return x;
}

int UIView::GetY() const
{
    int x = -1, y = -1;
    getyx( m_peer, y, x );
    return y;
}

void UIView::Write( const std::string text )
{
    waddstr( m_peer, text.c_str() );
    touchwin( m_peer );
}

void UIView::WriteCentered( int y, const std::string text )
{
    std::string::size_type length = text.length();
    std::string::size_type width = GetWidth();
    int startx = ( length < width ) ? ( static_cast<int>( ( width - length ) / 2 ) ) : 0;
    WriteAt( startx, y, text );
}

void UIView::WriteAt( int x, int y, const std::string text )
{
    mvwaddstr( m_peer, y, x, text.c_str() );
}

void UIView::Erase()
{
    werase( m_peer );
}

void UIView::Box( int nColorPair )
{
    SetAttribute( nColorPair );
    wborder( m_peer, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, 0, 0, 0, 0 );
}

void UIView::SetAttribute( int nPair )
{
    wattrset( m_peer, COLOR_PAIR( nPair ) );
}

void UIView::SetBackground( int nPair, chtype ch  )
{
    wbkgdset( m_peer, COLOR_PAIR( nPair ) | ch );
}

void UIView::DrawHorizontalLine( int y )
{
    DrawHorizontalLine( 0, y, GetWidth() );
}

void UIView::DrawHorizontalLine( int startx, int starty, int count )
{
    mvwhline( m_peer, starty, startx, ACS_HLINE, count );
}

void UIView::DrawVerticalLine( int startx, int starty, int count )
{
    mvwvline( m_peer, starty, startx, ACS_VLINE, count );
}

void UIView::DrawVerticalLine( int x )
{
    DrawVerticalLine( x, 0, GetHeight() );
}

void UIView::AddChild( UIView* child )
{
    m_children.push_back( child );
}

void UIView::RemoveChild( UIView* child )
{
    UIView::Children::iterator pos = find( m_children.begin(), m_children.end(), child );
    if ( pos != m_children.end() )
    {
        m_children.erase( pos );
    }
}

UIView::Children::const_iterator UIView::GetChildrenIterator()
{
    return m_children.begin();
}

bool UIView::ProcessKeyEvent( int key )
{
    return false;
}

void UIView::Idle()
{
    // TODO allow clients to register idle event handlers.
}

int UIView::GetKey()
{
    while ( true )
    {
        int key = getch();
        if ( key == ERR )
        {
            Idle();
        }
        else
        {
            return key;
        }
    }
}

/** main even loop */
bool UIView::RunLoop()
{
    while ( true )
    {
        int key = GetKey();
        // bubble from active view to root view looking for someone to process this.
        UIView* view = UIView::GetActiveView();
        while ( view != NULL && !view->ProcessKeyEvent( key ) )
        {
            view = view->GetParent();
        }
    }
}

