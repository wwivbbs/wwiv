#pragma once
#include "window.h"
#include "Colors.h"


class UIMessageBox :
    public UIWindow
{
private:
    bool m_done;
    bool m_okButton;
    int m_currentLine;
    int m_maxHeight;
public:
    UIMessageBox( UIView* parent, int height, int width, int starty, int startx, bool okButton = true );
    bool AddText( std::string text, bool centered = false, int colorPair = UIColors::TEXT_COLOR );
    virtual ~UIMessageBox();

    bool Run();

};
