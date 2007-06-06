#include "TextUI.h"

UIComponent::UIComponent( const UIComponent& m )
{
    *this = m;
}


const UIComponent& UIComponent::operator= (const UIComponent& right )
{
    SetText( right.GetText() );
    SetDescription( right.GetDescription() );
    return *this;
}

const std::string& UIComponent::GetDescription() const 
{ 
    return ( !m_description.empty() ) ? m_description : m_text;
}

