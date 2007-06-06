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

#if !defined(TUI_COMPONENT_H)
#define TUI_COMPONENT_H

#include "View.h"

class UIComponent
{
private:
    std::string m_text;
    std::string m_description;
public:
    UIComponent( std::string text = "", std::string description = "" ) : m_text( text ), m_description( description ) {}
    virtual ~UIComponent() {}
    UIComponent( const UIComponent& m );
    const UIComponent& operator= (const UIComponent& right );

    virtual void SetText( std::string text ) { m_text = text; }
    virtual const std::string& GetText() const { return m_text; }

    virtual void SetDescription( std::string description ) { m_description = description; }
    virtual const std::string& GetDescription() const;

};

#endif //#if !defined(TUI_COMPONENT_H)

