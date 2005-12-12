#ifndef __INCLUDED_WTEXTFILE_H__
#define __INCLUDED_WTEXTFILE_H__
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


#if defined( _WIN32 )
#define _CRT_SECURE_NO_DEPRECATE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#endif 

#include <cstdio>
#include <cstring>
#include <string>


class WTextFile
{

public:
    WTextFile( const char* pszFileName, const char* pszFileMode );
    WTextFile( const char* pszDirectoryName, const char* pszFileName, const char* pszFileMode );
    WTextFile( std::string& strFileName, const char* pszFileMode );
    operator FILE*() const { return m_hFile; }
    bool Open( const char* pszFileName, const char* pszFileMode );
    bool Close();
    bool IsOpen() { return m_hFile != NULL; }
    bool IsEndOfFile() { return feof(m_hFile) ? true : false; }
    int Write( const char *pszText ) { return fputs( pszText, m_hFile ); }
    int WriteFormatted( const char *pszFormatText, ... );
    bool ReadLine( char *pszBuffer, int nBufferSize ) { return (fgets(pszBuffer, nBufferSize, m_hFile) != NULL) ? true : false; }
    bool ReadLine( std::string &buffer );
    long GetPosition() { return ftell(m_hFile); }
    const char* GetFullPathName()
    {
        return m_szFileName;
    }

public:
    ~WTextFile();

private:
    char    m_szFileName[ MAX_PATH + 1 ];
    char    m_szFileMode[ 10 ];
    FILE*   m_hFile;
};



#endif // __INCLUDED_WTEXTFILE_H__