/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
 * Attempts to allocate lNumBytes (+1) bytes on the heap, returns ptr to memory
 * if successful.  Exits the BBS if it fails to allocate the required memory
 * <P>
 * @param lNumBytes Number of bytes to allocate
 * @param pszComment Friendly name of the memory region to be allocated
 * @return (void *) The memory that is allocated
 */
void* BbsAllocWithComment( size_t lNumBytes, const char *pszComment ) {
	WWIV_ASSERT( lNumBytes >= 0 );
	if ( lNumBytes <= 0 ) {
		lNumBytes = 1;
	}
	void* pBuffer = bbsmalloc( lNumBytes );
#ifndef NOT_BBS
	if ( !pBuffer ) {
		char szBuffer[ 255 ];
		snprintf( szBuffer, sizeof( szBuffer ), "Insufficient memory (%ld bytes) for %s.\n", lNumBytes, pszComment );
		std::cout << szBuffer;
		WWIV_OutputDebugString( szBuffer );
		GetApplication()->AbortBBS();
	}
	memset( ( void * ) pBuffer, 0, lNumBytes );
#endif
	return pBuffer;
}



/**
 * Attempts to allocate nbytes (+1) bytes on the heap, returns ptr to memory
 * if successful.  Writes to sysoplog if unable to allocate the required memory
 * <P>
 * @param lNumBytes Number of bytes to allocate
 */
void *BbsAllocA( size_t lNumBytes ) {
	WWIV_ASSERT( lNumBytes > 0 );

	void* pBuffer = bbsmalloc( lNumBytes + 1 );
	memset( pBuffer, 0, lNumBytes );
	WWIV_ASSERT( pBuffer );
#ifndef NOT_BBS
	if ( pBuffer == NULL ) {
		GetSession()->bout << "\r\nNot enough memory, needed " << lNumBytes << " bytes.\r\n\n";
		char szLogLine[ 255 ];
		snprintf( szLogLine, sizeof( szLogLine ), "!!! Ran out of memory, needed %ld bytes !!!", lNumBytes );
		sysoplog( szLogLine );
		WWIV_OutputDebugString( szLogLine );
	}
#endif
	return pBuffer;
}


char **BbsAlloc2D(int nRow, int nCol, int nSize ) {
	WWIV_ASSERT( nSize > 0 );
#ifndef NOT_BBS
	const char szErrorMessage[] = "WWIV: BbsAlloc2D: Memory Allocation Error -- Please inform the SysOp!";
#endif

	char* pdata = reinterpret_cast<char*>( calloc( nRow * nCol, nSize ) );
	if ( pdata == NULL ) {
#ifndef NOT_BBS
		GetSession()->bout << szErrorMessage;
		WWIV_OutputDebugString( szErrorMessage );
		sysoplog( szErrorMessage );
		hangup = true;
		GetSession()->bout.NewLine();
#endif
		return NULL;
	}
	char** prow = static_cast< char ** >( BbsAllocA( nRow * sizeof( char * ) ) );
	if ( prow == ( char ** ) NULL ) {
#ifndef NOT_BBS
		GetSession()->bout << szErrorMessage;
		WWIV_OutputDebugString( szErrorMessage );
		sysoplog( szErrorMessage );
		hangup = true;
		GetSession()->bout.NewLine();
#endif
		BbsFreeMemory( pdata );
		return NULL;
	}
	for ( int i = 0; i < nRow; i++ ) {
		prow[i] = pdata;
		pdata += nSize * nCol;
	}
	return prow;
}


void BbsFree2D( char **pa ) {
	if ( pa ) {
		BbsFreeMemory( *pa );
		BbsFreeMemory( pa );
	}
}


