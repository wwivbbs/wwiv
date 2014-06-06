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


// local impl of _getch
unsigned char _getch();


static int curatr = 7;


WLocalIO::WLocalIO() {
	// These 2 lines must remain in here.
	ExtendedKeyWaiting = 0;
}


WLocalIO::WLocalIO( const WLocalIO& copy ) {
	printf("OOPS! - WLocalIO Copy Constructor called!\r\n" );
}


WLocalIO::~WLocalIO() {
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
	y = std::min<int>( y, GetScreenBottom() );

	m_cursorPositionX = static_cast< short > ( x );
	m_cursorPositionY = static_cast< short > ( y );

	std::cout << "\x1b[" << y << ";" << x << "H";
}




/* This function returns the current X cursor position, as the number of
 * characters from the left hand side of the screen.  An X position of zero
 * means the cursor is at the left-most position
 */
int WLocalIO::WhereX() {
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
	} else if (m_cursorPositionY != 0) {
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
	LocalPuts( pszText );
}

int WLocalIO::getchd() {
  return _getch();
}

void WLocalIO::LocalClrEol() {
}


void WLocalIO::LocalScrollScreen(int nTop, int nBottom, int nDirection) {
  // TODO(rushfan): Implement me
}
