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
#include "wwiv.h"
#include "WTextFile.h"

FILE *fsh_open(const char *path, char *mode);


WTextFile::WTextFile( const char* pszFileName, const char* pszFileMode )
{
    m_hFile = NULL;
    strcpy( m_szFileName, pszFileName );
    strcpy( m_szFileName, pszFileMode );
    m_hFile = fsh_open( m_szFileName, m_szFileMode);
}

bool WTextFile::Close()
{
    if ( m_hFile != NULL )
    {
        fclose(m_hFile);
        m_hFile = NULL;
        return true;
    }
    return false;
}


int WTextFile::WriteFormatted( const char *pszFormatText,... )
{
    va_list ap;
    char szBuffer[ 4096 ];

    va_start( ap, pszFormatText );
    vsnprintf( szBuffer, sizeof( szBuffer ), pszFormatText, ap );
    va_end( ap );
    return Write( szBuffer );
}


bool WTextFile::ReadLine( std::string &buffer )
{
    char szBuffer[4096];
    char *pszBuffer = fgets(szBuffer, sizeof(szBuffer), m_hFile);
    buffer = szBuffer;
    return ( pszBuffer != NULL );
}


WTextFile::~WTextFile()
{
    Close();
}
