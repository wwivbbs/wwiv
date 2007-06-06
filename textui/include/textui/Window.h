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

#if !defined(TUI_UIWINDOW_H)
#define TUI_UIWINDOW_H

#include "View.h"
#include "SubWindow.h"

class UIWindow :
    public UIView
{
private:
    UISubWindow* m_panel;
public:
    UIWindow( UIView* parent, int height, int width, int starty, int startx, int colorPair, bool erase = true, bool box = true );
    UIView* GetContentPanel();
    void SetTitle( std::string title );
    virtual ~UIWindow();
};

#endif //#if !defined(TUI_UIWINDOW_H)
