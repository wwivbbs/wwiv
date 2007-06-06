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