/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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

#ifndef NOT_BBS
#include "wwiv.h"
#endif

#include <fcntl.h>

/////////////////////////////////////////////////////////////////////////////
//
// Constants
//

const int WFile::modeDefault        = ( _O_RDWR | _O_BINARY );
const int WFile::modeUnknown        = -1;
const int WFile::modeAppend         = _O_APPEND;
const int WFile::modeBinary         = _O_BINARY;
const int WFile::modeCreateFile     = _O_CREAT;
const int WFile::modeReadOnly       = _O_RDONLY;
const int WFile::modeReadWrite      = _O_RDWR;
const int WFile::modeText           = _O_TEXT;
const int WFile::modeWriteOnly      = _O_WRONLY;
const int WFile::modeTruncate       = _O_TRUNC;

const int WFile::shareUnknown       = -1;
const int WFile::shareDenyReadWrite = SH_DENYRW;
const int WFile::shareDenyWrite     = SH_DENYWR;
const int WFile::shareDenyRead      = SH_DENYRD;
const int WFile::shareDenyNone      = SH_DENYNO;

const int WFile::permUnknown        = -1;
const int WFile::permWrite          = _S_IWRITE;
const int WFile::permRead           = _S_IREAD;
const int WFile::permReadWrite      = ( _S_IREAD | _S_IWRITE );

const int WFile::seekBegin          = SEEK_SET;
const int WFile::seekCurrent        = SEEK_CUR;
const int WFile::seekEnd            = SEEK_END;

const int WFile::invalid_handle     = -1;

const char WFile::pathSeparatorChar = '\\';
const char WFile::separatorChar     = ';';

WLogger*    WFile::m_pLogger;
int         WFile::m_nDebugLevel;


#define WAIT_TIME 10
#define TRIES 100


/////////////////////////////////////////////////////////////////////////////
//
// Constructors/Destructors
//

WFile::WFile()
{
    init();
}


WFile::WFile( const char* pszFileName )
{
    init();
    this->SetName( pszFileName );
}


WFile::WFile( const char* pszDirName, const char *pszFileName )
{
    init();
    this->SetName( pszDirName, pszFileName );
}


WFile::WFile( std::string& strFileName )
{
	init();
	this->SetName( strFileName.c_str() );
}


void WFile::init()
{
    m_bOpen                 = false;
    m_bCloseOnExit          = true;
    m_hFile                 = WFile::invalid_handle;
    ZeroMemory( m_szFileName, MAX_PATH + 1 );
}


WFile::~WFile()
{
    if ( this->IsOpen() && this->IsCloseOnExit() )
    {
        this->Close();
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// Member functions
//


bool WFile::SetName( const char* pszFileName )
{
    WWIV_ASSERT( pszFileName );
    strncpy( m_szFileName, pszFileName, MAX_PATH );
    return true;
}


bool WFile::SetName( const char* pszDirName, const char *pszFileName )
{
    WWIV_ASSERT( pszDirName );
    WWIV_ASSERT( pszFileName );
    std::string strFileName;
	strFileName.append( pszDirName );
    int nDirLen = strlen(pszDirName);
    if ( ( nDirLen > 0 ) && ( pszDirName[ nDirLen - 1 ] == '\\' ) )
    {
        strFileName.append( pszFileName );
    }
    else
    {
        strFileName.append( WWIV_FILE_SEPERATOR_STRING ).append( pszFileName );
    }
    return this->SetName( strFileName.c_str() );
}


bool WFile::Open( int nFileMode, int nShareMode, int nPermissions )
{
    WWIV_ASSERT( this->IsOpen() == false );

    // Set default permissions
    if ( nPermissions == WFile::permUnknown )
    {
		// modeReadOnly is 0 on Win32, so the test with & always fails.
		// This means that readonly is always allowed on Win32
		// hence defaulting to permRead always
        nPermissions = WFile::permRead;
        if ( ( nFileMode & WFile::modeReadWrite ) ||
             ( nFileMode & WFile::modeWriteOnly ) )
        {
            nPermissions |= WFile::permWrite;
        }
    }

    // Set default share mode
    if ( nShareMode == WFile::shareUnknown )
    {
        nShareMode = shareDenyWrite;
        if ( ( nFileMode & WFile::modeReadWrite ) ||
             ( nFileMode & WFile::modeWriteOnly ) ||
             ( nPermissions & WFile::permWrite ) )
        {
            nShareMode = WFile::shareDenyReadWrite;
        }
    }

    WWIV_ASSERT( nShareMode   != WFile::shareUnknown );
    WWIV_ASSERT( nFileMode    != WFile::modeUnknown  );
    WWIV_ASSERT( nPermissions != WFile::permUnknown  );

    if ( m_nDebugLevel > 2)
	{
        m_pLogger->LogMessage( "\rSH_OPEN %s, access=%u\r\n", m_szFileName, nFileMode );
	}

    m_hFile = _sopen(m_szFileName, nFileMode, nShareMode, nPermissions);
	if ( m_hFile < 0 )
	{
		int nCount = 1;
		if (access(m_szFileName, 0) != -1)
		{
			WWIV_Delay(WAIT_TIME);
			m_hFile = _sopen(m_szFileName, nFileMode, nShareMode, nPermissions);
			while ( ( ( m_hFile < 0 ) && ( errno == EACCES ) ) &&  (nCount < TRIES ) )
			{
                Sleep( (nCount % 2) ? WAIT_TIME : 0 );
				if (m_nDebugLevel > 0)
				{
					m_pLogger->LogMessage("\rWaiting to access %s %d.  \r", m_szFileName, TRIES - nCount);
				}
				nCount++;
				m_hFile = _sopen(m_szFileName, nFileMode, nShareMode, nPermissions);
			}


            if ( ( m_hFile < 0 ) && ( m_nDebugLevel > 0 ) )
			{
				m_pLogger->LogMessage("\rThe file %s is busy.  Try again later.\r\n", m_szFileName);
			}
		}
	}


    if ( m_nDebugLevel > 1 )
	{
		m_pLogger->LogMessage("\rSH_OPEN %s, access=%u, handle=%d.\r\n", m_szFileName, nFileMode, m_hFile);
	}

    if ( WFile::IsFileHandleValid( m_hFile ) )
    {
        m_bOpen = true;
        return true;
    }
    else
    {
        m_bOpen = false;
        return false;
    }
}


void WFile::Close()
{
    if ( WFile::IsFileHandleValid( m_hFile ) )
    {
        close( m_hFile );
        m_hFile = WFile::invalid_handle;
        m_bOpen = false;
    }
}


int WFile::Read( void * pBuffer, int nCount )
{
    int nRet = read( m_hFile, pBuffer, nCount );
	if ( nRet == -1 )
	{
        std::cout << "[DEBUG: errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
	}
	// TODO: Make this an WWIV_ASSERT once we get rid of these issues
	return nRet;
}


int WFile::Write( void * pBuffer, int nCount )
{
    int nRet = write( m_hFile, pBuffer, nCount );
	if ( nRet == -1 )
	{
        std::cout <<"[DEBUG: errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
	}
	// TODO: Make this an WWIV_ASSERT once we get rid of these issues
	return nRet;
}


long WFile::GetLength()
{
    bool bOpenedHere = false;
    if ( !this->IsOpen() )
    {
        bOpenedHere = true;
        Open();
    }
    WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

    long lFileSize = filelength( m_hFile );
    if ( bOpenedHere )
    {
        Close();
    }
    return lFileSize;
}


long WFile::Seek( long lOffset, int nFrom )
{
    WWIV_ASSERT( nFrom == WFile::seekBegin || nFrom == WFile::seekCurrent || nFrom == WFile::seekEnd );
    WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

    return lseek( m_hFile, lOffset, nFrom );
}


void WFile::SetLength( long lNewLength )
{
    WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );
    _chsize( m_hFile, lNewLength );
}


bool WFile::Exists() const
{
    return WFile::Exists( m_szFileName );
}


bool WFile::Delete( bool bUseTrashCan )
{
	UNREFERENCED_PARAMETER( bUseTrashCan );
    if ( this->IsOpen() )
    {
        this->Close();
    }

    return DeleteFile( m_szFileName ) ? true : false;
}


void WFile::SetCloseOnExit( bool bCloseOnExit )
{
    m_bCloseOnExit = bCloseOnExit;
}


bool WFile::IsDirectory()
{
    DWORD dwAttributes = GetFileAttributes( m_szFileName );
    return ( dwAttributes & FILE_ATTRIBUTE_DIRECTORY ) ? true : false;
}


bool WFile::IsFile()
{
    return this->IsDirectory() ? false : true;
}


bool WFile::SetFilePermissions( int nPermissions )
{
    return( _chmod( m_szFileName, nPermissions ) == 0 ) ? true : false;
}

time_t WFile::GetFileTime()
{
    bool bOpenedHere = false;
    if ( !this->IsOpen() )
    {
        bOpenedHere = true;
        Open( WFile::modeReadOnly );
    }
    WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

    struct _stat buf;
    int nFileTime = ( _fstat( m_hFile, &buf ) == -1 ) ? 0 : buf.st_mtime;

    if ( bOpenedHere )
    {
        Close();
    }
    return nFileTime;
}


/////////////////////////////////////////////////////////////////////////////
//
// Static functions
//

bool WFile::Remove( const char *pszFileName )
{
    WWIV_ASSERT( pszFileName );
    return ( DeleteFile( pszFileName ) ) ? true : false;
}


bool WFile::Remove( const char *pszDirectoryName, const char *pszFileName )
{
	WWIV_ASSERT( pszDirectoryName );
	WWIV_ASSERT( pszFileName );

	std::string strFullFileName = pszDirectoryName;
	strFullFileName += pszFileName;
	return WFile::Remove( strFullFileName.c_str() );
}


bool WFile::Rename( const char *pszOrigFileName, const char* pszNewFileName )
{
    WWIV_ASSERT( pszOrigFileName );
    WWIV_ASSERT( pszNewFileName );
    return MoveFile( pszOrigFileName, pszNewFileName ) ? true : false;
}


bool WFile::Exists( const char *pszFileName )
{
    WWIV_ASSERT( pszFileName );

    // If one of these assertions are thrown, then replace this call with
    // WFile::EcistsWildcard since we are obviously using the wrong call here.
    WWIV_ASSERT( strstr( pszFileName, "*" ) == NULL );
    WWIV_ASSERT( strstr( pszFileName, "?" ) == NULL );

    return ( access( pszFileName, 0 ) != -1 ) ? true : false;
}


bool WFile::ExistsWildcard( const char *pszWildCard )
{
	WWIV_ASSERT( pszWildCard );

	WFindFile fnd;
	return (fnd.open(pszWildCard, 0) == false) ? false : true;
}


bool WFile::SetFilePermissions( const char *pszFileName, int nPermissions )
{
    WWIV_ASSERT( pszFileName );
    return ( _chmod( pszFileName, nPermissions ) == 0 ) ? true : false;
}


bool WFile::IsFileHandleValid( int hFile )
{
    return ( hFile != WFile::invalid_handle ) ? true : false;
}



