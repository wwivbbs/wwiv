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

#if !defined(TUI_MENUITEM_H)
#define TUI_MENUITEM_H

#include "Component.h"
#include "Command.h"

class UIMenuItem :
    public UIComponent
{
private:
    bool m_selected;
    UICommand* m_command;
public:
    UIMenuItem( std::string text, UICommand *command );
    virtual ~UIMenuItem();
    UIMenuItem( const UIMenuItem& m );
    const UIMenuItem& operator= (const UIMenuItem& right );

    void SetSelected( bool selected ) { m_selected = selected; }
    bool IsSelected() const { return m_selected; }

    void SetCommand( UICommand* command ) { m_command = command; }
    UICommand* GetCommand() const { return m_command; }

};

#endif //#if !defined(TUI_MENUITEM_H)

