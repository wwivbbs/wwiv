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
