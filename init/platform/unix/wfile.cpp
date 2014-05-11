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

#include "wwiv.h"
#include <sys/file.h>
#include <sys/stat.h>
#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>

#if defined( __APPLE__ )
#if !defined( O_BINARY )
#define O_BINARY 0
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
//
// Constants
//

const int WFile::modeDefault        = O_RDWR;
const int WFile::modeUnknown        = -1;
const int WFile::modeAppend         = O_APPEND;
const int WFile::modeBinary         = 0;
const int WFile::modeCreateFile     = O_CREAT;
const int WFile::modeReadOnly       = O_RDONLY;
const int WFile::modeReadWrite      = O_RDWR;
const int WFile::modeText           = 0;
const int WFile::modeWriteOnly      = O_WRONLY;
const int WFile::modeTruncate       = O_TRUNC;

const int WFile::shareUnknown       = -1;
const int WFile::shareDenyReadWrite = S_IWRITE;
const int WFile::shareDenyWrite     = 0;
const int WFile::shareDenyRead      = S_IREAD;
const int WFile::shareDenyNone      = 0;

const int WFile::permUnknown        = -1;
const int WFile::permWrite          = O_WRONLY;
const int WFile::permRead           = O_RDONLY;
const int WFile::permReadWrite      = O_RDWR;

const int WFile::seekBegin          = SEEK_SET;
const int WFile::seekCurrent        = SEEK_CUR;
const int WFile::seekEnd            = SEEK_END;

const int WFile::invalid_handle     = -1;

const char WFile::pathSeparatorChar = '/';
const char WFile::separatorChar     = ':';

WLogger*  WFile::m_pLogger;
int       WFile::m_nDebugLevel;

// WAIT_TIME is 10 seconds, 1 second = 1000 useconds
#define WAIT_TIME 10000
#define TRIES 100

/////////////////////////////////////////////////////////////////////////////
//
// Constructors/Destructors
//

WFile::WFile() {
	init();
}


WFile::WFile( const std::string fileName ) {
	init();
	this->SetName( fileName );
}


WFile::WFile( const std::string dirName, const std::string fileName ) {
	init();
	this->SetName( dirName, fileName );
}


void WFile::init() {
	m_bOpen                 = false;
	m_hFile                 = WFile::invalid_handle;
	memset( m_szFileName, 0, MAX_PATH + 1 );
}


WFile::~WFile() {
	if (this->IsOpen()) {
		this->Close();
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// Member functions
//


bool WFile::SetName( const std::string fileName ) {
	strncpy( m_szFileName, fileName.c_str(), MAX_PATH );
	return true;
}


bool WFile::SetName( const std::string dirName, const std::string fileName ) {
	std::stringstream fullPathName;
	fullPathName << dirName;
	if ( !dirName.empty() && dirName[ dirName.length() -1 ] == '/' ) {
		fullPathName << fileName;
	} else {
		fullPathName << "/" << fileName;
	}
	return SetName( fullPathName.str() );
}

bool WFile::Open( int nFileMode, int nShareMode, int nPermissions ) {
	WWIV_ASSERT( this->IsOpen() == false );

	// Set default permissions
	if ( nPermissions == WFile::permUnknown ) {
		// See platform/WIN32/WFile.cpp for notes on why this is not 0.
		nPermissions = WFile::permRead;
		if ( ( nFileMode & WFile::modeReadWrite ) ||
		        ( nFileMode & WFile::modeWriteOnly ) ) {
			nPermissions |= WFile::permWrite;
		}
	}

	// Set default share mode
	if ( nShareMode == WFile::shareUnknown ) {
		nShareMode = shareDenyWrite;
	}

	WWIV_ASSERT( nShareMode   != WFile::shareUnknown );
	WWIV_ASSERT( nFileMode    != WFile::modeUnknown  );
	WWIV_ASSERT( nPermissions != WFile::permUnknown  );

	m_hFile =  open( m_szFileName, nFileMode, 0644 );
	if (m_hFile < 0) {
		int count = 1;
		if ( access( m_szFileName, 0 ) != -1 ) {
#if !defined(NOT_BBS)
			WWIV_Delay( WAIT_TIME );
#endif // #if !defined(NOT_BBS)
			m_hFile = open( m_szFileName, nFileMode, nShareMode );
			while ( ( m_hFile < 0 && errno == EACCES ) && count < TRIES ) {
#if !defined(NOT_BBS)
				WWIV_Delay( ( count % 2 ) ? WAIT_TIME : 0 );
#endif // #if !defined(NOT_BBS)
				count++;
				m_hFile = open( m_szFileName, nFileMode, nShareMode );
			}
		}
	}

	m_bOpen = WFile::IsFileHandleValid( m_hFile );
	if( m_bOpen ) {
		flock( m_hFile, (nShareMode & shareDenyWrite) ? LOCK_EX : LOCK_SH );
	}
	return m_bOpen;
}


void WFile::Close() {
	if ( WFile::IsFileHandleValid( m_hFile ) ) {
		flock(m_hFile, LOCK_UN);
		close( m_hFile );
		m_hFile = WFile::invalid_handle;
		m_bOpen = false;
	}
}


int WFile::Read( void * pBuffer, size_t nCount ) {
	return read( m_hFile, pBuffer, nCount );
}


int WFile::Write( const void * pBuffer, size_t nCount ) {
	return write( m_hFile, pBuffer, nCount );
}


long WFile::GetLength() {
	WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

	if (! IsOpen() ) {
		return -1;
	}

	// Save Current position, then seek to the end of the file, then retore the original position
	size_t nSavedPosition = lseek(m_hFile, 0L, SEEK_CUR);		/* No "ftell()" equiv */
	size_t len = lseek(m_hFile, 0L, SEEK_END);
	lseek(m_hFile, nSavedPosition, SEEK_SET);

	return static_cast<long>( len );
}


long WFile::Seek( long lOffset, int nFrom ) {
	WWIV_ASSERT( nFrom == WFile::seekBegin || nFrom == WFile::seekCurrent || nFrom == WFile::seekEnd );
	WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

	return lseek( m_hFile, lOffset, nFrom );
}


void WFile::SetLength( long lNewLength ) {
	WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );
	ftruncate( m_hFile, lNewLength );
}


bool WFile::Exists() const {
	return WFile::Exists( m_szFileName );
}


bool WFile::Delete( bool bUseTrashCan ) {
	if( this->IsOpen() ) {
		this->Close();
	}
	return ( unlink(m_szFileName ) == 0 ) ? true : false;
}

bool WFile::IsDirectory() {
	struct stat statbuf;
	stat(m_szFileName, &statbuf);
	return(statbuf.st_mode & 0x0040000 ? true : false);
}

bool WFile::IsFile() {
	return(!IsDirectory());
}


bool WFile::SetFilePermissions( int nPermissions ) {
	return ( chmod( m_szFileName, nPermissions ) == 0 ) ? true : false;
}


time_t WFile::GetFileTime() {
	bool bOpenedHere = false;
	if ( !this->IsOpen() ) {
		bOpenedHere = true;
		Open();
	}
	WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

	struct stat buf;
	int nFileTime = ( fstat( m_hFile, &buf ) == -1 ) ? 0 : buf.st_mtime;

	if ( bOpenedHere ) {
		Close();
	}
	return nFileTime;
}


/////////////////////////////////////////////////////////////////////////////
//
// Static functions
//

bool WFile::Remove( const std::string fileName ) {
	WWIV_ASSERT( !fileName.empty() );
	return ( unlink(fileName.c_str()) ? false : true );
}

bool WFile::Remove( const std::string directoryName, const std::string fileName ) {
	WWIV_ASSERT( !directoryName.empty() );
	WWIV_ASSERT( !fileName.empty() );
	std::stringstream fullFileName;
	fullFileName << directoryName << fileName;
	return WFile::Remove( fullFileName.str() );
}

bool WFile::Rename( const std::string origFileName, const std::string newFileName ) {
	WWIV_ASSERT( !origFileName.empty() );
	WWIV_ASSERT( !newFileName.empty() );
	return rename( origFileName.c_str(), newFileName.c_str() );
}


bool WFile::Exists( const std::string fileName ) {
	// Note, if a directory name is passed for pszFileName, it is expected that
	// this will work with either a trailing slash or without it.
	WWIV_ASSERT( !fileName.empty() );
	struct stat buf;
	return ( stat(fileName.c_str(), &buf) ? false : true );
}


bool WFile::Exists( const std::string directoryName, const std::string fileName ) {
	std::stringstream fullFileName;
	if (!directoryName.empty() && directoryName[directoryName.length()-1] == '/') {
		fullFileName << directoryName << fileName;
	} else {
		fullFileName << directoryName << "/" << fileName;
	}
	return Exists( fullFileName.str() );
}


bool WFile::SetFilePermissions( const std::string fileName, int nPermissions ) {
	WWIV_ASSERT( !fileName.empty() );
	return ( chmod( fileName.c_str(), nPermissions ) == 0 ) ? true : false;
}


bool WFile::IsFileHandleValid( int hFile ) {
	return ( hFile != WFile::invalid_handle ) ? true : false;
}


bool WFile::ExistsWildcard( const std::string pszWildCard ) {
	WFindFile fnd;
	return(fnd.open(pszWildCard.c_str(), 0));
}


bool WFile::CopyFile( const std::string sourceFileName, const std::string destFileName ) {
	if ( sourceFileName != destFileName && WFile::Exists( sourceFileName ) && !WFile::Exists( destFileName ) ) {
		char *pBuffer = static_cast<char *>( malloc( 16400 ) );
		if ( pBuffer == NULL ) {
			return false;
		}
		int hSourceFile = open( sourceFileName.c_str(), O_RDONLY | O_BINARY );
		if(!hSourceFile) {
			BbsFreeMemory(pBuffer);
			return false;
		}

		int hDestFile = open( destFileName.c_str(), O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE );
		if(!hDestFile) {
			BbsFreeMemory(pBuffer);
			close(hSourceFile);
			return false;
		}

		int i = read(hSourceFile, (void *) pBuffer, 16384);

		while (i > 0) {
#if !defined(NOT_BBS)
			giveup_timeslice();
#endif // #if !defined(NOT_BBS)
			write(hDestFile, (void *) pBuffer, i);
			i = read(hSourceFile, (void *) pBuffer, 16384);
		}

		hSourceFile=close(hSourceFile);
		hDestFile=close(hDestFile);
		BbsFreeMemory(pBuffer);
	}

	// I'm not sure about the logic here since you would think we should return true
	// in the last block, and false here.  This seems fishy
	return true;
}

bool WFile::MoveFile( const std::string sourceFileName, const std::string destFileName ) {
	//TODO: Atani needs to see if Rushfan buggered up this implementation
	if ( CopyFile( sourceFileName, destFileName ) ) {
		return Remove( sourceFileName );
	}
	return false;
}
