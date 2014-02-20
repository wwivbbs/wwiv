/*
 * Copyright 2001,2004 Frank Reid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 

#include "pppproj.h"
#include "socketwrapper.h"


#define SOCK_GETS_BUFFER_SIZE 65536
static char szOverflowBuffer[ SOCK_GETS_BUFFER_SIZE ];



int sock_tbused( SOCKET sock )
{
	// here we are only returning 1 or 0.. also, MS says ioctlsocket is bad, but it's 
	// the easiest way to do things with the way this code is structured...  A future
	// revision should make a seperate receive thread that stores the data in a buffer
	// like we do for WWIV5.
	u_long lResult = 0;
	int nRet = ioctlsocket( sock, FIONREAD, &lResult );
	if ( nRet == SOCKET_ERROR )
	{
		return 0;
	}
	return ( lResult > 0 ) ? 1 : 0;
}



void sock_flushbuffer( SOCKET s )
{
	char szBuffer[8192];
	Sleep( 500 );
	while ( sock_tbused( s ) > 0 )
	{
		/* int nRet = */ recv( s, szBuffer, 8192, 0 );
		Sleep(100);
	}
	ZeroMemory( szOverflowBuffer, SOCK_GETS_BUFFER_SIZE );
}


int sock_read( SOCKET s, char *pszText, int nBufSize )
{
	int nCurSize = 0;
	ZeroMemory( pszText, nBufSize );
	while ( sock_tbused( s ) > 0 && nCurSize < nBufSize )
	{
		char szTemp[8192];
		ZeroMemory( szTemp, sizeof(szTemp) );
		int nStrLen = recv( s, szTemp, sizeof(szTemp), 0 );
		if ( nStrLen < 0 || nStrLen == SOCKET_ERROR )
		{
			return nCurSize;
		}
		if ( ( nStrLen + nCurSize ) > nBufSize )
		{
			return nCurSize;
		}
		strcat( pszText, szTemp );
		nCurSize = strlen( pszText );
		int nCount = 0;
		while (sock_tbused( s ) <= 0 && nCount++ < 30)
		{
			Sleep( 500 );
		}
	}
	return nCurSize;
}

int sock_gets( SOCKET s, char * pszText, int nBufSize )
{
	int nRet = 0, nSleepCount = 0;
	bool bSkipSocketCall = false;
	for( ;; )
	{
		bSkipSocketCall = ( sock_tbused( s ) == 0 ) ? true : false;
		if (sock_tbused( s ) == 0 && szOverflowBuffer[0] == '\0')
		{
			Sleep(1000);
			//printf("(SLEEP)");
		}
		else if ( szOverflowBuffer[0] != '\0' && ( strlen( szOverflowBuffer ) + nBufSize ) > SOCK_GETS_BUFFER_SIZE )
		{
			printf("\n *DEBUG* Skipping read since buffer mostly full \n");
			bSkipSocketCall = true;
			break;
		}
		else if ( strchr( szOverflowBuffer, '\n' ) == NULL  && bSkipSocketCall )
		{
			printf( "\n *DEBUG* Sleeping since no \\n is in the buffer \n" );
			Sleep( 1000 );
		}
		else
		{
			break;
		}

		nSleepCount++;
		if ( nSleepCount > 60 )
		{
			// At 60 1 second loops... we've waited one minute for a response...
			bSkipSocketCall = true;
			printf("\n *DEBUG* Bailing on waiting... it's been 5 seconds...\n");
			break;
		}
	}

	if (!bSkipSocketCall)
	{
		
		nRet = recv( s, pszText, nBufSize, 0 );
		if ( nRet == SOCKET_ERROR || nRet == 0 )
		{
			strcpy( pszText, szOverflowBuffer );
			char *start = szOverflowBuffer;
			char *p = strchr( szOverflowBuffer, '\n' );
			if ( p == NULL )
			{
				ZeroMemory( szOverflowBuffer, SOCK_GETS_BUFFER_SIZE );
			}
			else
			{
				memmove( start, p+2, SOCK_GETS_BUFFER_SIZE - ( p - start + 1 ) );
			}
            
            //printf("\n *DEBUG* sock_gets=[%s]", pszText );
			return strlen( pszText );
		}
		pszText[nRet] = '\0';

		if ( strlen( szOverflowBuffer ) > 0 )
		{
			strcat( szOverflowBuffer, pszText );
		}
		else
		{
			strcpy( szOverflowBuffer, pszText );
		}
	}

	char *p		= szOverflowBuffer;
	char *start = szOverflowBuffer;
	int curPos	= 0;
	for( ;; )
	{
		switch ( *p )
		{
		case '\r':
			if ( *( p+1 ) == '\n' )
			{
				*p = '\0';
				strcpy( pszText, start );
				memmove( start, p+2, SOCK_GETS_BUFFER_SIZE - ( p - start + 1 ) );
				if ( *start == '\0' )
				{
					// Clean up
					ZeroMemory( szOverflowBuffer, SOCK_GETS_BUFFER_SIZE );
				}
                //printf("\n *DEBUG* Debug : sock_gets=[%s]", pszText );
				return( strlen( pszText ) );
			}
			break;
		case '\n':
			*p = '\0';
			strcpy( pszText, start );
			memmove( start, p+2, SOCK_GETS_BUFFER_SIZE - ( p - start + 1 ) );
			if ( *start == '\0' )
			{
				// Clean up
				ZeroMemory( szOverflowBuffer, SOCK_GETS_BUFFER_SIZE );
			}
            //printf("\n *DEBUG* Debug : sock_gets=[%s]", pszText );
			return( strlen( pszText ) );
			break;
		case '\0':
			strcpy( pszText, start );
			// Clean up
			ZeroMemory( szOverflowBuffer, SOCK_GETS_BUFFER_SIZE );

            //printf("\n *DEBUG* sock_gets=[%s]", pszText );
			return( strlen( pszText ) );
		default:
			break;
		}
		*p++;
		curPos++;
	}
}



/* Always in ASCII mode */
int sock_puts( SOCKET s, const char *pszText )
{
	sock_flushbuffer( s );
	int nBufLen = sock_write( s, pszText );
	send( s, "\r\n", 2, 0 );
    //printf("\n *DEBUG* sock_puts=[%s]", pszText );
	return nBufLen + 2;
}


int sock_write( SOCKET s, const char *pszText )
{
	send( s, pszText, strlen( pszText ), 0 );
	return strlen( pszText );
}


bool InitializeWinsock()
{
	WORD wVersionRequested = MAKEWORD( 1, 1 );
	WSADATA wsaData;
	int nRC = WSAStartup( wVersionRequested, &wsaData );	
	if ( nRC )
	{
		WSACleanup();
		return false;
	}
	return true;
}


#undef SOCK_GETS_BUFFER_SIZE


 
