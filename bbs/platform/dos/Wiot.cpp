/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/

#include "../../wwiv.h"
#include "../../circbuf.h"



cbuf_t WIOTelnet::inBuffer;
HANDLE WIOTelnet::hInBufferMutex;

void InboundTelnetProc(void *hSocketHandle);


bool WIOTelnet::setup(char parity, int wordlen, int stopbits, unsigned long baud)
{
    return (true);
}

unsigned int WIOTelnet::open()
{
	return(0);
}

void WIOTelnet::close()
{
	closesocket( sysinfo.hDuplicateSocket );
	closesocket( sysinfo.hSocket );
}

unsigned int WIOTelnet::putW(unsigned char ch)
{
	if (sysinfo.hSocket == INVALID_SOCKET)
	{
#ifdef _DEBUG
        printf("pw(INVALID-SOCKET = %d) ", sysinfo.hSocket);
#endif // _DEBUG
		return(0);
	}
	char szBuffer[5];
	szBuffer[0] = (char) ch;
	szBuffer[1] = '\0';
	if( ch == 255 )
	{
		szBuffer[1] = (char) ch;
		szBuffer[2] = '\0';
	}
	int nRet;
	while( 1 )
	{
		nRet = send( sysinfo.hSocket, szBuffer, strlen( szBuffer ), 0 );
		if( nRet == SOCKET_ERROR )
		{
			if( WSAGetLastError() != WSAEWOULDBLOCK )
			{
				if( WSAGetLastError() != WSAENOTSOCK )
				{
					printf("DEBUG: ERROR on send from putW() [%04u] [#%u]\n", WSAGetLastError(), ch);
				}
				return( 0 );
			}
		}
		else
		{
			return( 0 );
		}
		WWIV_Delay( 100 );
		WWIV_Delay( 0 );
	}
	if (nRet != 1)
	{
		printf("DEBUG: in putW(), expected to send 1 character, actually sent %d (CHAR = #%d)\n", nRet, ch);
	}
	return(0);
}

unsigned char WIOTelnet::getW()
{
	char ch = 0;
	WaitForSingleObject(WIOTelnet::hInBufferMutex, INFINITE);
	if (inBuffer.data_avail() == C_TRUE)
	{
		inBuffer.fetch(&ch);
	}
	ReleaseMutex(WIOTelnet::hInBufferMutex);
	return static_cast<unsigned char> (ch);
}


unsigned char WIOTelnet::dtr(bool raise)
{
	if (!raise)
	{
		closesocket(sysinfo.hDuplicateSocket);
		closesocket(sysinfo.hSocket);
		sysinfo.hSocket = INVALID_SOCKET;
		sysinfo.hDuplicateSocket = INVALID_SOCKET;
	}
	return(0);
}

void WIOTelnet::flushOut()
{
}

void WIOTelnet::purgeOut()
{
}

void WIOTelnet::purgeIn()
{
}

unsigned int WIOTelnet::put(unsigned char ch)
{
	return putW( ch );
}

char WIOTelnet::peek()
{
	if (inBuffer.data_avail())
	{
		char ch;
		inBuffer.peek(&ch);
		return ch;
	}
	return(0);
}

unsigned int WIOTelnet::read(char *buffer, unsigned int count)
{
	int nRet = 0;
	char ch;
	char * pszTemp = buffer;

	while (inBuffer.data_avail() == C_TRUE)
	{
		inBuffer.fetch(&ch);
		*pszTemp++ = ch;
		nRet++;
	}
	*pszTemp++ = '\0';

	return(nRet);
}

unsigned int WIOTelnet::write(char *buffer, unsigned int count)
{
	int nRet;

	while( 1 )
	{
		nRet = send( sysinfo.hSocket, buffer, count, 0 );
		if( nRet == SOCKET_ERROR )
		{
			if( WSAGetLastError() != WSAEWOULDBLOCK )
			{
				if( WSAGetLastError() != WSAENOTSOCK )
				{
					printf("DEBUG: in write(), expected to send %u character(s), actually sent %d\n", count, nRet);
				}
				return( 0 );
			}
		}
		else
		{
			return( 0 );
		}
		WWIV_Delay( 100 );
		WWIV_Delay( 0 );
	}
	if ( (unsigned)nRet != count )
	{
		printf("DEBUG: in write(), expected to send %u character(s), actually sent %d\n", count, nRet);
	}

	return( nRet );
}


bool WIOTelnet::carrier()
{
	if( sysinfo.hSocket != INVALID_SOCKET )
	{
		return( true );
	}
	return( false );
}


bool WIOTelnet::incoming()
{
	if( peek() )
	{
		return( true );
	}
	return ( false );
}


bool WIOTelnet::startup()
{
    if ((sysinfo.hSocket != 0) && (sysinfo.hSocket != INVALID_SOCKET))
    {
    	inBuffer.init(MAX_WWIV_INPUT_BUFFER_SIZE);
	    WIOTelnet::hInBufferMutex = ::CreateMutex(NULL, false, "WWIV Input Buffer");
	    _beginthread(InboundTelnetProc, 0, (void*) sysinfo.hSocket);
    }
	return true;
}


bool WIOTelnet::shutdown()
{
	return true;
}


void WIOTelnet::StopThreads()
{
	// NOP - we don't play that with telnet
}

	
void WIOTelnet::StartThreads()
{
	// NOP - we don't play that with telnet
}


void InboundTelnetProc(void *hSocketHandle)
{
	WSAEVENT hEvent = WSACreateEvent();
	WSANETWORKEVENTS events;
	char szBuffer[4096];
	bool bDone = false;
	SOCKET hSocket = (SOCKET) sysinfo.hSocket;
	int nRet = WSAEventSelect(hSocket, hEvent, FD_READ | FD_CLOSE);
	if (nRet == SOCKET_ERROR)
	{
		bDone = true;
	}
	while (!bDone)
	{
		WSAWaitForMultipleEvents(1, &hEvent, false, 10000, false);
		int nRet = WSAEnumNetworkEvents(hSocket, hEvent, &events);
		if (nRet == SOCKET_ERROR)
		{
			bDone = true;
			break;
		}
		if (events.lNetworkEvents & FD_READ)
		{
			nRet = recv(hSocket, szBuffer, sizeof(szBuffer), 0);
			if ( nRet == SOCKET_ERROR )
			{
				if( WSAGetLastError() != WSAEWOULDBLOCK )
				{
					// Error or the socket was closed.
					hSocket = INVALID_SOCKET;
					bDone = true;
					break;
				}
			}
			else if ( nRet == 0 )
			{
				hSocket = INVALID_SOCKET;
				bDone = true;
				break;
			}
			for (int i=0; i<nRet; i++)
			{
				// Add the data to the input buffer
                WaitForSingleObject(WIOTelnet::hInBufferMutex, INFINITE);
                WIOTelnet::inBuffer.add(szBuffer[i]);
                ReleaseMutex(WIOTelnet::hInBufferMutex);
			}
		} 
		else if (events.lNetworkEvents & FD_CLOSE)
		{
			bDone = true;
			hSocket = INVALID_SOCKET;
			break;
		}
	}

	if (hSocket == INVALID_SOCKET)
	{
		closesocket( sysinfo.hSocket );
		closesocket( sysinfo.hDuplicateSocket );
		sysinfo.hSocket = INVALID_SOCKET;
		sysinfo.hDuplicateSocket = INVALID_SOCKET;
	}

	WSACloseEvent(hEvent);
	_endthread();

}


