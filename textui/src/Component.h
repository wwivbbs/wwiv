#if !defined(TUI_COMPONENT_H)
#define TUI_COMPONENT_H

#include "view.h"

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

