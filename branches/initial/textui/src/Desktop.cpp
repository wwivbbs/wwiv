#include "StdAfx.h"
#include "Desktop.h"
#include "View.h"
#include "SubWindow.h"
#include "Colors.h"

#pragma comment( lib, "C:\\wwiv\\pdcurs27\\win32\\pdcurses.lib" )

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // _WIN32


UIDesktop* UIDesktop::m_desktop;


class StatusBar : public UISubWindow
{
public:
    StatusBar( UIView *parent, int height, int width, int y, int x );
    void SetText( const std::string text );
};



StatusBar::StatusBar( UIView *parent, int height, int width, int y, int x ) : 
    UISubWindow( parent, height, width, y, x, UIColors::MENU_COLOR, true )
{
    SetAttribute( UIColors::LINE_COLOR );
    DrawHorizontalLine( 0 );
    Paint();
    m_id = "statusBar";
}


void StatusBar::SetText( const std::string text )
{
    Erase();
    SetAttribute( UIColors::LINE_COLOR );
    DrawHorizontalLine( 0 );
    SetAttribute( UIColors::TEXT_COLOR );
    WriteAt( 0, 1, text );
    Paint();
}



UIDesktop::UIDesktop( bool hasMenuBar, bool hasStatusBar ) : UIView(), m_menuBar( NULL ), m_statusBar( NULL )
{
    initscr();
    start_color();
    cbreak ();
    noecho ();
    nodelay (stdscr, TRUE);
    halfdelay(10);
    keypad (stdscr, TRUE);
    scrollok (stdscr, TRUE);
    m_colors.LoadScheme("");
    SetPeer( stdscr );
   
    wbkgdset( stdscr, m_colors.GetColor( UIColors::WINDOW_COLOR ) | ACS_BOARD );
    werase( stdscr );
    if ( hasMenuBar )
    {
        m_menuBar = CreateMenuBar();
        SetActiveView( m_menuBar );
    }
    else
    {
        UIView::SetActiveView( this );
    }
    if ( hasStatusBar )
    {
        m_statusBar = CreateStatusBar();
    }
    m_id = "desktop";
}


UIDesktop::~UIDesktop()
{
    delete m_menuBar;
    m_menuBar = NULL;
    delete m_statusBar;
    m_statusBar = NULL;
}


UIDesktop* UIDesktop::InitializeDesktop( bool hasMenuBar, bool hasStatusBar )
{
    if ( m_desktop == NULL )
    {
        m_desktop = new UIDesktop( hasMenuBar, hasStatusBar );
    }
    m_desktop->Paint();
    return m_desktop;
}


void UIDesktop::TerminateApplication( int exitCode )
{
    endwin();
    delete this;
    exit( exitCode );
}

void UIDesktop::Paint()
{
    UIView::Paint();
}


UIView* UIDesktop::GetStatusBar() const
{
    return m_statusBar;
}


void UIDesktop::SetStatusBarText( const std::string text )
{
    if ( m_statusBar != NULL )
    {
        m_statusBar->SetText( text );
    }
}


UIMenuBar* UIDesktop::CreateMenuBar()
{
    return new UIMenuBar( this, 2, getmaxx( stdscr ), 0, 0 );
}


UIMenuBar* UIDesktop::GetMenuBar() const
{
    return m_menuBar;
}


StatusBar* UIDesktop::CreateStatusBar()
{
    const int height = 2;
    const int width = getmaxx( stdscr );
    const int y = getmaxy( stdscr ) - height;
    return new StatusBar( this, height, width, y, 0 );
}


void UIDesktop::Idle()
{
    UIView::Idle();
}


bool UIDesktop::ProcessKeyEvent( int key )
{
    switch ( key )
    {
    case '\n':
        Beep( 500, 250 );
        break;
    case 0x1b:
        TerminateApplication( 0 );
        break;
    default:
        return false;
    }
    return true;
}