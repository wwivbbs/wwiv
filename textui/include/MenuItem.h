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