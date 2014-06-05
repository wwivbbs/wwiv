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
#include "platform/wlocal_io.h"

//
// local functions
//

static unsigned char GetKeyboardChar();
static int GetTopLine() { return 0; }

/*
 * Sets screen attribute at screen pos x,y to attribute contained in a.
 */
void WLocalIO::set_attr_xy(int x, int y, int a) {
	COORD loc = {0};
	DWORD cb = {0};

	loc.X = static_cast<short>( x );
	loc.Y = static_cast<short>( y );

	WriteConsoleOutputAttribute( m_hConOut, reinterpret_cast< LPWORD >( &a ), 1, loc, &cb );
}

WLocalIO::WLocalIO() {
	ExtendedKeyWaiting = 0;

	m_hConOut = GetStdHandle( STD_OUTPUT_HANDLE );
	m_hConIn  = GetStdHandle( STD_INPUT_HANDLE );
	if ( m_hConOut == INVALID_HANDLE_VALUE ) {
		std::cout << "\n\nCan't get console handle!.\n\n";
		abort();
	}
	GetConsoleScreenBufferInfo( m_hConOut,&m_consoleBufferInfo );
	m_originalConsoleSize = m_consoleBufferInfo.dwSize;
	SMALL_RECT rect = m_consoleBufferInfo.srWindow;
	COORD bufSize;
	bufSize.X = static_cast<short>( rect.Right - rect.Left + 1 );
	bufSize.Y = static_cast<short>( rect.Bottom - rect.Top + 1 );
	bufSize.X = static_cast<short>( std::min<SHORT>( bufSize.X, 80 ) );
	bufSize.Y = static_cast<short>( std::min<SHORT>( bufSize.Y, 25 ) );
	SetConsoleWindowInfo( m_hConOut, TRUE, &rect );
	SetConsoleScreenBufferSize( m_hConOut, bufSize );

	m_cursorPosition.X = m_consoleBufferInfo.dwCursorPosition.X;
	m_cursorPosition.Y = m_consoleBufferInfo.dwCursorPosition.Y;

	// Have to reset this info, otherwise bad things happen.
	GetConsoleScreenBufferInfo( m_hConOut,&m_consoleBufferInfo );
	SetScreenBottom(m_consoleBufferInfo.dwSize.Y - 1);
}

WLocalIO::~WLocalIO() {
	SetConsoleScreenBufferSize( m_hConOut, m_originalConsoleSize );
	SetConsoleTextAttribute(m_hConOut, static_cast<short>(0x07));
}

void WLocalIO::LocalGotoXY(int x, int y)
// This, obviously, moves the cursor to the location specified, offset from
// the protected dispaly at the top of the screen.  Note: this function
// is 0 based, so (0,0) is the upper left hand corner.
{
	x = std::max<int>( x, 0 );
	x = std::min<int>( x, 79 );
	y = std::max<int>( y, 0 );
	y += GetTopLine();
	y = std::min<int>( y, GetScreenBottom() );

	m_cursorPosition.X = static_cast< short > ( x );
	m_cursorPosition.Y = static_cast< short > ( y );
	SetConsoleCursorPosition(m_hConOut,m_cursorPosition);
}




int WLocalIO::WhereX()
/* This function returns the current X cursor position, as the number of
* characters from the left hand side of the screen.  An X position of zero
* means the cursor is at the left-most position
*/
{
	CONSOLE_SCREEN_BUFFER_INFO m_consoleBufferInfo;
	GetConsoleScreenBufferInfo(m_hConOut,&m_consoleBufferInfo);

	m_cursorPosition.X = m_consoleBufferInfo.dwCursorPosition.X;
	m_cursorPosition.Y = m_consoleBufferInfo.dwCursorPosition.Y;

	return m_cursorPosition.X;
}



int WLocalIO::WhereY()
/* This function returns the Y cursor position, as the line number from
* the top of the logical window.  The offset due to the protected top
* of the screen display is taken into account.  A WhereY() of zero means
* the cursor is at the top-most position it can be at.
*/
{
	CONSOLE_SCREEN_BUFFER_INFO m_consoleBufferInfo;

	GetConsoleScreenBufferInfo(m_hConOut,&m_consoleBufferInfo);

	m_cursorPosition.X = m_consoleBufferInfo.dwCursorPosition.X;
	m_cursorPosition.Y = m_consoleBufferInfo.dwCursorPosition.Y;

	return m_cursorPosition.Y - GetTopLine();
}



void WLocalIO::LocalLf()
/* This function performs a linefeed to the screen (but not remotely) by
* either moving the cursor down one line, or scrolling the logical screen
* up one line.
*/
{
	SMALL_RECT scrollRect;
	COORD dest;
	CHAR_INFO fill;

	if ( m_cursorPosition.Y >= GetScreenBottom() ) {
		dest.X = 0;
		dest.Y = static_cast< short > ( GetTopLine() );
		scrollRect.Top = static_cast< short > ( GetTopLine() + 1 );
		scrollRect.Bottom = static_cast< short > ( GetScreenBottom() );
		scrollRect.Left = 0;
		scrollRect.Right = 79;
		fill.Attributes = static_cast< short > ( curatr );
		fill.Char.AsciiChar = ' ';

		ScrollConsoleScreenBuffer(m_hConOut, &scrollRect, NULL, dest, &fill);
	} else {
		m_cursorPosition.Y++;
		SetConsoleCursorPosition(m_hConOut,m_cursorPosition);
	}
}



/**
 * Returns the local cursor to the left-most position on the screen.
 */
void WLocalIO::LocalCr() {
	m_cursorPosition.X = 0;
	SetConsoleCursorPosition(m_hConOut,m_cursorPosition);
}


/**
 * Clears the local logical screen
 */
void WLocalIO::LocalCls() {
	int nOldCurrentAttribute = curatr;
	curatr = 0x07;
	SMALL_RECT scrollRect;
	COORD dest;
	CHAR_INFO fill;

	dest.X = 32767;
	dest.Y = 0;
	scrollRect.Top = static_cast< short > ( GetTopLine() );
	scrollRect.Bottom = static_cast< short > ( GetScreenBottom() );
	scrollRect.Left = 0;
	scrollRect.Right = 79;
	fill.Attributes = static_cast< short > ( curatr );
	fill.Char.AsciiChar = ' ';

	ScrollConsoleScreenBuffer(m_hConOut, &scrollRect, NULL, dest, &fill);

	LocalGotoXY(0, 0);
	curatr = nOldCurrentAttribute;
}



void WLocalIO::LocalBackspace()
/* This function moves the cursor one position to the left, or if the cursor
* is currently at its left-most position, the cursor is moved to the end of
* the previous line, except if it is on the top line, in which case nothing
* happens.
*/
{
	if (m_cursorPosition.X >= 0) {
		m_cursorPosition.X--;
	} else if ( m_cursorPosition.Y != GetTopLine() ) {
		m_cursorPosition.Y--;
		m_cursorPosition.X = 79;
	}
	SetConsoleCursorPosition( m_hConOut,m_cursorPosition );
}



void WLocalIO::LocalPutchRaw(unsigned char ch)
/* This function outputs one character to the screen, then updates the
* cursor position accordingly, scolling the screen if necessary.  Not that
* this function performs no commands such as a C/R or L/F.  If a value of
* 8, 7, 13, 10, 12 (BACKSPACE, BEEP, C/R, L/F, TOF), or any other command-
* type characters are passed, the appropriate corresponding "graphics"
* symbol will be output to the screen as a normal character.
*/
{
	DWORD cb;

	SetConsoleTextAttribute( m_hConOut, static_cast< short > ( curatr ) );
	WriteConsole( m_hConOut, &ch, 1, &cb,NULL );

	if (m_cursorPosition.X <= 79) {
		m_cursorPosition.X++;
		return;
	}

	// Need to scroll the screen up one.
	m_cursorPosition.X = 0;
	if ( m_cursorPosition.Y == GetScreenBottom() ) {
		COORD dest;
		SMALL_RECT MoveRect;
		CHAR_INFO fill;

		// rushfan scrolling fix (was no +1)
		MoveRect.Top    = static_cast< short > ( GetTopLine() + 1 );
		MoveRect.Bottom = static_cast< short > ( GetScreenBottom() );
		MoveRect.Left   = 0;
		MoveRect.Right  = 79;

		fill.Attributes = static_cast< short > ( curatr );
		fill.Char.AsciiChar = ' ';

		dest.X = 0;
		// rushfan scrolling fix (was -1)
		dest.Y = static_cast< short > ( GetTopLine() );

		ScrollConsoleScreenBuffer(m_hConOut,&MoveRect,&MoveRect,dest,&fill);
	} else {
		m_cursorPosition.Y++;
	}
}


/**
 * This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void WLocalIO::LocalPutch( unsigned char ch ) {
	if ( ch > 31 ) {
		LocalPutchRaw(ch);
	} else if ( ch == CM ) {
		LocalCr();
	} else if ( ch == CJ ) {
		LocalLf();
	} else if ( ch == CL ) {
		LocalCls();
	} else if ( ch == BACKSPACE ) {
		LocalBackspace();
	}
}


void WLocalIO::LocalPuts( const char *pszText )
// This (obviously) outputs a string TO THE SCREEN ONLY
{
	while ( *pszText ) {
		LocalPutch( *pszText++ );
	}
}


void WLocalIO::LocalXYPuts( int x, int y, const char *pszText ) {
	LocalGotoXY( x, y );
	LocalPuts( pszText );
}

/*
* returns the ASCII code of the next character waiting in the
* keyboard buffer.  If there are no characters waiting in the
* keyboard buffer, then it waits for one.
*
* A value of 0 is returned for all extended keys (such as F1,
* Alt-X, etc.).  The function must be called again upon receiving
* a value of 0 to obtain the value of the extended key pressed.
*/
int WLocalIO::getchd() {
	if (ExtendedKeyWaiting) {
		ExtendedKeyWaiting = 0;
		return GetKeyboardChar();
	}
	unsigned char rc = GetKeyboardChar();
	if ( rc == 0 || rc == 0xe0 ) {
		ExtendedKeyWaiting = 1;
	}
	return rc;
}

void WLocalIO::LocalClrEol() {
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	DWORD cb;
	int len = 80 - WhereX();

	GetConsoleScreenBufferInfo( m_hConOut,&ConInfo );
	FillConsoleOutputCharacter( m_hConOut, ' ', len, ConInfo.dwCursorPosition, &cb );
	FillConsoleOutputAttribute( m_hConOut, (WORD) curatr, len, ConInfo.dwCursorPosition, &cb );
}

unsigned char GetKeyboardChar() {
	return static_cast< unsigned char >( _getch() );
}

void WLocalIO::LocalScrollScreen(int nTop, int nBottom, int nDirection)
{
	SMALL_RECT scrollRect;
	COORD dest;
	CHAR_INFO fill;

	dest.X = 0;
	if (nDirection == WLocalIO::scrollUp)
	{
		dest.Y = static_cast< short > (nTop);
		scrollRect.Top = static_cast< short > (nTop + 1);
	}
	else if (nDirection == WLocalIO::scrollDown)
	{
		dest.Y = static_cast< short > (nTop + 1);
		scrollRect.Top = static_cast< short > (nTop);
	}
	scrollRect.Bottom = static_cast< short > (nBottom);
	scrollRect.Left = 0;
	scrollRect.Right = 79;
	fill.Attributes = static_cast< short > (7);
	fill.Char.AsciiChar = ' ';

	ScrollConsoleScreenBuffer(m_hConOut, &scrollRect, NULL, dest, &fill);
}
