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

#ifndef __INCLUDED_PLATFORM_WLOCALIO_H__
#define __INCLUDED_PLATFORM_WLOCALIO_H__

#ifdef _WIN32
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOATOM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NOCRYPT

// Define these for MFC projects
#define NOTAPE
#define NOIMAGE
#define NOPROXYSTUB
#define NORPC
#define NOIME
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef CopyFile
#undef GetFullPathName

#endif // _WIN32
//
// This C++ class should encompass all Local Input/Output from The BBS.
// You should use a routine in here instead of using printf, puts, etc.
//

#define GLOBAL_SIZE 4096

class WFile;
class WStatus;
class WSession;

struct screentype {
	short x1, y1, topline1, curatr1;

#ifdef _WIN32
	CHAR_INFO* scrn1;
#else
	char *scrn1;
#endif
};




class WLocalIO {

  public:
	static const int cursorNone;
	static const int cursorNormal;
	static const int cursorSolid;

	static const int topdataNone;
	static const int topdataSystem;
	static const int topdataUser;

  private:
	std::string  m_chatReason;
	WFile   fileGlobalCap; // g_hGlobalCapHandle;
	bool    m_bSysopAlert;
	int     m_nTopLine;
	int     m_nScreenBottom;
	screentype m_ScreenSave;


  private:
	void ExecuteTemporaryCommand( const char *pszCommand );
	char scan_to_char( int nKeyCode );
	void alt_key( int nKeyCode );
	int  GetEditLineStringLength( const char *pszText );

  protected:
	int ExtendedKeyWaiting;
	char *global_buf;
	int global_ptr;
	int wx;

#if defined ( __OS2__ )
	unsigned char CellStr[3];
	VIOMODEINFO vmi;
#endif

#if defined ( _WIN32 )
	COORD  m_cursorPosition;
	HANDLE m_hConOut;
	HANDLE m_hConIn;
	CONSOLE_SCREEN_BUFFER_INFO m_consoleBufferInfo;
#endif

#if defined ( __APPLE__ )
	short m_cursorPositionX;
	short m_cursorPositionY;
#endif

  public:
#if defined ( _WIN32 )
	void set_attr_xy(int x, int y, int a);
	COORD m_originalConsoleSize;
#endif // _WIN32

	// Constructor/Destructor
	WLocalIO();
	WLocalIO( const WLocalIO& copy );
	virtual ~WLocalIO();

	void SetChatReason( char* pszChatReason ) {
		m_chatReason = pszChatReason;
	}
	void ClearChatReason() {
		m_chatReason.clear();
	}

	const int GetTopLine() const {
		return m_nTopLine;
	}
	void SetTopLine( int nTopLine ) {
		m_nTopLine = nTopLine;
	}

	const int GetScreenBottom() const {
		return m_nScreenBottom;
	}
	void SetScreenBottom( int nScreenBottom ) {
		m_nScreenBottom = nScreenBottom;
	}

	void SetSysopAlert( bool b ) {
		m_bSysopAlert = b;
	}
	const bool GetSysopAlert() const {
		return m_bSysopAlert;
	}
	void ToggleSysopAlert() {
		m_bSysopAlert = !m_bSysopAlert;
	}

	void set_global_handle(bool bOpenFile, bool bOnlyUpdateVariable = false );
	void global_char(char ch);
	void set_x_only(int tf, const char *pszFileName, int ovwr);
	void LocalGotoXY(int x, int y);
	int  WhereX();
	int  WhereY();
	void LocalLf();
	void LocalCr();
	void LocalCls();
	void LocalClrEol();
	void LocalBackspace();
	void LocalPutchRaw(unsigned char ch);
	void LocalPutch(unsigned char ch);
	void LocalPuts( const char *pszText );
	void LocalXYPuts( int x, int y, const char *pszText );
	void LocalFastPuts( const char *pszText );
	int  LocalPrintf( const char *pszFormattedText, ... );
	int  LocalXYPrintf( int x, int y, const char *pszFormattedText, ... );
	int  LocalXYAPrintf( int x, int y, int nAttribute, const char *pszFormattedText, ... );
	void pr_Wait(bool displayWait);
	void set_protect(int l);
	void savescreen();
	void restorescreen();
	void skey(char ch);
	void tleft(bool bCheckForTimeOut);
	void UpdateTopScreen( WStatus* pStatus, WSession *pSession, int nInstanceNumber );
	bool  LocalKeyPressed();
	unsigned char getchd();
	unsigned char getchd1();
	void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc);
	int  LocalGetChar();
	/*
	 * MakeLocalWindow makes a "shadowized" window with the upper-left hand corner at
	 * (x,y), and the lower-right corner at (x+xlen,y+ylen).
	 */
	void MakeLocalWindow(int x, int y, int xlen, int ylen);
	void SetCursor(int cursorStyle);
	void LocalWriteScreenBuffer(const char *pszBuffer);
	int  GetDefaultScreenBottom();

	void LocalEditLine(char *s, int len, int status, int *returncode, char *ss);
	void UpdateNativeTitleBar();

};



#endif // __INCLUDED_PLATFORM_WLOCALIO_H__
