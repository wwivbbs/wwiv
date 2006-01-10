#pragma once
#include "view.h"

class UISubWindow :
    public UIView
{
public:
    UISubWindow( UIView *parent, int height, int width, int starty, int startx, int colorPair, bool erase );
    virtual ~UISubWindow();
};
