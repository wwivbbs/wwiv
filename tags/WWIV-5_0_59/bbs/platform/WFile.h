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

#ifndef __INCLUDED_PLATFORM_WFILLE_H__
#define __INCLUDED_PLATFORM_WFILLE_H__

#if defined( _MSC_VER ) && !defined( _CRT_SECURE_NO_DEPRECATE )
#define _CRT_SECURE_NO_DEPRECATE
#endif	// _MSC_VER 


#include <string>
#include "WStringUtils.h"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

class WLogger
{
public:
    /////////////////////////////////////////////////////////////////////////
    //
    // Member functions
    //

    WLogger() {}
    virtual ~WLogger() {}
    virtual bool LogMessage( const char* pszFormat, ... ) = 0;
};



/**
 * WFile - File I/O Class.
 */
class WFile
{

public:
    /////////////////////////////////////////////////////////////////////////
    //
    // Constants
    //

    static const int modeDefault;
    static const int modeUnknown;
    static const int modeAppend;
    static const int modeBinary;
    static const int modeCreateFile;
    static const int modeReadOnly;
    static const int modeReadWrite;
    static const int modeText;
    static const int modeWriteOnly;
    static const int modeTruncate;

    static const int shareUnknown;
    static const int shareDenyReadWrite;
    static const int shareDenyWrite;
    static const int shareDenyRead;
    static const int shareDenyNone;

    static const int permUnknown;
    static const int permWrite;
    static const int permRead;
    static const int permReadWrite;

    static const int seekBegin;
    static const int seekCurrent;
    static const int seekEnd;

    static const int invalid_handle;

    static const char pathSeparatorChar;
    static const char separatorChar;

private:

    int     m_hFile;
    bool    m_bOpen;
    bool    m_bCloseOnExit;
    char    m_szFileName[ MAX_PATH + 1 ];
    static  WLogger* m_pLogger;
    static  int m_nDebugLevel;

    void init();

public:

    /////////////////////////////////////////////////////////////////////////
    //
    // Constructor/Destructor
    //

    WFile();
    WFile( const char* pszFileName );
    WFile( const char* pszDirName, const char *pszFileName );
	WFile( std::string& strFileName );
	virtual ~WFile();

    /////////////////////////////////////////////////////////////////////////
    //
    // Public Member functions
    //

    virtual bool SetName( const char* pszFileName );
    virtual bool SetName( const char* pszDirName, const char *pszFileName );

    virtual bool Open( int nFileMode = WFile::modeDefault, int nShareMode = WFile::shareUnknown, int nPermissions = WFile::permUnknown );
    virtual void Close();
    virtual bool IsOpen() const { return m_bOpen; }

    virtual int  Read( void * pBuffer, int nCount );
    virtual int  Write( void * pBuffer, int nCount );

    virtual long GetLength();
    virtual long Seek( long lOffset, int nFrom );
    virtual void SetLength( long lNewLength );

    virtual bool Exists() const;
    virtual bool Delete( bool bUseTrashCan = false );

    virtual void SetCloseOnExit( bool bCloseOnExit );

    virtual bool IsDirectory();
    virtual bool IsFile();

    virtual bool SetFilePermissions( int nPermissions );
    virtual time_t GetFileTime();

    virtual char *GetParent()
    {
		char *tmpCopy = WWIV_STRDUP(m_szFileName);
        char *p = &tmpCopy[strlen(tmpCopy)-1];
	    while(*p != WFile::pathSeparatorChar)
        {
            p--;
        }
	    *p = '\0';
	    return(tmpCopy);
    }

    virtual char *GetName()
    {
        char *p = &m_szFileName[strlen(m_szFileName)-1];
	    while(*p != WFile::pathSeparatorChar)
        {
            p--;
        }
	    p++;
	    return(p);
    }

    virtual char* GetFullPathName()
    {
        return m_szFileName;
    }

private:

    /////////////////////////////////////////////////////////////////////////
    //
    // Private Implemtation Details
    //

    virtual bool IsCloseOnExit() { return m_bCloseOnExit; }


public:

    /////////////////////////////////////////////////////////////////////////
    //
    // static functions
    //

    static bool Remove( const char *pszFileName );
	static bool Remove( const char *pszDirectoryName, const char *pszFileName );
    static bool Rename( const char *pszOrigFileName, const char* pszNewFileName );
    static bool Exists( const char *pszFileName );
    static bool ExistsWildcard( const char *pszWildCard );
    static bool SetFilePermissions( const char *pszFileName, int nPermissions );
    static bool IsFileHandleValid( int hFile );

    static void SetLogger( WLogger* pLogger ) { m_pLogger = pLogger; }
    static void SetDebugLevel( int nDebugLevel ) { m_nDebugLevel = nDebugLevel; }
};



#endif // __INCLUDED_PLATFORM_WFILLE_H__
