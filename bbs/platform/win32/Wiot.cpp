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


//
// Constants related to Telnet IAC options and such.
//
const char WIOTelnet::CHAR_TELNET_OPTION_IAC        = '\xFF';
const int  WIOTelnet::TELNET_OPTION_IAC             = 255;
const int  WIOTelnet::TELNET_OPTION_NOP             = 241;
const int  WIOTelnet::TELNET_OPTION_BRK             = 243;

const int  WIOTelnet::TELNET_OPTION_WILL            = 251;
const int  WIOTelnet::TELNET_OPTION_WONT            = 252;
const int  WIOTelnet::TELNET_OPTION_DO              = 253;
const int  WIOTelnet::TELNET_OPTION_DONT            = 254;

const int  WIOTelnet::TELNET_SB                     = 250;
const int  WIOTelnet::TELNET_SE                     = 240;

const int  WIOTelnet::TELNET_OPTION_BINARY          = 0;
const int  WIOTelnet::TELNET_OPTION_ECHO            = 1;
const int  WIOTelnet::TELNET_OPTION_RECONNECTION    = 2;
const int  WIOTelnet::TELNET_OPTION_SUPPRESSS_GA    = 3;
const int  WIOTelnet::TELNET_OPTION_TERMINAL_TYPE   = 24;
const int  WIOTelnet::TELNET_OPTION_WINDOW_SIZE     = 31;
const int  WIOTelnet::TELNET_OPTION_TERMINAL_SPEED  = 32;
const int  WIOTelnet::TELNET_OPTION_LINEMODE        = 34;


WIOTelnet::WIOTelnet()
{
    bThreadsStarted = false;
}


bool WIOTelnet::setup(char parity, int wordlen, int stopbits, unsigned long baud)
{
	UNREFERENCED_PARAMETER( parity );
	UNREFERENCED_PARAMETER( wordlen );
	UNREFERENCED_PARAMETER( stopbits );
	UNREFERENCED_PARAMETER( baud );

    return true;
}


unsigned int WIOTelnet::open()
{
    StartThreads();
	return 0;
}


void WIOTelnet::close( bool bIsTemporary )
{
    if ( !bIsTemporary )
    {
        // this will stop the threads
	    closesocket( GetSession()->hSocket );
    }
    else
    {
        StopThreads();
    }
}


unsigned int WIOTelnet::putW(unsigned char ch)
{
	if ( GetSession()->hSocket == INVALID_SOCKET )
	{
#ifdef _DEBUG
        std::cout << "pw(INVALID-SOCKET) ";
#endif // _DEBUG
		return 0;
	}
	char szBuffer[5];
	szBuffer[0] = static_cast<char>( ch );
	szBuffer[1] = '\0';
	if( ch == TELNET_OPTION_IAC )
	{
		szBuffer[1] = static_cast<char>( ch );
		szBuffer[2] = '\0';
	}

	for( ;; )
	{
		int nRet = send( GetSession()->hSocket, szBuffer, strlen( szBuffer ), 0 );
		if( nRet == SOCKET_ERROR )
		{
			if( WSAGetLastError() != WSAEWOULDBLOCK )
			{
				if( WSAGetLastError() != WSAENOTSOCK )
				{
                    std::cout << "DEBUG: ERROR on send from putW() [" << WSAGetLastError () << "] [#" << static_cast<int>( ch ) << "]" << std::endl;
				}
				return 0;
			}
		}
		else
		{
			return nRet;
		}
		WWIV_Delay( 0 );
	}
}


unsigned char WIOTelnet::getW()
{
	char ch = 0;
	WaitForSingleObject( hInBufferMutex, INFINITE );
	if ( !inBuffer.empty() )
	{
		ch = inBuffer.front();
		inBuffer.pop();
	}
	ReleaseMutex( hInBufferMutex );
	return static_cast<unsigned char> ( ch );
}


bool WIOTelnet::dtr(bool raise)
{
	if (!raise)
	{
		closesocket(GetSession()->hSocket);
		closesocket(GetSession()->hDuplicateSocket);
		GetSession()->hSocket = INVALID_SOCKET;
		GetSession()->hDuplicateSocket = INVALID_SOCKET;
	}
	return true;
}


void WIOTelnet::flushOut()
{
	// NOP - We don't wait for output
}


void WIOTelnet::purgeOut()
{
	// NOP - We don't wait for output
}


void WIOTelnet::purgeIn()
{
	// Implementing this is new as of 2003-03-31, so if this causes problems,
	// then we may need to remove it [Rushfan]
	WaitForSingleObject( hInBufferMutex, INFINITE );
	while( !inBuffer.empty() )
	{
		inBuffer.pop();
	}
	ReleaseMutex( hInBufferMutex );
}


unsigned int WIOTelnet::put(unsigned char ch)
{
	return putW( ch );
}


char WIOTelnet::peek()
{
	char ch = 0;

	WaitForSingleObject( hInBufferMutex, INFINITE );
	if ( !inBuffer.empty() )
	{
		ch = inBuffer.front();
	}
	ReleaseMutex( hInBufferMutex );

	return ch;
}


unsigned int WIOTelnet::read(char *buffer, unsigned int count)
{
	unsigned int nRet = 0;
	char * pszTemp = buffer;

	WaitForSingleObject( hInBufferMutex, INFINITE );
	while ( ( !inBuffer.empty() ) && ( nRet <= count ) )
	{
		char ch = inBuffer.front();
		inBuffer.pop();
		*pszTemp++ = ch;
		nRet++;
	}
	*pszTemp++ = '\0';

	ReleaseMutex( hInBufferMutex );

	return nRet;
}


unsigned int WIOTelnet::write(const char *buffer, unsigned int count, bool bNoTranslation )
{
	int nRet;
    char * pszBuffer = reinterpret_cast<char*>( bbsmalloc( count * 2 + 100 ) );
    ZeroMemory( pszBuffer, ( count * 2 ) + 100 );
    int nCount = count;

    if ( !bNoTranslation && ( memchr( buffer, CHAR_TELNET_OPTION_IAC, count ) != NULL ) )
    {
        // If there is a #255 then excape the #255's
        const char * p = buffer;
        char * p2 = pszBuffer;
        for( unsigned int i=0; i < count; i++ )
        {
            if ( *p == CHAR_TELNET_OPTION_IAC && !bNoTranslation )
            {
                *p2++ = CHAR_TELNET_OPTION_IAC;
                *p2++ = CHAR_TELNET_OPTION_IAC;
                nCount++;
            }
            else
            {
                *p2++ = *p;
            }
            p++;
        }
        *p2++ = '\0';
    }
    else
    {
        memcpy( pszBuffer, buffer, count );
    }

	for( ;; )
	{
		nRet = send( GetSession()->hSocket, pszBuffer, nCount, 0 );
		if( nRet == SOCKET_ERROR )
		{
			if( WSAGetLastError() != WSAEWOULDBLOCK )
			{
				if( WSAGetLastError() != WSAENOTSOCK )
				{
                    std::cout << "DEBUG: in write(), expected to send " << count << " character(s), actually sent " << nRet << std::endl;
				}
                BbsFreeMemory( pszBuffer );
				return 0;
			}
		}
		else
		{
            BbsFreeMemory( pszBuffer );
			return nRet;
		}
		WWIV_Delay( 0 );
	}
}


bool WIOTelnet::carrier()
{
    return ( GetSession()->hSocket != INVALID_SOCKET ) ? true : false;
}


bool WIOTelnet::incoming()
{
	WaitForSingleObject( hInBufferMutex, INFINITE );
	bool bRet = ( inBuffer.size() > 0 ) ? true : false;
	ReleaseMutex( hInBufferMutex );
    return bRet;
}


bool WIOTelnet::startup()
{
    if ( GetSession()->hSocket != 0 && GetSession()->hSocket != INVALID_SOCKET )
    {
        // Make sure our signal event is not set to the "signaled" state
        hReadStopEvent = CreateEvent( NULL, true, false, NULL );
    }
	return true;
}


bool WIOTelnet::shutdown()
{
	// Stop the send/receive threads
	StopThreads();

    CloseHandle( hReadStopEvent );
	return true;
}


void WIOTelnet::StopThreads()
{
    if ( !bThreadsStarted )
    {
        return;
    }
    if ( !SetEvent( hReadStopEvent ) )
    {
        const char *pText = GetLastErrorText();
        std::cout << "Error with PulseEvent " << GetLastError() << " - '" << pText << "'" << std::endl;
    }
    WWIV_Delay( 0 );

	// Stop read thread
    DWORD dwRes = WaitForSingleObject( hReadThread, 5000 );
    switch ( dwRes )
    {
    case WAIT_OBJECT_0:
        CloseHandle( hReadThread );
        hReadThread = NULL;
        // Thread Ended
        break;
    case WAIT_TIMEOUT:
        // The exit code of 123 doesn't mean anything, and isn't used anywhere.
        ::TerminateThread( hReadThread, 123 );
        break;
    }
    bThreadsStarted = false;
}


void WIOTelnet::StartThreads()
{
    if ( bThreadsStarted )
    {
        return;
    }

    if ( !ResetEvent( hReadStopEvent ) )
    {
        const char *pText = GetLastErrorText();
        std::cout << "Error with PulseEvent " << GetLastError() << " - '" << pText << "'" << std::endl;
    }

    hInBufferMutex = ::CreateMutex(NULL, false, "WWIV Input Buffer");
    hReadThread = (HANDLE) _beginthread(WIOTelnet::InboundTelnetProc, 0, static_cast< void * >( this ) );
    bThreadsStarted = true;
}


WIOTelnet::~WIOTelnet()
{
	inBuffer.empty();
	WSACleanup();
}


//
// Static Class Members.
//


void WIOTelnet::InboundTelnetProc(LPVOID pTelnetVoid)
{
    WIOTelnet* pTelnet = static_cast<WIOTelnet*>( pTelnetVoid );
	WSAEVENT hEvent = WSACreateEvent();
	WSANETWORKEVENTS events;
	char szBuffer[4096];
	bool bDone = false;
	SOCKET hSocket = static_cast<SOCKET>( GetSession()->hSocket );
	int nRet = WSAEventSelect(hSocket, hEvent, FD_READ | FD_CLOSE);
    HANDLE hArray[2];
    hArray[0] = hEvent;
	hArray[1] = pTelnet->hReadStopEvent;

	if (nRet == SOCKET_ERROR)
	{
		bDone = true;
	}
	while (!bDone)
	{
        DWORD dwWaitRet = WSAWaitForMultipleEvents( 2, hArray, false, 10000, false );    
        if ( dwWaitRet == ( WSA_WAIT_EVENT_0 + 1 ) )
        {
            if ( !ResetEvent( pTelnet->hReadStopEvent ) )
            {
                const char *pText = GetLastErrorText();
                std::cout << "Error with PulseEvent " << GetLastError() << " - '" << pText << "'" << std::endl;
            }

            bDone = true;
            break;
        }
		int nRet = WSAEnumNetworkEvents( hSocket, hEvent, &events );
		if ( nRet == SOCKET_ERROR )
		{
			bDone = true;
			break;
		}
		if (events.lNetworkEvents & FD_READ)
		{
            memset( szBuffer, 0, 4096 );
			nRet = recv( hSocket, szBuffer, sizeof(szBuffer), 0);
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
            szBuffer[ nRet ] = '\0';

			// Add the data to the input buffer
			int nNumSleeps = 0;
			while ( ( pTelnet->inBuffer.size() > 32678 ) && ( nNumSleeps++ <= 10 ) && !bDone )
			{
				::Sleep( 100 );
			}


            pTelnet->AddStringToInputBuffer( 0, nRet, szBuffer );
		}
		else if ( events.lNetworkEvents & FD_CLOSE )
		{
			bDone = true;
			hSocket = INVALID_SOCKET;
			break;
		}
	}

	if ( hSocket == INVALID_SOCKET )
	{
		closesocket( GetSession()->hSocket );
		closesocket( GetSession()->hDuplicateSocket );
		GetSession()->hSocket = INVALID_SOCKET;
		GetSession()->hDuplicateSocket = INVALID_SOCKET;
	}

	WSACloseEvent( hEvent );
}


void WIOTelnet::HandleTelnetIAC( unsigned char nCmd, unsigned char nParam )
{
    //
    // We should probably start responding to the DO and DONT options....
    //
	::OutputDebugString( "HandleTelnetIAC: " );

    switch ( nCmd )
    {
    case TELNET_OPTION_NOP:
		{
	        ::OutputDebugString( "TELNET_OPTION_NOP\n" );
		}
		break;
    case TELNET_OPTION_BRK:
		{
			::OutputDebugString( "TELNET_OPTION_BRK\n" );
		}
		break;
    case TELNET_OPTION_WILL:
        {
            char szBuffer[ 255 ];
            _snprintf( szBuffer, sizeof( szBuffer ), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WILL", nParam );
            ::OutputDebugString( szBuffer );
        }
		break;
    case TELNET_OPTION_WONT:
        {
            char szBuffer[ 255 ];
            _snprintf( szBuffer, sizeof( szBuffer ), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WONT", nParam );
            ::OutputDebugString( szBuffer );
        }
		break;
    case TELNET_OPTION_DO:
        {
            char szBuffer[ 255 ];
            _snprintf( szBuffer, sizeof( szBuffer ), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DO", nParam );
            ::OutputDebugString( szBuffer );
            switch ( nParam )
            {
            case TELNET_OPTION_SUPPRESSS_GA:
                {
                    char szTelnetOptionBuffer[ 255 ];
                    _snprintf( szTelnetOptionBuffer, sizeof( szTelnetOptionBuffer ), "%c%c%c", TELNET_OPTION_IAC, TELNET_OPTION_WILL, TELNET_OPTION_SUPPRESSS_GA );
                    write( szTelnetOptionBuffer, 3, true );
                    ::OutputDebugString( "Sent TELNET IAC WILL SUPPRESSS GA\r\n" );
                }
                break;
            }
        }
		break;
    case TELNET_OPTION_DONT:
        {
            char szBuffer[ 255 ];
            _snprintf( szBuffer, sizeof( szBuffer ), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DONT", nParam );
            ::OutputDebugString( szBuffer );
        }
		break;
    }
}


void WIOTelnet::AddStringToInputBuffer( int nStart, int nEnd, char *pszBuffer )
{
    WWIV_ASSERT( pszBuffer );
    WaitForSingleObject( hInBufferMutex, INFINITE );

    char szBuffer[4096];
    strcpy( szBuffer, pszBuffer );
	memcpy( szBuffer, pszBuffer, nEnd );
	bool bBinaryMode = GetBinaryMode();
    for ( int i = nStart; i < nEnd; i++ )
    {
        if ( ( static_cast<unsigned char>( szBuffer[i] ) == 255 ) )
        {
            if ( ( i + 1 ) < nEnd  && static_cast<unsigned char>( szBuffer[i+1] ) == 255 )
            {
                AddCharToInputBuffer( szBuffer[i+1] );
                i++;
            }
            else if ( ( i + 2 ) < nEnd )
            {
                HandleTelnetIAC( szBuffer[i+1], szBuffer[i+2] );
                i += 2;
            }
			else
			{
				::OutputDebugString( "WHAT THE HECK?!?!?!? 255 w/o any options or anything\r\n" );
			}
        }
        else if ( bBinaryMode || szBuffer[i] != '\0' )
        {
            // I think the nulls in the input buffer were being bad... RF20020906
            // This fixed the problem of telnetting with CRT to a linux machine and then telnetting from
            // that linux box to the bbs... Hopefully this will fix the Win9x built-in telnet client as
            // well as TetraTERM
            AddCharToInputBuffer( szBuffer[i] );
        }
        else
        {
            ::OutputDebugString( "szBuffer had a null\r\n" );
        }
    }
    ReleaseMutex( hInBufferMutex );
}


void WIOTelnet::AddCharToInputBuffer( char ch )
{
    inBuffer.push( ch );
}


