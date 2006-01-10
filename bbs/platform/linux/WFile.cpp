/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
    memset( m_szFileName, 0, MAX_PATH + 1 );
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
    std::string strFileName = pszDirName;
    strFileName.append( WWIV_FILE_SEPERATOR_STRING ).append( pszFileName );
    return this->SetName( strFileName.c_str() );
}


bool WFile::Open( int nFileMode, int nShareMode, int nPermissions )
{
    WWIV_ASSERT( this->IsOpen() == false );

    // Set default permissions
    if ( nPermissions == WFile::permUnknown )
    {
        // See platform/WIN32/WFile.cpp for notes on why this is not 0.
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
    }

    WWIV_ASSERT( nShareMode   != WFile::shareUnknown );
    WWIV_ASSERT( nFileMode    != WFile::modeUnknown  );
    WWIV_ASSERT( nPermissions != WFile::permUnknown  );

    m_hFile =  open( m_szFileName, nFileMode, 0644 );
	if (m_hFile < 0)
	{
		int count = 1;
		if ( access( m_szFileName, 0 ) != -1 )
		{
			WWIV_Delay( WAIT_TIME );
			m_hFile = open( m_szFileName, nFileMode, nShareMode );
			while ( ( m_hFile < 0 && errno == EACCES ) && count < TRIES )
			{
				WWIV_Delay( ( count % 2 ) ? WAIT_TIME : 0 );
				if ( sess->GetGlobalDebugLevel() > 0 )
				{
					std::cout << "\rWaiting to access " << m_szFileName << " " << TRIES - count << ".  \r";
				}
				count++;
				m_hFile = open( m_szFileName, nFileMode, nShareMode );
			}
		}
	}

    m_bOpen = WFile::IsFileHandleValid( m_hFile );
    if( m_bOpen )
    {
        flock( m_hFile, (nShareMode & shareDenyWrite) ? LOCK_EX : LOCK_SH );
    }
    return m_bOpen;
}


void WFile::Close()
{
    if ( WFile::IsFileHandleValid( m_hFile ) )
    {
        flock(m_hFile, LOCK_UN);
        close( m_hFile );
        m_hFile = WFile::invalid_handle;
        m_bOpen = false;
    }
}


int WFile::Read( void * pBuffer, int nCount )
{
    return read( m_hFile, pBuffer, nCount );
}


int WFile::Write( void * pBuffer, int nCount )
{
    return write( m_hFile, pBuffer, nCount );
}


long WFile::GetLength()
{
    WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

    if (! IsOpen() )
    {
        return -1;
    }
    return filelength( m_hFile );
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
    ftruncate( m_hFile, lNewLength );
}


bool WFile::Exists() const
{
    return WFile::Exists( m_szFileName );
}


bool WFile::Delete( bool bUseTrashCan )
{
    if( this->IsOpen() )
    {
        this->Close();
    }
    return ( unlink(m_szFileName ) == 0 ) ? true : false;
}


void WFile::SetCloseOnExit( bool bCloseOnExit )
{
    m_bCloseOnExit = bCloseOnExit;
}


bool WFile::IsDirectory()
{
    struct stat statbuf;
    stat(m_szFileName, &statbuf);
    return(statbuf.st_mode & 0x0040000 ? true : false);
}


bool WFile::IsFile()
{
    return(!IsDirectory());
}


bool WFile::SetFilePermissions( int nPermissions )
{
    return ( chmod( m_szFileName, nPermissions ) == 0 ) ? true : false;
}


time_t WFile::GetFileTime()
{
    bool bOpenedHere = false;
    if ( !this->IsOpen() )
    {
        bOpenedHere = true;
        Open();
    }
    WWIV_ASSERT( WFile::IsFileHandleValid( m_hFile ) );

    struct stat buf;
    int nFileTime = ( fstat( m_hFile, &buf ) == -1 ) ? 0 : buf.st_mtime;

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
    return ( unlink(pszFileName) ? false : true );
}

bool WFile::Remove( const char *pszDirectoryName, const char *pszFileName )
{
    WWIV_ASSERT( pszDirectoryName );
    WWIV_ASSERT( pszFileName );
    std::string strFullFileName = pszDirectoryName;
    strFullFileName += pszFileName;
    return WFile::Remove(strFullFileName.c_str());
}

bool WFile::Rename( const char *pszOrigFileName, const char* pszNewFileName )
{
    WWIV_ASSERT( pszOrigFileName );
    WWIV_ASSERT( pszNewFileName );
    return rename( pszOrigFileName, pszNewFileName );
}


bool WFile::Exists( const char *pszFileName )
{
    WWIV_ASSERT( pszFileName );
    struct stat buf;
    return ( stat(pszFileName, &buf) ? false : true );
}


bool WFile::SetFilePermissions( const char *pszFileName, int nPermissions )
{
    WWIV_ASSERT( pszFileName );
    return ( chmod( pszFileName, nPermissions ) == 0 ) ? true : false;
}


bool WFile::IsFileHandleValid( int hFile )
{
    return ( hFile != WFile::invalid_handle ) ? true : false;
}


bool WFile::ExistsWildcard( const char *pszWildCard )
{
    WWIV_ASSERT( pszWildCard );
    WFindFile fnd;
    return(fnd.open(pszWildCard, 0));
}


