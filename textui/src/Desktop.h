#pragma once
#include "View.h"
#include "Window.h"
#include "Colors.h"
#include "MenuBar.h"

class StatusBar;

class UIDesktop : public UIView
{
private:
    static UIDesktop *m_desktop;
    StatusBar *m_statusBar;
    UIMenuBar *m_menuBar;
    UIColors m_colors;
    UIDesktop( bool hasMenuBar, bool hasStatusBar );
    UIMenuBar* CreateMenuBar();
    StatusBar* CreateStatusBar();

public:
    void SetStatusBarText( const std::string text );
    virtual ~UIDesktop();
    UIMenuBar* GetMenuBar() const;
    UIView* GetStatusBar() const;
    void Paint();
    virtual void Idle();

    bool ProcessKeyEvent( int key );
    void TerminateApplication( int exitCode );

//
// Static Functions
//
public: 
    static UIDesktop* InitializeDesktop( bool hasMenuBar, bool hasStatusBar );
    static UIDesktop* GetDesktop() { return m_desktop; }


};
