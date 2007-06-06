#if !defined(TUI_VIEW_H)
#define TUI_VIEW_H

class UIView
{
public:
    typedef std::vector<UIView*> Children;
protected:
    std::string m_id;
    UIView( UIView* view, WINDOW *peer );
    UIView();
    void SetPeer( WINDOW *peer );
    void SetParent( UIView* view );
private:
    WINDOW* m_peer;
    UIView* m_parent;
    int m_attribute;
    Children m_children;
    static UIView* m_activeView;
public:
    virtual ~UIView();
    WINDOW* GetPeer() const;
    UIView* GetParent() const;
    virtual void Paint();

    virtual int GetWidth() const;
    virtual int GetHeight() const;
    virtual int GetWindowX() const;
    virtual int GetWindowY() const;
    virtual void SetXY( int x, int y );
    virtual int GetX() const;
    virtual int GetY() const;
    virtual void Write( const std::string text );
    virtual void WriteCentered( int y, const std::string text );
    virtual void WriteAt( int x, int y, const std::string text );
    virtual void Erase();
    virtual void Box( int nColorPair );
    virtual void SetAttribute( int nPair );
    virtual void SetBackground( int nPair, chtype ch = 0 );
    virtual void DrawHorizontalLine( int startx, int starty, int count );
    virtual void DrawHorizontalLine( int y );
    virtual void DrawVerticalLine( int startx, int starty, int count );
    virtual void DrawVerticalLine( int x );

    virtual void AddChild( UIView* child );
    virtual void RemoveChild( UIView* child );
    virtual Children::const_iterator GetChildrenIterator();

    virtual void Idle();
    virtual int GetKey();
    virtual bool RunLoop();
    virtual bool ProcessKeyEvent( int key );

    void SetID( std::string id ) { m_id = id; }

public:
    // Static functions
    static void SetActiveView( UIView* view ) { m_activeView = view; }
    static UIView* GetActiveView() { return m_activeView; }


};

#endif //#if !defined(TUI_VIEW_H)