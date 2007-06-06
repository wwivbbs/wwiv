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

#if !defined(TUI_COLORS_H)
#define TUI_COLORS_H

class UIColors
{
public:
    static const int TEXT_COLOR;
    static const int WINDOW_COLOR;
    static const int TITLEANDSTATUS_COLOR;
    static const int MENU_COLOR;
    static const int MENU_SELECTED_COLOR;
    static const int LINE_COLOR;
    static const int LINE_SELECTED_COLOR;
    static const int BACKGROUND_TEXT_COLOR;

public:
    UIColors();
    chtype GetColor( int num ) const;
    void LoadScheme( std::string colorSchemeName );
public:
    virtual ~UIColors();
};

#endif //#if !defined(TUI_COLORS_H)
