#pragma once

class UICommand
{
public:
    UICommand();
    virtual ~UICommand();

    virtual bool Execute() = 0;
};
