// TUI.cpp : Defines the entry point for the console application.
//
#include <iostream>
#include "TextUI.h"

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
        infoWindow.AddText( "About Text UI", true );
        infoWindow.AddText( "" );
        infoWindow.AddText( "Copyright (c) 2007 WWIV Software Services", true );
        infoWindow.AddText( "All Rights Reserved", true );
        infoWindow.AddText( "http://www.wwivbbs.org/", true );
	infoWindow.AddText( "https://github.com/wwivbbs/wwiv/", true );
        infoWindow.SetTitle( "About TUI" );
        infoWindow.Run();
        return true;
    }
};

class InputCommand : public UICommand
{
    virtual bool Execute()
    {
        UIDesktop *desktop = UIDesktop::GetDesktop();
		UIInputBox inputBox(desktop, 5, 40, 5, 20, "Enter Name:", "Input Demo");
		inputBox.Run();
		std::string result = inputBox.GetText();
		desktop->WriteCentered(desktop->GetHeight() - 3, result);
        return true;
    }
};

void CreateMenus( UIMenuBar* menuBar )
{
    UIMenu *fileMenu = new UIMenu( "File" );
    fileMenu->Add( new UIMenuItem("Input Test", new InputCommand ) );
    fileMenu->Add( new UIMenuItem("MessageBox Test", new InfoCommand ) );
    fileMenu->Add( new UIMenuItem ( "Exit", new ExitCommand ) );
    menuBar->AddMenu( fileMenu );
    //menuBar->AddMenu( new UIMenu( "System" ) );
    //menuBar->AddMenu( new UIMenu( "Options" ) );
    //menuBar->AddMenu( new UIMenu( "Modem" ) );
    //menuBar->AddMenu( new UIMenu( "Manager" ) );
}


int main( int argc, char* argv[] )
{
    UIDesktop* desktop = UIDesktop::InitializeDesktop( true, true );
    UISubWindow *window = new UISubWindow( desktop, 6, 50, 10, 15, UIColors::BACKGROUND_TEXT_COLOR, false );
    window->WriteCentered( 1, " Text UI Demo " );
    window->WriteCentered( 3, " Copyright (c) 2007 WWIV Software Services. " );
    window->WriteCentered( 5, " All Rights Reserved. " );


    CreateMenus( desktop->GetMenuBar() );
    
    desktop->Paint();
    desktop->RunLoop();
    delete window;
    delete desktop;
	return 0;
}
