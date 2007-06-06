#ifndef __INCLUDED_WTEXTFILE_H__
#define __INCLUDED_WTEXTFILE_H__
/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2005-2007, WWIV Software Services             */
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
#endif 

#include <cstdio>
#include <cstring>
#include <string>


class WTextFile
{

public:
    WTextFile( const std::string fileName, const std::string fileMode );
    WTextFile( const std::string directoryName, const std::string fileName, const std::string fileMode );
    operator FILE*() const { return m_hFile; }
    bool Open( const std::string fileName, const std::string fileMode );
    bool Close();
    bool IsOpen() { return m_hFile != NULL; }
    bool IsEndOfFile() { return feof(m_hFile) ? true : false; }
    int Write( const char *pszText ) { return fputs( pszText, m_hFile ); }
    int WriteChar( char ch ) { return fputc( ch, m_hFile ); }
    int WriteFormatted( const char *pszFormatText, ... );
    int WriteBinary( const void *pBuffer, size_t nSize ) { return fwrite(pBuffer, nSize, 1, m_hFile ); }
    bool ReadLine( char *pszBuffer, int nBufferSize ) { return (fgets(pszBuffer, nBufferSize, m_hFile) != NULL) ? true : false; }
    bool ReadLine( std::string &buffer );
    long GetPosition() { return ftell(m_hFile); }
    const char* GetFullPathName()
    {
        return m_fileName.c_str();
    }

public:
    ~WTextFile();

private:
    std::string m_fileName;
    std::string m_fileMode;
    FILE*   m_hFile;
    static FILE* OpenImpl( const char* pszFileName, const char* pszFileMode );
    static const int TRIES;
    static const int WAIT_TIME;

};



#endif // __INCLUDED_WTEXTFILE_H__

