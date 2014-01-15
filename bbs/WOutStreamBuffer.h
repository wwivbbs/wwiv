


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

#if !defined ( __INCLUDED_WStreamBuffer_H__ )
#define __INCLUDED_WStreamBuffer_H__


#if defined( _MSC_VER ) && !defined( _CRT_SECURE_NO_DEPRECATE )
#define _CRT_SECURE_NO_DEPRECATE
#endif	// _MSC_VER 

#include <iostream>
#include <streambuf>
#include <ios>

class WOutStreamBuffer : public std::streambuf {
  public:
	WOutStreamBuffer();
	~WOutStreamBuffer();
	virtual std::ostream::int_type overflow( std::ostream::int_type c );
	virtual std::streamsize xsputn( const char *pszText, std::streamsize numChars );
};


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable: 4511 4512 )
#endif


class WComm;
class WLocalIO;

class WOutStream : public std::ostream {
  protected:
	WOutStreamBuffer buf;
	WLocalIO *m_pLocalIO;
	WComm *m_pComm;

  public:
	WOutStream() :
#if defined(_WIN32)
		buf(),
#endif
		std::ostream( &buf ) {
		init( &buf );
	}
	virtual ~WOutStream() {}

	void SetLocalIO( WLocalIO *pLocalIO ) {
		m_pLocalIO = pLocalIO;
	}
	WLocalIO* localIO() {
		return m_pLocalIO;
	}

	void SetComm( WComm *pComm ) {
		m_pComm = pComm;
	}
	WComm* remoteIO() {
		return m_pComm;
	}

	void Color(int wwivColor);
	void ResetColors();
	void GotoXY(int x, int y);
	void NewLine(int nNumLines = 1);
	void BackSpace();
	/* This sets the current color (both locally and remotely) to that
	 * specified (in IBM format).
	 */
	void SystemColor( int nColor );
	void DisplayLiteBar(const char *fmt,...);
	/** Backspaces from the current cursor position to the beginning of a line */
	void BackLine();

	/**
	 * Moves the cursor to the end of the line using ANSI sequences.  If the user
	 * does not have ansi, this this function does nothing.
	 */
	void ClearEOL();

	/**
	 * Clears the local and remote screen using ANSI (if enabled), otherwise DEC 12
	 */
	void ClearScreen();

	/**
	 * This will make a reverse-video prompt line i characters long, repositioning
	 * the cursor at the beginning of the input prompt area.  Of course, if the
	 * user does not want ansi, this routine does nothing.
	 */
	void ColorizedInputField( int nNumberOfChars );

	/**
	 * This function outputs a string of characters to the screen (and remotely
	 * if applicable).  The com port is also checked first to see if a remote
	 * user has hung up
	 */
	int  Write(const char *pszText );

	int  WriteFormatted(const char *fmt,...);
};

namespace wwiv {
template<class charT, class traits>
std::basic_ostream<charT, traits>&
endl (std::basic_ostream<charT, traits>& strm ) {
	strm.write( "\r\n", 2 );
	return strm;
}
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif  // __INCLUDED_WStreamBuffer_H__
