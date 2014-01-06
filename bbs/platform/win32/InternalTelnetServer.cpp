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
#include "wwiv.h"
#include "InternalTelnetServer.h"
#include "Runnable.h"
#include "Wiot.h"
#include <iostream>
#include <cstring>

WInternalTelnetServer::WInternalTelnetServer( Runnable* pRunnable ) : m_pRunnable( pRunnable ), hSocketHandle( INVALID_SOCKET )
{
    WIOTelnet::InitializeWinsock();
}


WInternalTelnetServer::~WInternalTelnetServer()
{
}


void WInternalTelnetServer::CreateListener()
{
    int nRet = SOCKET_ERROR;

    // Start Listening Thread Socket
    SOCKET hSock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( hSock == INVALID_SOCKET )
    {
        std::cout << "Error Creating Listener socket..\r\n";
    }
    else
    {
        SOCKADDR_IN pstSockName;
        pstSockName.sin_addr.s_addr = ADDR_ANY;
        pstSockName.sin_family = PF_INET;
        pstSockName.sin_port = htons( 23 );
        nRet = bind( hSock, reinterpret_cast<LPSOCKADDR>( &pstSockName ), sizeof( SOCKADDR_IN ) );
        if ( nRet == SOCKET_ERROR )
        {
			int nBindErrCode = WSAGetLastError();
            std::cout << "error " << nBindErrCode << " binding socket\r\n";
			switch ( nBindErrCode )
			{
			case WSANOTINITIALISED:
                std::cout << "WSANOTINITIALISED";
				break;
			case WSAENETDOWN:
				std::cout << "WSAENETDOWN";
				break;
			case WSAEACCES:
				std::cout << "WSAEACCES";
				break;
			case WSAEADDRINUSE:
				std::cout << "WSAEADDRINUSE";
				break;
			case WSAEADDRNOTAVAIL:
				std::cout << "WSAEADDRNOTAVAIL";
				break;
			case WSAEFAULT:
				std::cout << "WSAEFAULT";
				break;
			case WSAEINPROGRESS:
				std::cout << "WSAEINPROGRESS";
				break;
			case WSAEINVAL:
				std::cout << "WSAEINVAL";
				break;
			case WSAENOBUFS:
				std::cout << "WSAENOBUFS";
				break;
			case WSAENOTSOCK:
				std::cout << "WSAENOTSOCK";
				break;
			default:
				std::cout << "*unknown error*";
				break;
			}
        }
        else
        {
            nRet = listen( hSock, 5 );
            if ( nRet == SOCKET_ERROR )
            {
                std::cout << "Error listening on socket\r\n";
            }
        }
    }

    if ( nRet == SOCKET_ERROR )
    {
        closesocket( hSock );
        hSock = INVALID_SOCKET;
        std::cout << "Unable to initilize Listening Socket!\r\n";
        WSACleanup();
        exit( 1 );
    }
    hSocketHandle = hSock;
}


void WInternalTelnetServer::RunTelnetServer()
{
    SOCKET hSock;
    SOCKADDR_IN lpstName;
    int AddrLen = sizeof( SOCKADDR_IN );

    CreateListener();

    if( hSocketHandle != INVALID_SOCKET )
    {
        std::cout << "Press control-c to exit\r\n\n";
        std::cout << "Waiting for socket connection...\r\n\n";
        hSock = accept( hSocketHandle, reinterpret_cast<LPSOCKADDR>( &lpstName ), &AddrLen );
        if( hSock != INVALID_SOCKET )
        {
            char buffer[20];
            _snprintf( buffer, sizeof( buffer ), "-H%u", hSock );
            char **szParameters;
            szParameters = new char *[3];
            szParameters[0] = new char [1];
            szParameters[1] = new char [20];
            szParameters[2] = new char [20];

            strcpy( szParameters[0], "" );
            strcpy( szParameters[1], buffer );
            strcpy( szParameters[2], "-XT" );

            m_pRunnable->Run( 3, szParameters );
            delete [] szParameters[0];
            delete [] szParameters[1];
            delete [] szParameters[2];
            delete [] szParameters;
        }
    }
}


