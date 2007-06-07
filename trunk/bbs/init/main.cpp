/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#define _DEFINE_GLOBALS_
#include "wwiv.h"
#undef _DEFINE_GLOBALS_

#include "TextUI.h"

#include "sys_general.h"
#include "sys_paths.h"

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
		infoWindow.AddText( "WWIV Init", true );
        infoWindow.AddText( "" );
        infoWindow.AddText( "Copyright (c) 2007 WWIV Software Services", true );
        infoWindow.AddText( "All Rights Reserved", true );
        infoWindow.AddText( "http://wwiv.sourceforge.net", true );
        infoWindow.SetTitle( "WWIV Init" );
        infoWindow.Run();
        return true;
    }
};

void CreateMenus( UIMenuBar* menuBar )
{
    UIMenu *fileMenu = new UIMenu( "File" );
    fileMenu->Add( new UIMenuItem ( "Exit", new ExitCommand ) );
    menuBar->AddMenu( fileMenu );
    UIMenu *systemMenu = new UIMenu( "System" );
	systemMenu->Add(new UIMenuItem( "General", new SystemGeneral ) );
	systemMenu->Add(new UIMenuItem( "Paths", new SystemPaths ) );
	systemMenu->Add(new UIMenuItem( "Networks", NULL ) );
    menuBar->AddMenu( systemMenu );
    UIMenu *optionsMenu = new UIMenu( "Options" );
	optionsMenu->Add(new UIMenuItem( "External Editors", NULL ) );
	optionsMenu->Add(new UIMenuItem( "Xfer protocols", NULL ) );
	optionsMenu->Add(new UIMenuItem( "Archivers", NULL ) );
	optionsMenu->Add(new UIMenuItem( "Security levels", NULL ) );
	optionsMenu->Add(new UIMenuItem( "Auto-Validation", NULL ) );
    menuBar->AddMenu( optionsMenu );
    UIMenu *modemMenu = new UIMenu( "Modem" );
	modemMenu->Add(new UIMenuItem( "Port(s)", NULL ) );
	modemMenu->Add(new UIMenuItem( "Auto-detect", NULL ) );
    menuBar->AddMenu( modemMenu );
}

int main( int argc, char* argv[] )
{
    UIDesktop* desktop = UIDesktop::InitializeDesktop( true, true );
    UISubWindow *window = new UISubWindow( desktop, 6, 50, 10, 15, UIColors::BACKGROUND_TEXT_COLOR, false );

    std::string name(" WWIV Init ");
    name += wwiv_version;
    name += " ";
    name += beta_version;
    name += " ";
    window->WriteCentered( 1, name );
    window->WriteCentered( 3, " Copyright (c) 2007 WWIV Software Services. " );
    window->WriteCentered( 5, " All Rights Reserved. " );

    WFile configFile( CONFIG_DAT );
    int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
    if ( configFile.Open( nFileMode ) )
    {
        configFile.Read( &syscfg, sizeof( configrec ) );
        configFile.Close();
        UIInputBox *pwInput = new UIInputBox(desktop, 5, 40, 0, 0, "Enter system password:", "Authentication", "*");
        pwInput->Run();
		std::string entered = pwInput->GetText();
		if(!wwiv::stringUtils::IsEqualsIgnoreCase(syscfg.systempw, entered.c_str()))
        {
            UIMessageBox msgBox(desktop, 8, 40, 5, 20, true);
            msgBox.AddText("System password does not match", true);
            msgBox.Run();
            delete pwInput;
            delete window;
            delete desktop;
	        exit(-1);
        }
        delete pwInput;
    }
    else
    {
        UIInputBox *pwInput = new UIInputBox(desktop, 5, 40, 0, 0, "Enter new system password:", "Authentication");
        pwInput->Run();
	memset( &syscfg, 0, sizeof( configrec ) );
        strcpy(syscfg.systempw, pwInput->GetText().c_str());
	int nFileMode = WFile::modeCreateFile | WFile::modeReadWrite | WFile::modeBinary;
        if ( configFile.Open( nFileMode ) )
	{
	    configFile.Write( &syscfg, sizeof( configrec ) );
	    configFile.Close();
            UIMessageBox box(desktop, 5, 40, 0, 0);
            box.WriteCentered(1, "Created new SYSTEM.DAT");
	    box.Run();
	}
        delete pwInput;
    }

    CreateMenus( desktop->GetMenuBar() );

    desktop->Paint();
    desktop->RunLoop();
    delete window;
    delete desktop;

    nFileMode = WFile::modeReadWrite | WFile::modeBinary;
    configFile.Open( nFileMode );
    configFile.Write( &syscfg, sizeof( configrec ) );
    configFile.Close();
    return 0;
}
