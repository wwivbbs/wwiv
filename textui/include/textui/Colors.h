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
