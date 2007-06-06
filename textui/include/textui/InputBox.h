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

#if !defined(TUI_INPUTBOX_H)
#define TUI_INPUTBOX_H

#include "Colors.h"
#include "Window.h"

class UIInputBox :
    public UIWindow
{
private:
	std::string m_text;
	std::string m_prompt;
	std::string m_maskCharacter;
public:
	UIInputBox( UIView* parent, int height, int width, int starty, int startx,
		std::string prompt = "", std::string title = "", std::string maskCharacter = "",
		int colorPair = UIColors::WINDOW_COLOR, bool erase = true, bool box = true );
	virtual ~UIInputBox() {};

	void SetText( const std::string text ) { m_text = text; };
	const std::string GetText() const { return m_text; };

	bool Run();
};

#endif //#if !defined(TUI_INPUTBOX_H)