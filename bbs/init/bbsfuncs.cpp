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

/**
 * Attempts to allocate nbytes (+1) bytes on the heap, returns ptr to memory
 * if successful.  Writes to sysoplog if unable to allocate the required memory
 * <P>
 * @param lNumBytes Number of bytes to allocate
 */
void *BbsAllocA( size_t lNumBytes )
{
    WWIV_ASSERT( lNumBytes > 0 );

    void* pBuffer = bbsmalloc( lNumBytes + 1 );
	memset( pBuffer, 0, lNumBytes );
    WWIV_ASSERT( pBuffer );
    if ( pBuffer == NULL )
    {
        printf( "!!! Ran out of memory, needed %ld bytes !!!", lNumBytes );
	exit(-1);
    }
    return pBuffer;
}

char *stripfn(const char *pszFileName)
{
    static char szStaticFileName[15];
    char szTempFileName[ MAX_PATH ];

	WWIV_ASSERT(pszFileName);

    int nSepIndex = -1;
    for (int i = 0; i < wwiv::stringUtils::GetStringLength(pszFileName); i++)
    {
        if ( pszFileName[i] == '\\' || pszFileName[i] == ':' || pszFileName[i] == '/' )
        {
            nSepIndex = i;
        }
    }
    if (nSepIndex != -1)
    {
        strcpy( szTempFileName, &( pszFileName[nSepIndex + 1] ) );
    }
    else
    {
        strcpy( szTempFileName, pszFileName );
    }
    for ( int i1 = 0; i1 < wwiv::stringUtils::GetStringLength( szTempFileName ); i1++ )
    {
        if ( szTempFileName[i1] >= 'A' && szTempFileName[i1] <= 'Z' )
        {
            szTempFileName[i1] = szTempFileName[i1] - 'A' + 'a';
        }
    }
    int j = 0;
    while ( szTempFileName[j] != 0 )
    {
        if ( szTempFileName[j] == SPACE )
        {
            strcpy( &szTempFileName[j], &szTempFileName[j + 1] );
        }
        else
        {
            ++j;
        }
    }
    strcpy( szStaticFileName, szTempFileName );
    return szStaticFileName;
}

