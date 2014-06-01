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
#include "wstringutils.h"

//
// local functions
//

bool HasKeyBeenPressed();
unsigned char GetKeyboardChar();
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
	lines_listed = 0;
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
	} else if ( ch == CG ) {
		if ( !outcom ) {
			// TODO Make the bell sound configurable.
			//WWIV_Sound( 500, 4 );
		}
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
	LocalFastPuts( pszText );
}


void WLocalIO::LocalFastPuts( const char *pszText )
// This RAPIDLY outputs ONE LINE to the screen only and is not exactly stable.
{
	DWORD cb = 0;
	int len = strlen( pszText );

	SetConsoleTextAttribute( m_hConOut, static_cast< short > ( curatr ) );
	WriteConsole( m_hConOut, pszText, len, &cb, NULL );
	m_cursorPosition.X = m_cursorPosition.X + static_cast< short >( cb );
}


int  WLocalIO::LocalPrintf( const char *pszFormattedText, ... ) {
	va_list ap;
	char szBuffer[ 1024 ];

	va_start( ap, pszFormattedText );
	int nNumWritten = vsnprintf( szBuffer, sizeof( szBuffer ), pszFormattedText, ap );
	va_end( ap );
	LocalFastPuts( szBuffer );
	return nNumWritten;
}


int  WLocalIO::LocalXYPrintf( int x, int y, const char *pszFormattedText, ... ) {
	va_list ap;
	char szBuffer[ 1024 ];

	va_start( ap, pszFormattedText );
	int nNumWritten = vsnprintf( szBuffer, sizeof( szBuffer ), pszFormattedText, ap );
	va_end( ap );
	LocalXYPuts( x, y, szBuffer );
	return nNumWritten;
}


int  WLocalIO::LocalXYAPrintf( int x, int y, int nAttribute, const char *pszFormattedText, ... ) {
	va_list ap;
	char szBuffer[ 1024 ];

	va_start( ap, pszFormattedText );
	int nNumWritten = vsnprintf( szBuffer, sizeof( szBuffer ), pszFormattedText, ap );
	va_end( ap );

	// GetSession()->bout.SystemColor( nAttribute );
	int nOldColor = curatr;
	curatr = nAttribute;
	LocalXYPuts( x, y, szBuffer );
	curatr = nOldColor;
	return nNumWritten;
}

/****************************************************************************/
/*
* returns the ASCII code of the next character waiting in the
* keyboard buffer.  If there are no characters waiting in the
* keyboard buffer, then it waits for one.
*
* A value of 0 is returned for all extended keys (such as F1,
* Alt-X, etc.).  The function must be called again upon receiving
* a value of 0 to obtain the value of the extended key pressed.
*/
unsigned char WLocalIO::getchd() {
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

void WLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {
	*cc = static_cast<char>( curatr );
	strcpy(xl, endofline);
	{
		WORD Attr[80];
		CONSOLE_SCREEN_BUFFER_INFO ConInfo;
		GetConsoleScreenBufferInfo(m_hConOut,&ConInfo);

		int len = ConInfo.dwCursorPosition.X;
		ConInfo.dwCursorPosition.X = 0;
		DWORD cb;
		ReadConsoleOutputCharacter(m_hConOut,cl,len,ConInfo.dwCursorPosition,&cb);
		ReadConsoleOutputAttribute(m_hConOut,Attr,len,ConInfo.dwCursorPosition,&cb);

		for (int i = 0; i < len; i++) {
			atr[i] = static_cast<char>( Attr[i] ); // atr is 8bit char, Attr is 16bit
		}
	}
	cl[ WhereX() ]	= 0;
	atr[ WhereX() ] = 0;
}


/**
 * LocalGetChar - gets character entered at local console.
 *                <B>Note: This is a blocking function call.</B>
 *
 * @return int value of key entered
 */
int  WLocalIO::LocalGetChar() {
	return GetKeyboardChar();
}


void WLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
	// Make sure that we are within the range of {(0,0), (80,GetScreenBottom())}
	xlen = std::min<int>( xlen, 80 );
	if ( ylen > ( GetScreenBottom() + 1 - GetTopLine() ) ) {
		ylen = ( GetScreenBottom() + 1 - GetTopLine() );
	}
	if ((x + xlen) > 80) {
		x = 80 - xlen;
	}
	if ((y + ylen) > GetScreenBottom() + 1) {
		y = GetScreenBottom() + 1 - ylen;
	}

	int xx = WhereX();
	int yy = WhereY();

	// we expect to be offset by GetTopLine()
	y += GetTopLine();

	// large enough to hold 80x50
	CHAR_INFO ci[4000];

	// pos is the position within the buffer
	COORD pos = {0, 0};
	COORD size;
	SMALL_RECT rect;

	// rect is the area on screen the buffer is to be drawn
	rect.Top    = static_cast< short > ( y );
	rect.Left   = static_cast< short > ( x );
	rect.Right  = static_cast< short > ( xlen + x - 1 );
	rect.Bottom = static_cast< short > ( ylen + y - 1 );

	// size of the buffer to use (lower right hand coordinate)
	size.X      = static_cast< short > ( xlen );
	size.Y      = static_cast< short > ( ylen );

	// our current position within the CHAR_INFO buffer
	int nCiPtr  = 0;

	//
	// Loop through Y, each time looping through X adding the right character
	//

	for (int yloop = 0; yloop<size.Y; yloop++) {
		for (int xloop=0; xloop<size.X; xloop++) {
			ci[nCiPtr].Attributes = static_cast< short > ( curatr );
			if ((yloop==0) || (yloop==size.Y-1)) {
				ci[nCiPtr].Char.AsciiChar   = '\xC4';      // top and bottom
			} else {
				if ((xloop==0) || (xloop==size.X-1)) {
					ci[nCiPtr].Char.AsciiChar   = '\xB3';  // right and left sides
				} else {
					ci[nCiPtr].Char.AsciiChar   = '\x20';   // nothing... Just filler (space)
				}
			}
			nCiPtr++;
		}
	}

	//
	// sum of the lengths of the previous lines (+0) is the start of next line
	//

	ci[0].Char.AsciiChar                    = '\xDA';      // upper left
	ci[xlen-1].Char.AsciiChar               = '\xBF';      // upper right

	ci[xlen*(ylen-1)].Char.AsciiChar        = '\xC0';      // lower left
	ci[xlen*(ylen-1)+xlen-1].Char.AsciiChar = '\xD9';      // lower right

	//
	// Send it all to the screen with 1 WIN32 API call (Windows 95's console mode API support
	// is MUCH slower than NT/Win2k, therefore it is MUCH faster to render the buffer off
	// screen and then write it with one fell swoop.
	//

	WriteConsoleOutput(m_hConOut, ci, size, pos, &rect);

	//
	// Draw shadow around boxed window
	//
	for (int i = 0; i < xlen; i++) {
		set_attr_xy(x + 1 + i, y + ylen, 0x08);
	}

	for (int i1 = 0; i1 < ylen; i1++) {
		set_attr_xy(x + xlen, y + 1 + i1, 0x08);
	}

	LocalGotoXY(xx, yy);

}


void WLocalIO::SetCursor(int cursorStyle) {
	CONSOLE_CURSOR_INFO cursInfo;

	switch (cursorStyle) {
	case WLocalIO::cursorNone: {
		cursInfo.dwSize = 20;
		cursInfo.bVisible = false;
		SetConsoleCursorInfo(m_hConOut, &cursInfo);
	}
	break;
	case WLocalIO::cursorSolid: {
		cursInfo.dwSize = 100;
		cursInfo.bVisible = true;
		SetConsoleCursorInfo(m_hConOut, &cursInfo);
	}
	break;
	case WLocalIO::cursorNormal:
	default: {
		cursInfo.dwSize = 20;
		cursInfo.bVisible = true;
		SetConsoleCursorInfo(m_hConOut, &cursInfo);
	}
	}
}

void WLocalIO::LocalClrEol() {
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	DWORD cb;
	int len = 80 - WhereX();

	GetConsoleScreenBufferInfo( m_hConOut,&ConInfo );
	FillConsoleOutputCharacter( m_hConOut, ' ', len, ConInfo.dwCursorPosition, &cb );
	FillConsoleOutputAttribute( m_hConOut, (WORD) curatr, len, ConInfo.dwCursorPosition, &cb );
}

void WLocalIO::LocalWriteScreenBuffer( const char *pszBuffer ) {
	CHAR_INFO ci[2000];

	COORD pos       = { 0, 0};
	COORD size      = { 80, 25 };
	SMALL_RECT rect = { 0, 0, 79, 24 };

	for(int i=0; i<2000; i++) {
		ci[i].Char.AsciiChar = (char) *(pszBuffer + ((i*2)+0));
		ci[i].Attributes     = (unsigned char) *(pszBuffer + ((i*2)+1));
	}
	WriteConsoleOutput( m_hConOut, ci, size, pos, &rect );
}


int WLocalIO::GetDefaultScreenBottom() {
	return ( m_consoleBufferInfo.dwSize.Y - 1 );
}

bool HasKeyBeenPressed() {
	return ( _kbhit() ) ? true : false;
}

unsigned char GetKeyboardChar() {
	return static_cast< unsigned char >( _getch() );
}


void WLocalIO::LocalEditLine( char *pszInOutText, int len, int status, int *returncode, char *pszAllowedSet ) {
	WWIV_ASSERT( pszInOutText );
	WWIV_ASSERT( pszAllowedSet );

	int oldatr = curatr;
	int cx = WhereX();
	int cy = WhereY();
	for ( int i = strlen( pszInOutText ); i < len; i++ ) {
		pszInOutText[i] = static_cast<unsigned char>( 176 );
	}
	pszInOutText[len] = '\0';
	curatr = GetSession()->GetEditLineColor();
	LocalFastPuts( pszInOutText );
	LocalGotoXY( cx, cy );
	bool done = false;
	int pos = 0;
	bool insert = false;
	do {
		unsigned char ch = getchd();
		if ( ch == 0 || ch == 224 ) {
			ch = getchd();
			switch ( ch ) {
			case F1:
				done = true;
				*returncode = DONE;
				break;
			case HOME:
				pos = 0;
				LocalGotoXY( cx, cy );
				break;
			case END:
				pos = GetEditLineStringLength( pszInOutText );
				LocalGotoXY( cx + pos, cy );
				break;
			case RARROW:
				if ( pos < GetEditLineStringLength( pszInOutText ) ) {
					pos++;
					LocalGotoXY( cx + pos, cy );
				}
				break;
			case LARROW:
				if ( pos > 0 ) {
					pos--;
					LocalGotoXY( cx + pos, cy );
				}
				break;
			case UARROW:
			case CO:
				done = true;
				*returncode = PREV;
				break;
			case DARROW:
				done = true;
				*returncode = NEXT;
				break;
			case INSERT:
				if (status != SET) {
					insert = !insert;
				}
				break;
			case KEY_DELETE:
				if ( status != SET ) {
					for ( int i = pos; i < len; i++ ) {
						pszInOutText[ i ] = pszInOutText[ i + 1 ];
					}
					pszInOutText[ len - 1 ] = static_cast<unsigned char>( 176 );
					LocalXYPuts( cx, cy, pszInOutText );
					LocalGotoXY( cx + pos, cy );
				}
				break;
			}
		} else {
			if (ch > 31) {
				if (status == UPPER_ONLY) {
					ch = wwiv::UpperCase<unsigned char>(ch);
				}
				if (status == SET) {
					ch = wwiv::UpperCase<unsigned char>(ch);
					if ( ch != SPACE ) {
						bool bLookingForSpace = true;
						for ( int i = 0; i < len; i++ ) {
							if ( ch == pszAllowedSet[i] && bLookingForSpace ) {
								bLookingForSpace = false;
								pos = i;
								LocalGotoXY( cx + pos, cy );
								if ( pszInOutText[pos] == SPACE ) {
									ch = pszAllowedSet[pos];
								} else {
									ch = SPACE;
								}
							}
						}
						if ( bLookingForSpace ) {
							ch = pszAllowedSet[pos];
						}
					}
				}
				if ((pos < len) && ((status == ALL) || (status == UPPER_ONLY) || (status == SET) ||
				                    ((status == NUM_ONLY) && (((ch >= '0') && (ch <= '9')) || (ch == SPACE))))) {
					if ( insert ) {
						for ( int i = len - 1; i > pos; i-- ) {
							pszInOutText[i] = pszInOutText[i - 1];
						}
						pszInOutText[ pos++ ] = ch;
						LocalXYPuts( cx, cy, pszInOutText );
						LocalGotoXY( cx + pos, cy );
					} else {
						pszInOutText[pos++] = ch;
						LocalPutch( ch );
					}
				}
			} else {
				// ch is > 32
				switch (ch) {
				case RETURN:
				case TAB:
					done = true;
					*returncode = NEXT;
					break;
				case ESC:
					done = true;
					*returncode = ABORTED;
					break;
				case CA:
					pos = 0;
					LocalGotoXY( cx, cy );
					break;
				case CE:
					pos = GetEditLineStringLength( pszInOutText ); // len;
					LocalGotoXY( cx + pos, cy );
					break;
				case BACKSPACE:
					if ( pos > 0 ) {
						if ( insert ) {
							for ( int i = pos - 1; i < len; i++ ) {
								pszInOutText[i] = pszInOutText[i + 1];
							}
							pszInOutText[len - 1] = static_cast<unsigned char>( 176 );
							pos--;
							LocalXYPuts( cx, cy, pszInOutText );
							LocalGotoXY(cx + pos, cy);
						} else {
							int nStringLen = GetEditLineStringLength( pszInOutText );
							pos--;
							if ( pos == ( nStringLen - 1 ) ) {
								pszInOutText[ pos ] = static_cast<unsigned char>( 176 );
							} else {
								pszInOutText[ pos ] = SPACE;
							}
							LocalXYPuts( cx, cy, pszInOutText );
							LocalGotoXY( cx + pos, cy );
						}
					}
					break;
				}
			}
		}
	} while ( !done );

	int z = strlen( pszInOutText );
	while ( z >= 0 && static_cast<unsigned char>( pszInOutText[z-1] ) == 176 ) {
		--z;
	}
	pszInOutText[z] = '\0';

	char szFinishedString[ 260 ];
	_snprintf( szFinishedString, sizeof( szFinishedString ), "%-255s", pszInOutText );
	szFinishedString[ len ] = '\0';
	LocalGotoXY( cx, cy );
	curatr=oldatr;
	LocalFastPuts( szFinishedString );
	LocalGotoXY( cx, cy );
}


int WLocalIO::GetEditLineStringLength( const char *pszText ) {
	int i = strlen( pszText );
	while ( i >= 0 && ( /*pszText[i-1] == 32 ||*/ static_cast<unsigned char>( pszText[i-1] ) == 176 ) ) {
		--i;
	}
	return i;
}


void WLocalIO::UpdateNativeTitleBar() {
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
