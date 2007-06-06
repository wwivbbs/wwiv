#if !defined(TUI_COMMAND_H)
#define TUI_COMMAND_H

class UICommand
{
public:
    UICommand();
    virtual ~UICommand();

    virtual bool Execute() = 0;
};

#endif //#if !defined(TUI_COMMAND_H)
