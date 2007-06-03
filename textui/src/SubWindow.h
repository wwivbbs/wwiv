#if !defined(TUI_UISUBWINDOW_H)
#define TUI_UISUBWINDOW_H

#include "view.h"

class UISubWindow :
    public UIView
{
public:
    UISubWindow( UIView *parent, int height, int width, int starty, int startx, int colorPair, bool erase );
    virtual ~UISubWindow();
};

#endif //#if !defined(TUI_UISUBWINDOW_H)