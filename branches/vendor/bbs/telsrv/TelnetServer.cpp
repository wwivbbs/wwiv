/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
// TelnetServer.cpp: implementation of the TelnetServer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TelnetServer.h"
#include "NodeStatus.h"
#include "Preferences.h"
#include "MainFrame.h"
#include "mmsystem.h"			


//
// Local variables
// 
HANDLE ghNoClients;
CRITICAL_SECTION gcriticalClients;
DWORD gdwClientCount;

//
// Prototypes for local functions ( non-class level )
//
//unsigned __stdcall ListenThread( void *pVoid );
HANDLE InitClientCount();
void DeleteClientCount();
void IncrementClientCount();
void DecrementClientCount();
//unsigned __stdcall ClientThread( void *pVoid );


struct DATAPACKET
{
	HANDLE event;
	SOCKET socket;
	TCHAR  szAddr[81];
	NodeStatus *nodeStatus;
    TelnetServer *telnetServer;
};

typedef DATAPACKET* LPDATAPACKET;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TelnetServer::TelnetServer()
{
    m_pDoc = NULL;
	m_nodeStatus = new NodeStatus();
}

TelnetServer::~TelnetServer()
{
	if ( m_nodeStatus != NULL )
	{
		delete m_nodeStatus;
	}
	m_nodeStatus = NULL;
}

BOOL TelnetServer::StartServer( CWWIVTelnetServerDoc* pDoc )
{
	SOCKADDR_IN		saServer;
	unsigned		ThreadAddr;
	int				nRet;

    m_pDoc = pDoc;

	Preferences prefs;
	m_nodeStatus->SetLowNode( prefs.m_nStartNode );
	m_nodeStatus->SetHighNode( prefs.m_nEndNode );

	// Create exit event object
	m_hExit = CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( m_hExit == NULL )
	{
		return FALSE;
	}

	// Create SOCKET
	m_Socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

	if ( m_Socket == INVALID_SOCKET )
	{
        pDoc->AppendLogText( "Error Creating Socket" );
		return FALSE;
	}

	int nLocalPort = ( prefs.m_nPortNum != 0 ) ? prefs.m_nPortNum : 23;
	
	saServer.sin_port = htons( static_cast<u_short>( nLocalPort ) ); 
	saServer.sin_family = AF_INET;
	saServer.sin_addr.S_un.S_addr = INADDR_ANY;

	// Bind the socket
	nRet = bind( m_Socket, reinterpret_cast<LPSOCKADDR>( &saServer ), sizeof ( struct sockaddr ) );

	if ( nRet == SOCKET_ERROR )
	{
        CString strError;
        strError.Format( "Error [%d] Binding to Socket on port [%d]", WSAGetLastError(), nLocalPort );
        pDoc->AppendLogText( strError );
		closesocket( m_Socket );
		return FALSE;
	}

	// Listen on the socket
	nRet = listen( m_Socket, SOMAXCONN );
	if ( nRet == SOCKET_ERROR )
	{
        CString strError;
        strError.Format( "Error [%d] Binding to Socket on port [%d]", WSAGetLastError(), nLocalPort );
        pDoc->AppendLogText( strError );
		closesocket( m_Socket );
		return FALSE;
	}

	DATAPACKET* packet = static_cast<DATAPACKET *>( malloc( sizeof( DATAPACKET ) ) );
	packet->event = m_hExit;
	packet->socket = m_Socket;
	packet->nodeStatus = m_nodeStatus;
    packet->telnetServer = this;
	
	// Create the listening thread
	m_dwListeningThread = _beginthreadex( 
								NULL,
								0,
								TelnetServer::ListenThread,
								packet,
								0,
								&ThreadAddr );

	if ( !m_dwListeningThread )
	{
		pDoc->AppendLogText( "Error Creating Thread to Listen to Socket" );
		closesocket( m_Socket );
        free( packet );
		return FALSE;
	}

    // No need to say anything, the caller reports success messages for us.
	return TRUE;
}

BOOL TelnetServer::StopServer()
{
	SetEvent( m_hExit );

	int nRet = closesocket( m_Socket );

	nRet = WaitForSingleObject( reinterpret_cast<HANDLE>( m_dwListeningThread ), INFINITE );
	if ( nRet == WAIT_TIMEOUT )
	{
		// %%TODO: Log error stoping listener thread
	}

	CloseHandle( reinterpret_cast<HANDLE>( m_dwListeningThread ) );
	CloseHandle( m_hExit );

	// %%TODO: Log message that we are done.
	return TRUE;
}

NodeStatus* TelnetServer::GetNodeStatus()
{
	return m_nodeStatus;
}


//
// Non-class functions
//
unsigned __stdcall TelnetServer::ListenThread( void *pVoid )
{	
	SOCKET socketClient;
	unsigned ThreadAddr;
	DWORD dwClientThread;
	SOCKADDR_IN SockAddr;
	int nLen;
	DWORD dwRet;
	HANDLE hNoClients;
	LPDATAPACKET pPacket = static_cast<LPDATAPACKET>( pVoid );
	SOCKET listenSocket = pPacket->socket;
	LPHANDLE	pHandle = static_cast<LPHANDLE>( pPacket->event );

	hNoClients = InitClientCount();

	while ( TRUE )
	{
		nLen = sizeof ( SOCKADDR_IN );
		socketClient = accept( listenSocket, reinterpret_cast<LPSOCKADDR>( &SockAddr ), &nLen );
		if ( socketClient == INVALID_SOCKET )
		{
			break;
			
		}

		// Connection established
		// %%TODO: Log connection

		DATAPACKET* packet = static_cast<DATAPACKET *>( malloc( sizeof( DATAPACKET ) ) );
		packet->socket = socketClient;
		packet->nodeStatus = pPacket->nodeStatus;
        packet->telnetServer = pPacket->telnetServer;
		_tcsncpy( packet->szAddr, inet_ntoa( SockAddr.sin_addr ), 80 );

		dwClientThread = _beginthreadex(
							NULL,
							0,
							TelnetServer::ClientThread,
							packet,// lpReq
							0,
							&ThreadAddr);
		if ( !dwClientThread )
		{
			// %%TODO: Log error starting client thread
		}
	
		CloseHandle( reinterpret_cast<HANDLE>( dwClientThread ) );
	}

	// Wait for exit event
	WaitForSingleObject( ( HANDLE ) pHandle, INFINITE );

	// Wait for all clients to exit.
	dwRet = WaitForSingleObject( hNoClients, 5000 );
	if ( dwRet == WAIT_TIMEOUT )
	{
        pPacket->telnetServer->m_pDoc->AppendLogText( "WARNING: Could not terminate Client" );
	}

	DeleteClientCount();
	free( pVoid );

	_endthreadex( 0 );
	return 0;
}


HANDLE InitClientCount()
{
	gdwClientCount = 0;
	InitializeCriticalSection( &gcriticalClients );

	// Create the no clients left event
	ghNoClients = CreateEvent( NULL, TRUE, TRUE, NULL );
	return ghNoClients;
}


void DeleteClientCount()
{
	DeleteCriticalSection( &gcriticalClients );
	CloseHandle( ghNoClients );
}


void IncrementClientCount()
{
	// %%NOTE: Should this be replaced by the InterlockedIncrement function?
	EnterCriticalSection( &gcriticalClients );
	gdwClientCount++;
	LeaveCriticalSection( &gcriticalClients );
	ResetEvent( ghNoClients );
}


void DecrementClientCount()
{
	EnterCriticalSection( &gcriticalClients );
	if ( gdwClientCount > 0 )
	{
		gdwClientCount--;
	}
	LeaveCriticalSection( &gcriticalClients );
	
	if ( gdwClientCount < 1 )
	{
		SetEvent( ghNoClients );
	}
}


unsigned __stdcall TelnetServer::ClientThread( void *pVoid )
{
	
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	LPDATAPACKET pPacket = static_cast<LPDATAPACKET>( pVoid );
	SOCKET OrigSock = pPacket->socket;
	NodeStatus *nodeStatus = pPacket->nodeStatus;
    TelnetServer *telnetServer = pPacket->telnetServer;
	SOCKET DuplicateSock;
	Preferences prefs;
	
    // 
	// Duplicate the socket OrigSock to create an inheritable copy.
	// 
	if ( !DuplicateHandle(
			GetCurrentProcess(),
			reinterpret_cast<HANDLE>( OrigSock ),
			GetCurrentProcess(),
			reinterpret_cast<HANDLE*>( &DuplicateSock ),
			0,
			TRUE, // Inheritable
			DUPLICATE_SAME_ACCESS ) ) 
    {
        TCHAR szTmpErrorMessage[ 255 ];
        wsprintf( szTmpErrorMessage, "ERROR: Unable to duplicate socket handle.  Error code [%ld]", GetLastError() );
        telnetServer->LogMessage( szTmpErrorMessage );
		return -1;
	}
	// 
	// Spawn the child process.
	// The first command line argument (argv[1]) is the socket handle.
	// 
	int nNodeNumber = nodeStatus->GetNextAvailableNode();
	if ( nNodeNumber == 0 )
	{
		// Tell the user we are busy
		send( DuplicateSock, "BUSY\r\n", 6, 0 );
        TCHAR szBuffer[ 255 ];
        wsprintf( szBuffer, "INFO: Sent BUSY signal to: [%s]", pPacket->szAddr );
        telnetServer->LogMessage( szBuffer );

		// Close sockets.
		closesocket( OrigSock );
		closesocket( DuplicateSock );
		return 0;
	}

	TCHAR szNodeNumber[21];
	wsprintf( szNodeNumber, "%d", nNodeNumber );

	TCHAR szSocketHandle[41];
	wsprintf( szSocketHandle, "%d", DuplicateSock );

	CString args = prefs.m_parameters;
	args.Replace( "@N", szNodeNumber );
	args.Replace( "@H", szSocketHandle );

	// Make room for a 1024, we do this because the UNICODE version of CreateProcess
	// will fail if the commandline is a const char array.
	TCHAR szCmdLine[1024];
	lstrcpy( szCmdLine, prefs.m_cmdLine );
	lstrcat( szCmdLine, " " );
	lstrcat( szCmdLine, args );

	ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    if ( prefs.m_bLaunchMinimized )
    {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow |= SW_MINIMIZE;
    }
    
    TCHAR szLogMsgCmdLine[ 1152 ];
    wsprintf( szLogMsgCmdLine, "INFO: Executing commandline: %s", szCmdLine );
    telnetServer->LogMessage( szLogMsgCmdLine ); 
    DWORD dwCreationFlags = 0;
	if ( !CreateProcess( NULL,
						 szCmdLine,
						 NULL,
						 NULL,
						 TRUE, // inherit handles
						 dwCreationFlags,
						 NULL,
						 prefs.m_workDir,
						 &si,
						 &pi ) )
	{
		closesocket(OrigSock);
		closesocket(DuplicateSock); 
		TCHAR szError[255];
        wsprintf( szError, "ERROR: CreateProcess failed with error code [%d]", GetLastError() );
        telnetServer->LogMessage( szError );
		return -1;
	}

	// Count me
	IncrementClientCount();
    if ( prefs.m_bUseSounds )
    {
        ::PlaySound( prefs.m_strLogonSound, NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NOWAIT );
    }

	nodeStatus->SetNodeInfo( nNodeNumber, TRUE, pPacket->szAddr );


    CWinApp* pWinApp = AfxGetApp();
    CWnd *pMainWindow = pWinApp->GetMainWnd();
    
    ASSERT( pMainWindow );
    pMainWindow->PostMessage( WM_NODESTATUSCHANGED, nNodeNumber, 0 );


	// 
	// On Windows 95, the parent needs to wait until the child
	// is done with the duplicated handle before closing it.
	// 
	WaitForSingleObject(pi.hProcess, INFINITE);
	
	// The duplicated socket handle must be closed by the owner
	// process--the parent. Otherwise, socket handle leakage
	// occurs. On the other hand, closing the handle prematurely
	// would make the duplicated handle invalid in the child. In this
	// sample, we use WaitForSingleObject(pi.hProcess, INFINITE) to
	// wait for the child.

	closesocket(OrigSock);
	closesocket(DuplicateSock); 

    //
    // Get the exit code
    //

    DWORD dwProcessExitCode = 0;
    GetExitCodeProcess( pi.hProcess, &dwProcessExitCode );
    
    CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
		
	DecrementClientCount();
	TCHAR szAddress[81];
	_tcscpy( szAddress, "0.0.0.0" );
	nodeStatus->SetNodeInfo( nNodeNumber, FALSE, szAddress );

	free( pVoid );

    ASSERT( pMainWindow );
	pMainWindow->PostMessage( WM_NODESTATUSCHANGED, nNodeNumber, dwProcessExitCode );
    if ( prefs.m_bUseSounds )
    {
        ::PlaySound( prefs.m_strLogoffSound, NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NOWAIT );
    }

	_endthreadex( 0 );
	return 0;
}


BOOL TelnetServer::LogMessage( LPCTSTR pszMessage )
{
    CWinApp* pWinApp = AfxGetApp();
    CWnd *pMainWindow = pWinApp->GetMainWnd();
    CString* pMessage = new CString( pszMessage );
    pMainWindow->PostMessage( WM_LOG_MESSAGE, 1, (LPARAM) pMessage );
    return TRUE;
}


