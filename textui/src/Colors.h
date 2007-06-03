#if !defined(TUI_COLORS_H)
#define TUI_COLORS_H

class UIColors
{
public:
    static const long TEXT_COLOR;
    static const long WINDOW_COLOR;
    static const long TITLEANDSTATUS_COLOR;
    static const long MENU_COLOR;
    static const long MENU_SELECTED_COLOR;
    static const long LINE_COLOR;
    static const long LINE_SELECTED_COLOR;
    static const long BACKGROUND_TEXT_COLOR;

public:
    UIColors();
    chtype GetColor( int num ) const;
    void LoadScheme( std::string colorSchemeName );
public:
    virtual ~UIColors();
};

#endif //#if !defined(TUI_COLORS_H)
