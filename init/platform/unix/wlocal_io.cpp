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
#if !defined (NOT_BBS)
	char szFileName[ MAX_PATH ];

	if (x_only) {
		return;
	}

	if (bOpenFile) {
		if (!fileGlobalCap.IsOpen()) {
			snprintf( szFileName, sizeof( szFileName ), "%sglobal-%d.txt", syscfg.gfilesdir, GetApplication()->GetInstanceNumber() );
			fileGlobalCap.SetName( szFileName );
			bool bOpen = fileGlobalCap.Open( WFile::modeBinary | WFile::modeAppend | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
			global_ptr = 0;
			global_buf = static_cast<char *>( malloc(GLOBAL_SIZE) );
			if (!bOpen || (!global_buf)) {
				if (global_buf) {
					free(global_buf);
					global_buf = NULL;
				}
			}
		}
	} else {
		if (fileGlobalCap.IsOpen() && !bOnlyUpdateVariable) {
			fileGlobalCap.Write(global_buf, global_ptr);
			fileGlobalCap.Close();
			if (global_buf) {
				free(global_buf);
				global_buf = NULL;
			}
		}
	}
#endif  // NOT_BBS
}


void WLocalIO::global_char(char ch) {
#if !defined (NOT_BBS)
	if (global_buf && fileGlobalCap.IsOpen()) {
		global_buf[global_ptr++] = ch;
		if (global_ptr == GLOBAL_SIZE) {
			fileGlobalCap.Write(global_buf, global_ptr);
			global_ptr = 0;
		}
	}
#endif  // NOT_BBS
}

void WLocalIO::set_x_only(int tf, const char *pszFileName, int ovwr) {
#if !defined(NOT_BBS)
	static bool bOldGlobalHandle;
	char szTempFileName[ MAX_PATH ];

	if (x_only) {
		if (!tf) {
			if (fileGlobalCap.IsOpen()) {
				fileGlobalCap.Write(global_buf, global_ptr);
				fileGlobalCap.Close();
				if (global_buf) {
					free(global_buf);
					global_buf = NULL;
				}
			}
			x_only = false;
			set_global_handle( bOldGlobalHandle );
			bOldGlobalHandle = false;
			express = expressabort = false;
		}
	} else {
		if (tf) {
			bOldGlobalHandle = fileGlobalCap.IsOpen();
			set_global_handle( false );
			x_only = true;
			wx = 0;
			snprintf( szTempFileName, sizeof( szTempFileName ), "%s%s", syscfgovr.tempdir, pszFileName );
			fileGlobalCap.SetName( szTempFileName );
			if (ovwr) {
				fileGlobalCap.Open(WFile::modeBinary | WFile::modeText | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite);
			} else {
				fileGlobalCap.Open(WFile::modeBinary | WFile::modeText | WFile::modeCreateFile | WFile::modeAppend | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite);
			}
			global_ptr = 0;
			express = true;
			expressabort = false;
			global_buf = static_cast<char *>( malloc(GLOBAL_SIZE) );
			if (!fileGlobalCap.IsOpen() || (!global_buf)) {
				if (global_buf) {
					free(global_buf);
					global_buf = NULL;
				}
				set_x_only(0, NULL, 0);
			}
		}
	}
	timelastchar1 = timer1();
#endif  // NOT_BBS
}


/*
 * This, obviously, moves the cursor to the location specified, offset from
 * the protected dispaly at the top of the screen.  Note: this function
 * is 0 based, so (0,0) is the upper left hand corner.
 */
void WLocalIO::LocalGotoXY(int x, int y) {
#if defined( __APPLE__ )
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
#endif
}




/* This function returns the current X cursor position, as the number of
 * characters from the left hand side of the screen.  An X position of zero
 * means the cursor is at the left-most position
 */
int WLocalIO::WhereX() {
#if defined( __APPLE__ )
	if (x_only) {
		return( wx );
	}
	return m_cursorPositionX;
#else
	return 0;
#endif
}



/* This function returns the Y cursor position, as the line number from
 * the top of the logical window.  The offset due to the protected top
 * of the screen display is taken into account.  A WhereY() of zero means
 * the cursor is at the top-most position it can be at.
 */
int WLocalIO::WhereY() {
#if defined( __APPLE__ )
	return m_cursorPositionY;
#else
	return 0;
#endif
}



/* This function performs a linefeed to the screen (but not remotely) by
 * either moving the cursor down one line, or scrolling the logical screen
 * up one line.
 */
void WLocalIO::LocalLf() {
#if defined( __APPLE__ )
	std::cout << "\n";
	m_cursorPositionY++;
	if(m_cursorPositionY > 24) {
		m_cursorPositionY = 24;
	}
#endif
}



/* This short function returns the local cursor to the left-most position
 * on the screen.
 */
void WLocalIO::LocalCr() {
#if defined( __APPLE__ )
	std::cout << "\r";
	m_cursorPositionX = 0;
#endif
}

/*
 * This clears the local logical screen
 */
void WLocalIO::LocalCls() {
#if defined( __APPLE__ )
	std::cout << "\x1b[2J";
	m_cursorPositionX = 0;
	m_cursorPositionY = 0;
#endif
}



/* This function moves the cursor one position to the left, or if the cursor
 * is currently at its left-most position, the cursor is moved to the end of
 * the previous line, except if it is on the top line, in which case nothing
 * happens.
 */
void WLocalIO::LocalBackspace() {
#if defined( __APPLE__ )
	std::cout << "\b";
	if( m_cursorPositionX >= 0 ) {
		m_cursorPositionX--;
	} else if( m_cursorPositionY != GetTopLine() ) {
		m_cursorPositionX = 79;
		m_cursorPositionY--;
	}
#endif
}



/* This function outputs one character to the screen, then updates the
 * cursor position accordingly, scolling the screen if necessary.  Not that
 * this function performs no commands such as a C/R or L/F.  If a value of
 * 8, 7, 13, 10, 12 (BACKSPACE, BEEP, C/R, L/F, TOF), or any other command-
 * type characters are passed, the appropriate corresponding "graphics"
 * symbol will be output to the screen as a normal character.
 */
void WLocalIO::LocalPutchRaw(unsigned char ch) {
#if defined( __APPLE__ )
	std::cout << ch;
	if(m_cursorPositionX <= 79) {
		m_cursorPositionX++;
		return;
	}
	m_cursorPositionX = 0;
	if(m_cursorPositionY != GetScreenBottom()) {
		m_cursorPositionY++;
	}
#endif
}




/* This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void WLocalIO::LocalPutch(unsigned char ch) {
#if defined( __APPLE__ )
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
#endif
}


/*
 * This (obviously) outputs a string TO THE SCREEN ONLY
 */
void WLocalIO::LocalPuts(const char *s) {
#if defined( __APPLE__ )
	while (*s) {
		LocalPutch(*s++);
	}
#endif
}


void WLocalIO::LocalXYPuts( int x, int y, const char *pszText ) {
#if defined( __APPLE__ )
	LocalGotoXY( x, y );
	LocalFastPuts( pszText );
#endif
}


/*
 * This RAPIDLY outputs ONE LINE to the screen only
 */
void WLocalIO::LocalFastPuts(const char *s) {
#if defined( __APPLE__ )
	m_cursorPositionX += strlen( s );
	m_cursorPositionX %= 80;

	// TODO: set current attributes

	std::cout << s;
#endif
}


int  WLocalIO::LocalPrintf( const char *pszFormattedText, ... ) {
#if defined( __APPLE__ )
	va_list ap;
	char szBuffer[ 1024 ];

	va_start( ap, pszFormattedText );
	int nNumWritten = vsnprintf( szBuffer, sizeof( szBuffer ), pszFormattedText, ap );
	va_end( ap );
	LocalFastPuts( szBuffer );
	return nNumWritten;
#endif
	// NOP
	return 0;
}


int  WLocalIO::LocalXYPrintf( int x, int y, const char *pszFormattedText, ... ) {
#if defined( __APPLE__ )
	va_list ap;
	char szBuffer[ 1024 ];

	va_start( ap, pszFormattedText );
	int nNumWritten = vsnprintf( szBuffer, sizeof( szBuffer ), pszFormattedText, ap );
	va_end( ap );
	LocalXYPuts( x, y, szBuffer );
	return nNumWritten;
#endif
	// NOP
	return 0;
}


int  WLocalIO::LocalXYAPrintf( int x, int y, int nAttribute, const char *pszFormattedText, ... ) {
#if defined( __APPLE__ )
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
#endif
	// NOP
	return 0;
}



/*
 * set_protect sets the number of lines protected at the top of the screen.
 */
void WLocalIO::set_protect(int l) {
#if defined (__APPLE__) && !defined (NOT_BBS)
	SetTopLine( l );
	GetSession()->screenlinest = ( GetSession()->using_modem ) ? GetSession()->GetCurrentUser()->GetScreenLines() : defscreenbottom + 1 - GetTopLine();
#endif
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
#if defined (__APPLE__) && !defined (NOT_BBS)
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
#endif // __APPLE__ && !NOT_BBS
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
#if defined (__APPLE__) && !defined (NOT_BBS)
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
				LocalPutch( static_cast< unsigned char > ( '+' ) );
			}
		} else {
			LocalXYPuts( 1, nLineNumber, GetSession()->GetCurrentSpeed().c_str() );
			for (int i = WhereX(); i < 23; i++) {
				LocalPutch( static_cast< unsigned char > ( '+' ) );
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
#endif
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
#if defined (__APPLE__) && !defined (NOT_BBS)
	char i;
	char sl[82], ar[17], dar[17], restrict[17], rst[17], lo[90];

	int lll = lines_listed;

	if ( so() && !incom ) {
		pSession->topdata = WLocalIO::topdataNone;
	}

	//if ( syscfg.sysconfig & sysconfig_titlebar )
	//{
	// Only set the titlebar if the user wanted it that way.
	//char szConsoleTitle[ 255 ];
	//_snprintf( szConsoleTitle, sizeof( szConsoleTitle ), "WWIV Node %d (User: %s)", nInstanceNumber, pSession->GetCurrentUser()->GetUserNameAndNumber( pSession->usernum ) );
	//::SetConsoleTitle( szConsoleTitle );
	//}

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
			snprintf( lo, sizeof( lo ), "Today:%2d", pSession->GetCurrentUser()->GetTimesOnToday() );
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
			snprintf( szCallSignOrRegNum, sizeof( szCallSignOrRegNum ), "%lu", pSession->GetCurrentUser()->GetWWIVRegNumber() );
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
#endif
}

void WLocalIO::LocalScrollScreen(int nTop, int nBottom, int nDirection) {
  // TODO(rushfan): Implement me
}
