/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2007, WWIV Software Services                */
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

#if !defined(TUI_DESKTOP_H)
#define TUI_DESKTOP_H

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

#endif //#if !defined(TUI_DESKTOP_H)
