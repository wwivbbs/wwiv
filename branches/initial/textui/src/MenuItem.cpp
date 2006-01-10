#include "StdAfx.h"
#include "MenuItem.h"


UIMenuItem::UIMenuItem( std::string text, UICommand *command  ) : UIComponent( text ), m_selected( false ), m_command( command )
{
}


UIMenuItem::UIMenuItem( const UIMenuItem& m )
{
    *this = m;
}


const UIMenuItem& UIMenuItem::operator= (const UIMenuItem& right )
{
    UIComponent::operator =(right);
    SetCommand( right.GetCommand() );
    SetSelected( right.IsSelected() );
    return *this;
}


UIMenuItem::~UIMenuItem()
{
}

