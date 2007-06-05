#include "StdAfx.h"
#include "Colors.h"


const int UIColors::TEXT_COLOR  = 0;
const int UIColors::WINDOW_COLOR = 1;
const int UIColors::TITLEANDSTATUS_COLOR = 2;
const int UIColors::MENU_COLOR = 3;
const int UIColors::MENU_SELECTED_COLOR = ( 4 | A_BOLD );
const int UIColors::LINE_COLOR = ( 5 | A_BOLD );
const int UIColors::LINE_SELECTED_COLOR = ( 6 | A_BOLD );
const int UIColors::BACKGROUND_TEXT_COLOR = 7;


UIColors::UIColors()
{
}

UIColors::~UIColors()
{
}


void UIColors::LoadScheme( std::string colorSchemeName )
{
    // TODO - Support Schemes
    init_pair( static_cast<short>( TEXT_COLOR ), COLOR_CYAN, COLOR_BLACK );
    init_pair( static_cast<short>( WINDOW_COLOR ), COLOR_WHITE, COLOR_BLACK );
    init_pair( static_cast<short>( TITLEANDSTATUS_COLOR ), COLOR_CYAN, COLOR_BLACK );
    init_pair( static_cast<short>( MENU_COLOR ), COLOR_CYAN, COLOR_BLACK );
    init_pair( static_cast<short>( MENU_SELECTED_COLOR ), COLOR_BLUE, COLOR_CYAN );
    init_pair( static_cast<short>( LINE_COLOR ), COLOR_BLUE, COLOR_BLACK );
    init_pair( static_cast<short>( LINE_SELECTED_COLOR ), COLOR_CYAN, COLOR_BLACK );
    init_pair( static_cast<short>( BACKGROUND_TEXT_COLOR ), COLOR_BLACK, COLOR_WHITE );

}

chtype UIColors::GetColor( int num ) const
{
    return COLOR_PAIR( num );
}
