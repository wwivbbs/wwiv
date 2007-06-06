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
