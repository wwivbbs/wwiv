/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#include <iostream>
#include <stdarg.h>
#include <string>

#include "platform/wlocal_io.h"
#include "platform/wutil.h"
#include "wconstants.h"

const int WLocalIO::cursorNone      = 0;
const int WLocalIO::cursorNormal    = 1;
const int WLocalIO::cursorSolid     = 2;

const int WLocalIO::topdataNone     = 0;
const int WLocalIO::topdataSystem   = 1;
const int WLocalIO::topdataUser     = 2;

const int WLocalIO::scrollUp = 0;
const int WLocalIO::scrollDown = 1;

static bool x_only = false;
static int curatr = 7;
static char endofline[81];

WLocalIO::WLocalIO() {
	// These 2 lines must remain in here.
	ExtendedKeyWaiting = 0;
	wx = 0;
	//m_nWfcStatus = 0;

	// TODO (for kwalker) Add Linux platform specific console maniuplation stuff
}


WLocalIO::WLocalIO( const WLocalIO& copy ) {
	printf("OOPS! - WLocalIO Copy Constructor called!\r\n" );
}


WLocalIO::~WLocalIO() {
}


void WLocalIO::set_global_handle( bool bOpenFile, bool bOnlyUpdateVariable ) {
}


void WLocalIO::global_char(char ch) {
}

void WLocalIO::set_x_only(int tf, const char *pszFileName, int ovwr) {
}


/*
 * This, obviously, moves the cursor to the location specified, offset from
 * the protected dispaly at the top of the screen.  Note: this function
 * is 0 based, so (0,0) is the upper left hand corner.
 */
void WLocalIO::LocalGotoXY(int x, int y) {
	x = std::max<int>( x, 0 );
	x = std::min<int>( x, 79 );
	y = std::max<int>( y, 0 );
	y += GetTopLine();
	y = std::min<int>( y, GetScreenBottom() );

	if (x_only) {
		wx = x;
		return;
	}
	m_cursorPositionX = static_cast< short > ( x );
	m_cursorPositionY = static_cast< short > ( y );

	std::cout << "\x1b[" << y << ";" << x << "H";
}




/* This function returns the current X cursor position, as the number of
 * characters from the left hand side of the screen.  An X position of zero
 * means the cursor is at the left-most position
 */
int WLocalIO::WhereX() {
	if (x_only) {
		return( wx );
	}
	return m_cursorPositionX;
}



/* This function returns the Y cursor position, as the line number from
 * the top of the logical window.  The offset due to the protected top
 * of the screen display is taken into account.  A WhereY() of zero means
 * the cursor is at the top-most position it can be at.
 */
int WLocalIO::WhereY() {
	return m_cursorPositionY;
}



/* This function performs a linefeed to the screen (but not remotely) by
 * either moving the cursor down one line, or scrolling the logical screen
 * up one line.
 */
void WLocalIO::LocalLf() {
	std::cout << "\n";
	m_cursorPositionY++;
	if(m_cursorPositionY > 24) {
		m_cursorPositionY = 24;
	}
}



/* This short function returns the local cursor to the left-most position
 * on the screen.
 */
void WLocalIO::LocalCr() {
	std::cout << "\r";
	m_cursorPositionX = 0;
}

/*
 * This clears the local logical screen
 */
void WLocalIO::LocalCls() {
	std::cout << "\x1b[2J";
	m_cursorPositionX = 0;
	m_cursorPositionY = 0;
}



/* This function moves the cursor one position to the left, or if the cursor
 * is currently at its left-most position, the cursor is moved to the end of
 * the previous line, except if it is on the top line, in which case nothing
 * happens.
 */
void WLocalIO::LocalBackspace() {
	std::cout << "\b";
	if( m_cursorPositionX >= 0 ) {
		m_cursorPositionX--;
	} else if( m_cursorPositionY != GetTopLine() ) {
		m_cursorPositionX = 79;
		m_cursorPositionY--;
	}
}



/* This function outputs one character to the screen, then updates the
 * cursor position accordingly, scolling the screen if necessary.  Not that
 * this function performs no commands such as a C/R or L/F.  If a value of
 * 8, 7, 13, 10, 12 (BACKSPACE, BEEP, C/R, L/F, TOF), or any other command-
 * type characters are passed, the appropriate corresponding "graphics"
 * symbol will be output to the screen as a normal character.
 */
void WLocalIO::LocalPutchRaw(unsigned char ch) {
	std::cout << ch;
	if(m_cursorPositionX <= 79) {
		m_cursorPositionX++;
		return;
	}
	m_cursorPositionX = 0;
	if(m_cursorPositionY != GetScreenBottom()) {
		m_cursorPositionY++;
	}
}




/* This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void WLocalIO::LocalPutch(unsigned char ch) {
	if ( x_only ) {
		if ( ch > 31 ) {
			wx = ( wx + 1 ) % 80;
		} else if ( ch == RETURN || ch == CL ) {
			wx = 0;
		} else if ( ch == BACKSPACE ) {
			if ( wx ) {
				wx--;
			}
		}
		return;
	}
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
	  // Bell
	}
}


/*
 * This (obviously) outputs a string TO THE SCREEN ONLY
 */
void WLocalIO::LocalPuts(const char *s) {
	while (*s) {
		LocalPutch(*s++);
	}
}


void WLocalIO::LocalXYPuts( int x, int y, const char *pszText ) {
	LocalGotoXY( x, y );
	LocalFastPuts( pszText );
}


/*
 * This RAPIDLY outputs ONE LINE to the screen only
 */
void WLocalIO::LocalFastPuts(const char *s) {
	m_cursorPositionX += strlen( s );
	m_cursorPositionX %= 80;

	// TODO: set current attributes

	std::cout << s;
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

	int nOldColor = curatr;
	curatr = nAttribute;
	LocalXYPuts( x, y, szBuffer );
	curatr = nOldColor;
	return nNumWritten;
}



/*
 * set_protect sets the number of lines protected at the top of the screen.
 */
void WLocalIO::set_protect(int l) {
}


void WLocalIO::savescreen() {
}


/*
 * restorescreen restores a screen previously saved with savescreen
 */
void WLocalIO::restorescreen() {
}


void WLocalIO::ExecuteTemporaryCommand( const char *pszCommand ) {
}


char xlate[] = {
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, 0, 0, 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, 0, 0, 0, 0,
	'Z', 'X', 'C', 'V', 'B', 'N', 'M',
};


char WLocalIO::scan_to_char( int nKeyCode ) {
	return ( nKeyCode >= 16 && nKeyCode <= 50 ) ? xlate[ nKeyCode - 16 ] : '\x00';
}


void WLocalIO::alt_key( int nKeyCode ) {
	// TODO: implement macro support
}


/*
 * skey handles all f-keys and the like hit FROM THE KEYBOARD ONLY
 */
void WLocalIO::skey(char ch) {
}


static const char * pszTopScrItems[] = {
	"Comm Disabled",
	"Temp Sysop",
	"Capture",
	"Alert",
	"ÕÕÕÕÕÕÕ",
	"Available",
	"ÕÕÕÕÕÕÕÕÕÕÕ",
	"%s chatting with %s"
};

void WLocalIO::tleft(bool bCheckForTimeOut) {
}


/****************************************************************************/


/**
 * IsLocalKeyPressed - returns whether or not a key been pressed at the local console.
 *
 * @return true if a key has been pressed at the local console, false otherwise
 */
bool WLocalIO::LocalKeyPressed() {
	return false;
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
	return 0;
}


void WLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {
	*cl = 0;
	*atr= 0;
	*cc = static_cast<char>( curatr );
	strcpy( xl, endofline );
}


/**
 * LocalGetChar - gets character entered at local console.
 *                <B>Note: This is a blocking function call.</B>
 *
 * @return int value of key entered
 */

int  WLocalIO::LocalGetChar() {
	return getchar();
}


void WLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
	x=x;
	y=y;
	xlen=xlen;
	ylen=ylen;
}


void WLocalIO::SetCursor(int cursorStyle) {
}



void WLocalIO::LocalClrEol() {
}


void WLocalIO::LocalWriteScreenBuffer(const char *pszBuffer) {
	pszBuffer = pszBuffer; // No warning
}


int WLocalIO::GetDefaultScreenBottom() {
	return 25;
}


/**
 * Edits a string, doing local screen I/O only.
 */
void WLocalIO::LocalEditLine( char *s, int len, int status, int *returncode, char *ss ) {
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

void WLocalIO::UpdateTopScreen( WStatus* pStatus, WSession *pSession, int nInstanceNumber ) {
}

void WLocalIO::LocalScrollScreen(int nTop, int nBottom, int nDirection) {
  // TODO(rushfan): Implement me
}
