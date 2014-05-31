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
#include <algorithm>
#include <conio.h>

#ifndef NOT_BBS
#include "wwiv.h"
#include "platform/wutil.h"
#endif

extern int oldy = 0;

const int WLocalIO::cursorNone      = 0;
const int WLocalIO::cursorNormal    = 1;
const int WLocalIO::cursorSolid     = 2;

const int WLocalIO::topdataNone     = 0;
const int WLocalIO::topdataSystem   = 1;
const int WLocalIO::topdataUser     = 2;

//
// local functions
//

bool HasKeyBeenPressed();
unsigned char GetKeyboardChar();

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
	SetTopLine( 0 );
	SetScreenBottom( 0 );
	ExtendedKeyWaiting = 0;
	wx = 0;

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
}


WLocalIO::~WLocalIO() {
	SetConsoleScreenBufferSize( m_hConOut, m_originalConsoleSize );
	SetConsoleTextAttribute(m_hConOut, 0x07);
}


void WLocalIO::set_global_handle(bool bOpenFile, bool bOnlyUpdateVariable ) {
	if ( x_only ) {
		return;
	}

	if ( bOpenFile ) {
		if ( !fileGlobalCap.IsOpen() ) {
			char szFileName[MAX_PATH];
			_snprintf(szFileName, sizeof( szFileName ), "%sglobal-%d.txt", syscfg.gfilesdir, GetApplication()->GetInstanceNumber() );
			fileGlobalCap.SetName( szFileName );

			bool bOpen = fileGlobalCap.Open( WFile::modeBinary | WFile::modeAppend | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
			global_ptr = 0;
			global_buf = static_cast<char *>( BbsAllocA( GLOBAL_SIZE ) );
			if ( !bOpen || !global_buf ) {
				if (global_buf) {
					BbsFreeMemory(global_buf);
					global_buf = NULL;
				}
			}
		}
	} else {
		if ( fileGlobalCap.IsOpen() && !bOnlyUpdateVariable ) {
			fileGlobalCap.Write( global_buf, global_ptr );
			fileGlobalCap.Close();
			if (global_buf) {
				BbsFreeMemory(global_buf);
				global_buf = NULL;
			}
		}
	}
}


void WLocalIO::global_char(char ch) {
	if ( global_buf && fileGlobalCap.IsOpen() ) {
		global_buf[global_ptr++] = ch;
		if (global_ptr == GLOBAL_SIZE) {
			fileGlobalCap.Write( global_buf, global_ptr );
			global_ptr = 0;
		}
	}
}

void WLocalIO::set_x_only(int tf, const char *pszFileName, int ovwr) {
	static bool nOldGlobalHandle;

	if (x_only) {
		if (!tf) {
			if ( fileGlobalCap.IsOpen() ) {
				fileGlobalCap.Write( global_buf, global_ptr );
				fileGlobalCap.Close();
				if (global_buf) {
					BbsFreeMemory(global_buf);
					global_buf = NULL;
				}
			}
			x_only = false;
			set_global_handle( ( nOldGlobalHandle ) ? true : false );
			nOldGlobalHandle = false;
			express = expressabort = false;
		}
	} else {
		if (tf) {
			nOldGlobalHandle = fileGlobalCap.IsOpen();
			set_global_handle( false );
			x_only = true;
			wx = 0;
			fileGlobalCap.SetName( syscfgovr.tempdir, pszFileName );

			if (ovwr) {
				fileGlobalCap.Open( WFile::modeBinary|WFile::modeText|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
			} else {
				fileGlobalCap.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeAppend|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
			}
			global_ptr = 0;
			express = true;
			expressabort = false;
			global_buf = static_cast<char *>( BbsAllocA(GLOBAL_SIZE) );
			if ( !fileGlobalCap.IsOpen() || !global_buf ) {
				if (global_buf) {
					BbsFreeMemory(global_buf);
					global_buf = NULL;
				}
				set_x_only(0, NULL, 0);
			}
		}
	}
	timelastchar1 = timer1();
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

	if (x_only) {
		wx = x;
		return;
	}
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
	if (x_only) {
		return( wx );
	}

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
		if ( !outcom ) {
			// TODO Make the bell sound configurable.
			WWIV_Sound( 500, 4 );
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


void WLocalIO::set_protect(int l) //JZ Set_Protect Fix
// set_protect sets the number of lines protected at the top of the screen.
{
	if ( l != GetTopLine() ) {
		COORD coord;
		coord.X = 0;
		coord.Y = static_cast< short > ( l );

		if (l > GetTopLine()) {
			if ( ( WhereY() + GetTopLine() - l ) < 0 ) {
				CHAR_INFO lpFill;
				SMALL_RECT scrnl;

				scrnl.Top = static_cast< short > ( GetTopLine() );
				scrnl.Left = 0;
				scrnl.Bottom = static_cast< short > ( GetScreenBottom() );
				scrnl.Right = 79; //%%TODO - JZ Make the console size user defined

				lpFill.Char.AsciiChar = ' ';
				lpFill.Attributes = 0;

				coord.X = 0;
				coord.Y = static_cast< short > ( l );
				ScrollConsoleScreenBuffer(m_hConOut, &scrnl, NULL, coord, &lpFill);
				LocalGotoXY( WhereX(), WhereY() + l - GetTopLine() );
			}
			oldy += (GetTopLine() - l);
		} else {
			DWORD written;
			FillConsoleOutputAttribute(m_hConOut,0,(GetTopLine() - l) * 80,coord,&written);
			oldy += (GetTopLine() - l);
		}
	}
	SetTopLine( l );
	GetSession()->screenlinest = ( GetSession()->using_modem ) ? GetSession()->GetCurrentUser()->GetScreenLines() : defscreenbottom + 1 - GetTopLine();
}


void WLocalIO::savescreen() {
	COORD topleft;
	CONSOLE_SCREEN_BUFFER_INFO bufinfo;
	SMALL_RECT region;

	GetConsoleScreenBufferInfo(m_hConOut,&bufinfo);
	topleft.Y = topleft.X = region.Top = region.Left = 0;
	region.Bottom = static_cast< short > ( bufinfo.dwSize.Y - 1 );
	region.Right  = static_cast< short > ( bufinfo.dwSize.X - 1 );

	if (!m_ScreenSave.scrn1) {
		m_ScreenSave.scrn1= static_cast< CHAR_INFO *> ( bbsmalloc((bufinfo.dwSize.X*bufinfo.dwSize.Y)*sizeof(CHAR_INFO)) );
	}

	if (m_ScreenSave.scrn1) {
		ReadConsoleOutput(m_hConOut,(CHAR_INFO *)m_ScreenSave.scrn1,bufinfo.dwSize,topleft,&region);
	}

	m_ScreenSave.x1 = static_cast< short > ( WhereX() );
	m_ScreenSave.y1 = static_cast< short > ( WhereY() );
	m_ScreenSave.topline1 = static_cast< short > ( GetTopLine() );
	m_ScreenSave.curatr1 = static_cast< short > ( curatr );
}


/*
 * restorescreen restores a screen previously saved with savescreen
 */
void WLocalIO::restorescreen() {
	if (m_ScreenSave.scrn1) {
		// COORD size;
		COORD topleft;
		CONSOLE_SCREEN_BUFFER_INFO bufinfo;
		SMALL_RECT region;

		GetConsoleScreenBufferInfo(m_hConOut,&bufinfo);
		topleft.Y = topleft.X = region.Top = region.Left = 0;
		region.Bottom = static_cast< short > ( bufinfo.dwSize.Y - 1 );
		region.Right  = static_cast< short > ( bufinfo.dwSize.X - 1 );

		WriteConsoleOutput(m_hConOut,m_ScreenSave.scrn1,bufinfo.dwSize,topleft,&region);
		BbsFreeMemory(m_ScreenSave.scrn1);
		m_ScreenSave.scrn1 = NULL;
	}
	SetTopLine( m_ScreenSave.topline1 );
	curatr = m_ScreenSave.curatr1;
	LocalGotoXY(m_ScreenSave.x1, m_ScreenSave.y1);
}


void WLocalIO::ExecuteTemporaryCommand( const char *pszCommand ) {
	GetSession()->DisplaySysopWorkingIndicator( true );
	savescreen();
	int i = GetTopLine();
	SetTopLine( 0 );
	curatr = 0x07;
	LocalCls();
	ExecuteExternalProgram( pszCommand, EFLAG_TOPSCREEN );
	restorescreen();
	SetTopLine( i );
	GetSession()->DisplaySysopWorkingIndicator( false );
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
	char ch1 = scan_to_char( nKeyCode );
	if (ch1) {
		char szCommand[ MAX_PATH ];
		memset( szCommand, 0, sizeof( szCommand ) );
		WFile macroFile( syscfg.datadir, MACROS_TXT );
		if ( macroFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
			int l = macroFile.GetLength();
			char* ss = static_cast<char *>( BbsAllocA(l + 10) );
			if (ss) {
				macroFile.Read( ss, l );
				macroFile.Close();

				ss[l] = 0;
				char* ss1 = strtok(ss, "\r\n");
				while (ss1) {
					if (upcase(*ss1) == ch1) {
						strtok(ss1, " \t");
						ss1 = strtok(NULL, "\r\n");
						if (ss1 && (strlen(ss1) < 128)) {
							strncpy(szCommand, ss1, sizeof(szCommand));
						}
						ss1 = NULL;
					} else {
						ss1 = strtok(NULL, "\r\n");
					}
				}
				BbsFreeMemory(ss);
			}

			if (szCommand[0]) {
				if (szCommand[0] == '@') {
					if (okmacro && okskey && (!charbufferpointer) && (szCommand[1])) {
						for (l = strlen(szCommand) - 1; l >= 0; l--) {
							if (szCommand[l] == '{') {
								szCommand[l] = '\r';
							}
							strcpy(charbuffer, szCommand);
							charbufferpointer = 1;
						}
					}
				}
				// TODO - Re-enable this at some point, but since we don't answer calls
				// too well, it's not too important right now.
				/*
				else if (m_nWfcStatus == 1)
				{
				    holdphone( true );
				    ExecuteTemporaryCommand(szCommand);
				    cleanup_net();
				    holdphone( false );
				}
				*/

			}
		}
	}
}


/*
 * skey handles all f-keys and the like hit FROM THE KEYBOARD ONLY
 */
void WLocalIO::skey( char ch ) {
	int nKeyCode = static_cast<unsigned char>( ch );
	int i, i1;

	if ( (syscfg.sysconfig & sysconfig_no_local) == 0 ) {
		if (okskey) {
			if ( nKeyCode >= AF1 && nKeyCode <= AF10 ) {
				set_autoval( nKeyCode - 104 );
			} else {
				switch ( nKeyCode ) {
				case F1:                          /* F1 */
					OnlineUserEditor();
					break;
				case SF1:                          /* Shift-F1 */
					set_global_handle( ( fileGlobalCap.IsOpen() ) ? false : true );
					GetApplication()->UpdateTopScreen();
					break;
				case CF1:                          /* Ctrl-F1 */
					GetApplication()->ToggleShutDown();
					break;
				case F2:                          /* F2 */
					GetSession()->topdata++;
					if ( GetSession()->topdata > WLocalIO::topdataUser ) {
						GetSession()->topdata = WLocalIO::topdataNone;
					}
					GetApplication()->UpdateTopScreen();
					break;
				case F3:                          /* F3 */
					if ( GetSession()->using_modem ) {
						incom = !incom;
						dump();
						tleft( false );
					}
					break;
				case F4:                          /* F4 */
					chatcall = false;
					GetApplication()->UpdateTopScreen();
					break;
				case F5:                          /* F5 */
					hangup = true;
					GetSession()->remoteIO()->dtr( false );
					break;
				case SF5:                          /* Shift-F5 */
					i1 = (rand() % 20) + 10;
					for (i = 0; i < i1; i++) {
						bputch( static_cast< unsigned char > ( rand() % 256 ) );
					}
					hangup = true;
					GetSession()->remoteIO()->dtr( false );
					break;
				case CF5:                          /* Ctrl-F5 */
					GetSession()->bout << "\r\nCall back later when you are there.\r\n\n";
					hangup = true;
					GetSession()->remoteIO()->dtr( false );
					break;
				case F6:                          /* F6 */
					ToggleSysopAlert();
					tleft( false );
					break;
				case F7:                          /* F7 */
					GetSession()->GetCurrentUser()->SetExtraTime( GetSession()->GetCurrentUser()->GetExtraTime() -
					        static_cast<float>( 5.0 * SECONDS_PER_MINUTE_FLOAT ) );
					tleft( false );
					break;
				case F8:                          /* F8 */
					GetSession()->GetCurrentUser()->SetExtraTime( GetSession()->GetCurrentUser()->GetExtraTime() +
					        static_cast<float>( 5.0 * SECONDS_PER_MINUTE_FLOAT ) );
					tleft( false );
					break;
				case F9:                          /* F9 */
					if ( GetSession()->GetCurrentUser()->GetSl() != 255 ) {
						if ( GetSession()->GetEffectiveSl() != 255) {
							GetSession()->SetEffectiveSl( 255 );
						} else {
							GetSession()->ResetEffectiveSl();
						}
						changedsl();
						tleft( false );
					}
					break;
				case F10:                          /* F10 */
					if (chatting == 0) {
						if (syscfg.sysconfig & sysconfig_2_way) {
							chat1("", true);
						} else {
							chat1("", false);
						}
					} else {
						chatting = 0;
					}
					break;
				case CF10:                         /* Ctrl-F10 */
					if (chatting == 0) {
						chat1("", false);
					} else {
						chatting = 0;
					}
					break;
				case HOME:                          /* HOME */
					if (chatting == 1) {
						chat_file = !chat_file;
					}
					break;
				default:
					alt_key( nKeyCode );
					break;
				}
			}
		} else {
			alt_key( nKeyCode );
		}
	}
}


static const char * pszTopScrItems[] = {
	"Comm Disabled",
	"Temp Sysop",
	"Capture",
	"Alert",
	"อออออออ",
	"Available",
	"อออออออออออ",
	"%s chatting with %s"
};


void WLocalIO::tleft(bool bCheckForTimeOut) {
	static char sbuf[200];
	static char *ss[8];

	if (!sbuf[0]) {
		ss[0] = sbuf;
		for (int i = 0; i < 7; i++) {
			strcpy(ss[i], pszTopScrItems[i]);
			ss[i + 1] = ss[i] + strlen(ss[i]) + 1;
		}
	}
	int cx = WhereX();
	int cy = WhereY();
	int ctl = GetTopLine();
	int cc = curatr;
	curatr = GetSession()->GetTopScreenColor();
	SetTopLine( 0 );
	double nsln = nsl();
	int nLineNumber = (chatcall && (GetSession()->topdata == WLocalIO::topdataUser)) ? 5 : 4;


	if (GetSession()->topdata) {
		if (GetSession()->using_modem && !incom) {
			LocalXYPuts( 1, nLineNumber, ss[0] );
			for ( std::string::size_type i = 19; i < GetSession()->GetCurrentSpeed().length(); i++ ) {
				LocalPutch( static_cast< unsigned char > ( 'อ' ) );
			}
		} else {
			LocalXYPuts( 1, nLineNumber, GetSession()->GetCurrentSpeed().c_str() );
			for (int i = WhereX(); i < 23; i++) {
				LocalPutch( static_cast< unsigned char > ( 'อ' ) );
			}
		}

		if (GetSession()->GetCurrentUser()->GetSl() != 255 && GetSession()->GetEffectiveSl() == 255) {
			LocalXYPuts( 23, nLineNumber, ss[1] );
		}
		if ( fileGlobalCap.IsOpen() ) {
			LocalXYPuts( 40, nLineNumber, ss[2] );
		}
		if (GetSysopAlert()) {
			LocalXYPuts( 54, nLineNumber, ss[3] );
		} else {
			LocalXYPuts( 54, nLineNumber, ss[4] );
		}

		if (sysop1()) {
			LocalXYPuts( 64, nLineNumber, ss[5] );
		} else {
			LocalXYPuts( 64, nLineNumber, ss[6] );
		}
	}
	switch (GetSession()->topdata) {
	case WLocalIO::topdataSystem:
		if ( GetSession()->IsUserOnline() ) {
			LocalXYPrintf( 18, 3, "T-%6.2f", nsln / SECONDS_PER_MINUTE_FLOAT );
		}
		break;
	case WLocalIO::topdataUser: {
		if ( GetSession()->IsUserOnline() ) {
			LocalXYPrintf( 18, 3, "T-%6.2f", nsln / SECONDS_PER_MINUTE_FLOAT );
		} else {
			LocalXYPrintf( 18, 3, GetSession()->GetCurrentUser()->GetPassword() );
		}
	}
	break;
	}
	SetTopLine( ctl );
	curatr = cc;
	LocalGotoXY( cx, cy );
	if ( bCheckForTimeOut && GetSession()->IsUserOnline() ) {
		if ( nsln == 0.0 ) {
			GetSession()->bout << "\r\nTime expired.\r\n\n";
			hangup = true;
		}
	}
}


void WLocalIO::UpdateTopScreen( WStatus* pStatus, WSession *pSession, int nInstanceNumber ) {
	char i;
	char sl[82], ar[17], dar[17], restrict[17], rst[17], lo[90];

	int lll = lines_listed;

	if ( so() && !incom ) {
		pSession->topdata = WLocalIO::topdataNone;
	}

	if ( syscfg.sysconfig & sysconfig_titlebar ) {
		// Only set the titlebar if the user wanted it that way.
		char szConsoleTitle[ 255 ];
		_snprintf( szConsoleTitle, sizeof( szConsoleTitle ), "WWIV Node %d (User: %s)", nInstanceNumber, pSession->GetCurrentUser()->GetUserNameAndNumber( pSession->usernum ) );
		::SetConsoleTitle( szConsoleTitle );
	}

	switch ( pSession->topdata ) {
	case WLocalIO::topdataNone:
		set_protect( 0 );
		break;
	case WLocalIO::topdataSystem:
		set_protect( 5 );
		break;
	case WLocalIO::topdataUser:
		if ( chatcall ) {
			set_protect( 6 );
		} else {
			if ( GetTopLine() == 6 ) {
				set_protect( 0 );
			}
			set_protect( 5 );
		}
		break;
	}
	int cx = WhereX();
	int cy = WhereY();
	int nOldTopLine = GetTopLine();
	int cc = curatr;
	curatr = pSession->GetTopScreenColor();
	SetTopLine( 0 );
	for ( i = 0; i < 80; i++ ) {
		sl[i] = '\xCD';
	}
	sl[80] = '\0';

	switch (pSession->topdata) {
	case WLocalIO::topdataNone:
		break;
	case WLocalIO::topdataSystem: {
		LocalXYPrintf( 0, 0, "%-50s  Activity for %8s:      ", syscfg.systemname, pStatus->GetLastDate() );

		LocalXYPrintf( 0, 1, "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
		               pStatus->GetNumUsers(), pStatus->GetCallerNumber(),
		               pStatus->GetNumCallsToday(), pStatus->GetNumLocalPosts() );

		LocalXYPrintf( 0, 2, "%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
		               pSession->GetCurrentUser()->GetUserNameAndNumber( pSession->usernum ),
		               pStatus->GetMinutesActiveToday(),
		               static_cast<int>( 10 * pStatus->GetMinutesActiveToday() / 144 ),
		               pStatus->GetNumEmailSentToday() );

		LocalXYPrintf( 0, 3, "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
		               pSession->GetCurrentUser()->GetSl(),
		               pSession->GetCurrentUser()->GetDsl(),
		               fwaiting,
		               pStatus->GetNumUploadsToday(),
		               pStatus->GetNumFeedbackSentToday() );
	}
	break;
	case WLocalIO::topdataUser: {
		strcpy(rst, restrict_string);
		for (i = 0; i <= 15; i++) {
			if ( pSession->GetCurrentUser()->HasArFlag( 1 << i ) ) {
				ar[i] = static_cast< char > ('A' + i );
			} else {
				ar[i] = SPACE;
			}
			if ( pSession->GetCurrentUser()->HasDarFlag( 1 << i ) ) {
				dar[i] = static_cast< char > ( 'A' + i );
			} else {
				dar[i] = SPACE;
			}
			if ( pSession->GetCurrentUser()->HasRestrictionFlag( 1 << i ) ) {
				restrict[i] = rst[i];
			} else {
				restrict[i] = SPACE;
			}
		}
		dar[16] = '\0';
		ar[16] = '\0';
		restrict[16] = '\0';
		if ( !wwiv::strings::IsEquals( pSession->GetCurrentUser()->GetLastOn(), date() ) ) {
			strcpy( lo, pSession->GetCurrentUser()->GetLastOn() );
		} else {
			_snprintf( lo, sizeof( lo ), "Today:%2d", pSession->GetCurrentUser()->GetTimesOnToday() );
		}

		LocalXYAPrintf( 0, 0, curatr, "%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u",
		                pSession->GetCurrentUser()->GetUserNameAndNumber( pSession->usernum ),
		                pSession->GetCurrentUser()->GetNumMailWaiting(),
		                pSession->GetCurrentUser()->GetFilesUploaded(),
		                pSession->GetCurrentUser()->GetUploadK(),
		                pSession->GetCurrentUser()->GetSl(),
		                pSession->GetCurrentUser()->GetNumLogons(),
		                pSession->GetCurrentUser()->GetNumMessagesPosted() );

		char szCallSignOrRegNum[ 41 ];
		if ( pSession->GetCurrentUser()->GetWWIVRegNumber() ) {
			_snprintf( szCallSignOrRegNum, sizeof( szCallSignOrRegNum ), "%lu", pSession->GetCurrentUser()->GetWWIVRegNumber() );
		} else {
			strcpy( szCallSignOrRegNum, pSession->GetCurrentUser()->GetCallsign() );
		}
		LocalXYPrintf(  0, 1, "%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0lu ES=%4u",
		                pSession->GetCurrentUser()->GetRealName(),
		                pSession->GetCurrentUser()->GetVoicePhoneNumber(),
		                szCallSignOrRegNum,
		                pSession->GetCurrentUser()->GetFilesDownloaded(),
		                pSession->GetCurrentUser()->GetDownloadK(),
		                pSession->GetCurrentUser()->GetDsl(),
		                static_cast<long>( ( pSession->GetCurrentUser()->GetTimeOn() + timer() - timeon ) / SECONDS_PER_MINUTE_FLOAT ),
		                pSession->GetCurrentUser()->GetNumEmailSent() + pSession->GetCurrentUser()->GetNumNetEmailSent() );

		LocalXYPrintf( 0, 2, "ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u",
		               ar, dar, restrict, pSession->GetCurrentUser()->GetExempt(),
		               lo, pSession->GetCurrentUser()->GetNumFeedbackSent() );

		LocalXYPrintf( 0, 3, "%-40.40s %c %2u %-16.16s           FW= %3u",
		               pSession->GetCurrentUser()->GetNote(),
		               pSession->GetCurrentUser()->GetGender(),
		               pSession->GetCurrentUser()->GetAge(),
		               ctypes( pSession->GetCurrentUser()->GetComputerType() ), fwaiting );

		if (chatcall) {
			LocalXYPuts( 0, 4, m_chatReason.c_str() );
		}
	}
	break;
	default:
		break;
	}
	if ( nOldTopLine != 0 ) {
		LocalXYPuts( 0, nOldTopLine - 1, sl );
	}
	SetTopLine( nOldTopLine );
	LocalGotoXY( cx, cy );
	curatr = cc;
	tleft( false );

	lines_listed = lll;
}


/****************************************************************************/


/**
 * IsLocalKeyPressed - returns whether or not a key been pressed at the local console.
 *
 * @return true if a key has been pressed at the local console, false otherwise
 */
bool WLocalIO::LocalKeyPressed() {
	return (x_only ? 0 : (HasKeyBeenPressed() || ExtendedKeyWaiting)) ? true : false;
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


/****************************************************************************/
/*
* returns the ASCII code of the next character waiting in the
* keyboard buffer.  If there are no characters waiting in the
* keyboard buffer, then it returns immediately with a value
* of 255.
*
* A value of 0 is returned for all extended keys (such as F1,
* Alt-X, etc.).  The function must be called again upon receiving
* a value of 0 to obtain the value of the extended key pressed.
*/
unsigned char WLocalIO::getchd1() {
	if ( !( HasKeyBeenPressed() || ExtendedKeyWaiting ) ) {
		return 255;
	}
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

	// TODO - This code below doesn't work, hence we aren't using it.
	// Ideally, we should support mouse input, and try to not use the
	// generic CRT functions whenever possible to get a better Win32
	// feel to everything, but until the code works, it's disabled.
#if 0

	PINPUT_RECORD pIRBuf;
	DWORD NumPeeked;
	bool bHasKeyBeenPressed = false;

	DWORD dwNumEvents;  // NumPending
	GetNumberOfConsoleInputEvents( m_hConIn, &dwNumEvents );
	if ( dwNumEvents == 0 ) {
		return false;
	}

	PINPUT_RECORD pInputRec = ( PINPUT_RECORD ) bbsmalloc( dwNumEvents * sizeof( INPUT_RECORD ) );
	if ( !pInputRec ) {
		// Really something bad happened here.
		return false;
	}

	DWORD dwNumEventsRead;
	if ( PeekConsoleInput( m_hConIn, pInputRec, dwNumEvents, &dwNumEventsRead ) ) {
		for( int i=0; i < dwNumEventsRead; i++ ) {
			if ( pInputRec->EventType == KEY_EVENT &&
			        pInputRec->Event.KeyEvent.bKeyDown &&
			        ( pInputRec->Event.KeyEvent.uChar.AsciiChar ||
			IsExtendedKeyCode( pInputRec->Event.KeyEvent ) ) {
			bHasKeyBeenPressed = true;
			break;
		}
		pInputRec++;
	}
}

// Scan all of the peeked events to determine if any is a key event
// which should be recognized.
for ( ; NumPeeked > 0 ; NumPeeked--, pIRBuf++ ) {
		if ( (pIRBuf->EventType == KEY_EVENT) &&
		        (pIRBuf->Event.KeyEvent.bKeyDown) &&
		        ( pIRBuf->Event.KeyEvent.uChar.AsciiChar ||
		          _getextendedkeycode( &(pIRBuf->Event.KeyEvent) ) ) ) {
			// Key event corresponding to an ASCII character or an
			// extended code. In either case, success!
			ret = TRUE;
		}
	}

	BbsFreeMemory( pInputRec );

	return bHasKeyBeenPressed;
#endif

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
	// Set console title
	std::stringstream consoleTitleStream;
	consoleTitleStream << "WWIV Node " << GetApplication()->GetInstanceNumber() << " (" << syscfg.systemname << ")";
	SetConsoleTitle( consoleTitleStream.str().c_str() );
}


