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


bool get_qotd(char* host, char *fn)
{
	char quote[8192];
	long quotelen = 0L;
	FILE *fp;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0 );
	if ( sock == INVALID_SOCKET )
	{
		printf("ERROR: Unable to create socket\n");
		return false;
	}
	
	SOCKADDR_IN sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons((u_short) QOTD_PORT );
	sockAddr.sin_addr.s_addr = inet_addr( host );
	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost;
		lphost = gethostbyname( host );
		if (lphost != NULL)
		{
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		}
		else
		{
			printf("\xFE Error : Cannot resolve host %s\n", host);
			return false;
		}
	}
	
	if ( connect( sock, ( SOCKADDR *) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR )
	{
		printf("\n \xFE Error : Unable to connect to %s\n", host);
		return false;
	}
	
	printf( "Connected to host [%s]\n", host );
	printf( "Waiting for buffer to fill...\n" );
	Sleep(1500);
	printf( "Sending request, waiting for response from remote server\n");
	//sock_write(sock, "\n");
	for( int numTries = 0; numTries < 30; numTries++ )
	{
		if (sock_tbused(sock)>0) 
		{
			quotelen = sock_read(sock, quote, sizeof(quote));
			quote[quotelen] = '\0';
			fprintf(stderr, "\n%s", quote);
			if ((quote[0] != 0) && (fn[0] != 0)) 
			{
				fp = fopen(fn, "wb+");
				if (fp != NULL) 
				{
					fprintf(fp, "%c1Quote of the Day%c0\r\n", 3, 3);
					fprintf(fp, quote);
					fclose(fp);
				}
			}
			closesocket(sock);
			printf("\n ** SUCCESS\n");
			return true;
		}
		printf(".");
		Sleep( 1000 );
	}
	printf("\n ** FAILURE, Response not received.\n" );
	return false;
}


int main(int argc, char **argv)
{
	if (argc < 3)
	{	
		// %%TODO: Show usage information here.
		return 1;
	}
	
	if ( !InitializeWinsock())
	{
		// %%TODO: Whine about this...
		return 1;
	}

    if (get_qotd(argv[1], argv[2]))
	{
		WSACleanup();
		return 0;
	}
	WSACleanup();
	return 1;
}
