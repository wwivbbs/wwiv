// TUI.cpp : Defines the entry point for the console application.
//
#include <iostream>
#include "StdAfx.h"
#include "Desktop.h"
#include "Colors.h"
#include "SubWindow.h"
#include "Command.h"
#include "MessageBox.h"
#include "Menu.h"
#include "MenuBar.h"

class ExitCommand : public UICommand
{
    virtual bool Execute()
    {
        UIDesktop::GetDesktop()->TerminateApplication( 0 );
        return true;
    }
};

class InfoCommand : public UICommand
{
    virtual bool Execute()
    {
        UIDesktop *desktop = UIDesktop::GetDesktop();
        UIMessageBox infoWindow( desktop, 10, 40, 5, 20, true );
        infoWindow.AddText( "About WWIV Configuration Utility", true );
        infoWindow.AddText( "" );
        infoWindow.AddText( "Copyright (c) 2006 WWIV Software Services", true );
        infoWindow.AddText( "All Rights Reserved", true );
        infoWindow.AddText( "http://wwiv.sourceforge.net", true );
        infoWindow.SetTitle( "About WWIV" );
        infoWindow.Run();
        return true;
    }
};

void CreateMenus( UIMenuBar* menuBar )
{
    UIMenu *fileMenu = new UIMenu( "File" );
    fileMenu->Add( new UIMenuItem("Switches", NULL ) );
    fileMenu->Add( new UIMenuItem("Info", new InfoCommand ) );
    fileMenu->Add( new UIMenuItem ( "Exit", new ExitCommand ) );
    menuBar->AddMenu( fileMenu );
    menuBar->AddMenu( new UIMenu( "System" ) );
    menuBar->AddMenu( new UIMenu( "Options" ) );
    menuBar->AddMenu( new UIMenu( "Modem" ) );
    menuBar->AddMenu( new UIMenu( "Manager" ) );
}


int main( int argc, char* argv[] )
{
    UIDesktop* desktop = UIDesktop::InitializeDesktop( true, true );
    UISubWindow *window = new UISubWindow( desktop, 6, 50, 10, 15, UIColors::BACKGROUND_TEXT_COLOR, false );
    window->WriteCentered( 1, " WWIV 5.0 Configuration Utility; 5.0.62 " );
    window->WriteCentered( 3, " Copyright (c) 2006 WWIV Software Services. " );
    window->WriteCentered( 5, " All Rights Reserved. " );

    CreateMenus( desktop->GetMenuBar() );

    desktop->Paint();
    desktop->RunLoop();
    delete window;
    delete desktop;
	return 0;
}
