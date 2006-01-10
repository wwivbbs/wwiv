#pragma once
#include "view.h"
#include "SubWindow.h"

class UIWindow :
    public UIView
{
private:
    UISubWindow* m_panel;
public:
    UIWindow( UIView* parent, int height, int width, int starty, int startx, int colorPair, bool erase = true, bool box = true );
    UIView* GetContentPanel();
    void SetTitle( std::string title );
    virtual ~UIWindow();
};
