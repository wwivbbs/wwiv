/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/

#include <algorithm>
#include <iostream>
#include <sstream>

#include "wwivinit.h"
#include "wlocal_io.h"

//TODO(rushfan): Don't include windows.h from wlocal_io anymore. Make a WinConsoleIU subclass like
// this CursesIO class and move the Window.h definition into there.
#undef MOUSE_MOVED

#include "curses.h"


static int GetTopLine() { return 0; }

#ifdef _WIN32
void CursesIO::set_attr_xy(int x, int y, int a) {
    //TODO(rushfan): Implement me if needed for compile.cpp
}
#endif  // _WIN32

CursesIO::CursesIO() {
    window_ = initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    nonl();
    max_x_ = getmaxx(stdscr);
    max_y_ = getmaxy(stdscr);
    SetScreenBottom(max_y_);
    start_color();
    /*
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_CYAN | A_BOLD, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW | A_BOLD, COLOR_BLACK);
    init_pair(7, COLOR_WHITE | A_BOLD, COLOR_BLUE);
    */
    for (short b=0; b<COLORS; b++) {
        for (short f=0; f<COLORS; f++) {
            init_pair( (b * 16) + f, f, b);
        }
    }
    refresh();
}

CursesIO::~CursesIO() {
    endwin();
}

void CursesIO::LocalGotoXY(int x, int y) {
// This, obviously, moves the cursor to the location specified, offset from
// the protected dispaly at the top of the screen.  Note: this function
// is 0 based, so (0,0) is the upper left hand corner.
	x = std::max<int>( x, 0 );
	x = std::min<int>( x, max_x_ );
	y = std::max<int>( y, 0 );
	y += GetTopLine();
	y = std::min<int>( y, max_y_ );

    move(y, x);
    refresh();
}

int CursesIO::WhereX() {
/* This function returns the current X cursor position, as the number of
* characters from the left hand side of the screen.  An X position of zero
* means the cursor is at the left-most position
*/
    return getcurx(stdscr);
}

int CursesIO::WhereY() {
/* This function returns the Y cursor position, as the line number from
* the top of the logical window.  The offset due to the protected top
* of the screen display is taken into account.  A WhereY() of zero means
* the cursor is at the top-most position it can be at.
*/
    return getcury(stdscr);
}

void CursesIO::LocalLf() {
/* This function performs a linefeed to the screen (but not remotely) by
* either moving the cursor down one line, or scrolling the logical screen
* up one line.
*/
    // TODO(rushfan): Implement me
#if 0
	if ( WhereY() >= GetScreenBottom() ) {
        scroll(stdscr);
	} else {
		m_cursorPosition.Y++;
		SetConsoleCursorPosition(m_hConOut,m_cursorPosition);
	}
#endif
}

/**
 * Returns the local cursor to the left-most position on the screen.
 */
void CursesIO::LocalCr() {
    int y = WhereY();
    LocalGotoXY(0, y);
}

/**
 * Clears the local logical screen
 */
void CursesIO::LocalCls() {
    attron(COLOR_PAIR(7));
    clear();
    refresh();
	LocalGotoXY(0, 0);
}

void CursesIO::LocalBackspace() {
    // TODO(rushfan): Inline into only caller in WLocalIO.
}

static void SetCursesAttribute() {
    unsigned long attr = COLOR_PAIR(curatr);
    unsigned long f = curatr & 0xff00;
    if (f > 7) {
        attr |= A_BOLD;
    }
    attron(attr);
}

void CursesIO::LocalPutchRaw(unsigned char ch) {
    SetCursesAttribute();
    addch(ch);
    refresh();
}

/**
 * This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void CursesIO::LocalPutch( unsigned char ch ) {
    SetCursesAttribute();
    addch(ch);
    refresh();
}

void CursesIO::LocalPuts( const char *pszText ) {
    SetCursesAttribute();
    if (strlen(pszText) == 2) {
        if (pszText[0] == '\r' && pszText[1] == '\n') {
            LocalGotoXY(0, WhereY() + 1);
            return;
        }
    }
    addstr(pszText);
    refresh();
}

void CursesIO::LocalXYPuts( int x, int y, const char *pszText ) {
    LocalGotoXY(x, y);
    LocalPuts(pszText);
}

int CursesIO::getchd() {
    return getch();
}

void CursesIO::LocalClrEol() {
    SetCursesAttribute();
    clrtoeol();
    refresh();
}

void CursesIO::LocalScrollScreen(int nTop, int nBottom, int nDirection) {
    //TODO(rushfan): Implement if needed for compile.cpp
}
