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
#include "version.h"

#define TCP_TIME 1
#define BASE_TIME 2208988800L

unsigned long ntime(SOCKET socket)
{
    // Wait for buffer to fill up a bit.
	printf( "Sending request, waiting for response from remote server\n");
    Sleep( 500 );

    for ( ;; )
    {
        if ( sock_tbused( socket ) )
        {
            unsigned long lTempTime;
            int nNumRead = recv( socket, ( char * ) &lTempTime, 100, 0 );
            if ( nNumRead >= 4 )
            {
                printf( "lTime    = [%lu]\r\n", lTempTime );
                unsigned long lTime = ntohl( lTempTime );
                lTime -= BASE_TIME;
                return lTime;
            }
            break;
        }
    }
    return 0;
}


int ExecuteNTime( char* pszHostName )
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0 );
	if ( sock == INVALID_SOCKET )
	{
		printf("ERROR: Unable to create socket\r\n\n");
		return 1;
	}

	SOCKADDR_IN sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons((u_short) TIME_PORT );
	sockAddr.sin_addr.s_addr = inet_addr( pszHostName );
	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost;
		lphost = gethostbyname( pszHostName );
		if (lphost != NULL)
		{
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		}
		else
		{
			printf("\xFE Error : Cannot resolve host %s\r\n\n", pszHostName);
			return 1;
		}
	}

	if ( connect( sock, ( SOCKADDR *) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR )
	{
		printf("\xFE Error : Unable to connect to %s\r\n\n", pszHostName);
		return 1;
	}

    unsigned long lNewTime = (unsigned long) ntime( sock );\
    time_t lMyTime;
    time( &lMyTime );
    struct tm *newtime = localtime( ( time_t* ) &lNewTime );

    printf( "Date: [mm/dd/yyyy] = [%d/%d/%d]\r\n", newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_year + 1900 );
    printf( "Time: [hh:mi:ss] = [%d:%d:%d]\r\n", newtime->tm_hour, newtime->tm_min, newtime->tm_sec );
    closesocket( sock );

    if ( lNewTime == 0L )
    {
        printf( "Unable to get time\r\n" );
        return 1;
    }

//    printf( "lNewTime = [%lu]\r\n", lNewTime );
//    printf( "MyTime   = [%lu]\r\n", lMyTime );

/*
    newtime -= BASE_TIME;
    unixtodos(newtime, &dstruct, &tstruct);
    settime(&tstruct);
    setdate(&dstruct);
    sprintf(s, "%s", ctime((unsigned long *) & newtime));
    s[strlen(s) - 1] = '\0';
    printf("clock set to %s.\r\n", s);
*/
    return 0;
}


int main( int argc, char* argv[] )
{
	if ( !InitializeWinsock())
	{
		// %%TODO: Whine about this...
		return 1;
	}

    if ( argc != 2 )
    {
        // %%TODO: Print Usage
        printf(" NTime: Incorrect Command Line Arguments Specified\r\n\n" );
        return 1;
    }

    int ret = ExecuteNTime( argv[1] );
	WSACleanup();
    return ret;

}