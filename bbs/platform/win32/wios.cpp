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
#include "wios.h"

//TODO(rushfan): Remove this one too after fcns.h goes away
#include <iostream>
#include "platform/platformfcns.h"
#include <process.h>


#define TRACE_DEBUGSTRING

//#define TRACE_DEBUG
//#define TRACE_SYSOPLOG
//#define TRACE_DEBUGSTRING

#ifdef TRACE_DEBUG

#define WIOS_TRACE(x) std::cout << (x)
#define WIOS_TRACE1(x,y) std::cout << (x) << (y)
#define WIOS_TRACE2(x,y,z) std::cout << (x) <<(y) << (z)
#define WIOS_TRACE3(x,y,z, z2) std::cout << (x) << (y) << (z) << (z2)

#elif defined TRACE_SYSOPLOG

static char szLogStr[255];

#define WIOS_TRACE(x)			{ sysoplog( ( x ) ); }
#define WIOS_TRACE1(x,y)		{ _snprintf( szLogStr, sizeof( szLogStr ), (x),(y)); sysoplog( szLogStr ); }
#define WIOS_TRACE2(x,y,z)		{ _snprintf( szLogStr, sizeof(szLogStr), (x),(y),(z)); sysoplog(szLogStr); }
#define WIOS_TRACE3(x,y,z, z2)	{ _snprintf( szLogStr, sizeof( szLogStr ), (x),(y),(z), (z2)); sysoplog( szLogStr ); }

#elif defined TRACE_DEBUGSTRING

static char szLogStr[255];

#define WIOS_TRACE(x)			{ OutputDebugString( ( x ) ); }
#define WIOS_TRACE1(x,y)		{ _snprintf(szLogStr, sizeof( szLogStr ), (x),(y)); OutputDebugString(szLogStr); }
#define WIOS_TRACE2(x,y,z)		{ _snprintf(szLogStr, sizeof( szLogStr ), (x),(y),(z)); OutputDebugString(szLogStr); }
#define WIOS_TRACE3(x,y,z, z2)	{ _snprintf(szLogStr, sizeof( szLogStr ), (x),(y),(z), (z2)); OutputDebugString(szLogStr); }

#else

#define WIOS_TRACE(x)
#define WIOS_TRACE1(x,y)
#define WIOS_TRACE2(x,y,z)
#define WIOS_TRACE3(x,y,z, z2)

#endif // TRACE_DEBUG


//
// Static variables
//

//std::queue<char> WIOSerial::m_inputQueue;
//HANDLE WIOSerial::m_hInBufferMutex;
//HANDLE WIOSerial::m_hReadStopEvent;




const int READ_BUF_SIZE = 1024;



bool WIOSerial::setup(char parity, int wordlen, int stopbits, unsigned long baud) {
	WIOS_TRACE("WIOSerial::setup()\n");
	if (!bOpen) {
		return false;
	}
	this->SetBaudRate(baud);

	// Get the current parameters
	if ( !GetCommState( hComm, &dcb ) ) {
		WIOS_TRACE2( "rc from GetCommState is %lu - '%s' \n", GetLastError(), GetLastErrorText() );
		return false;
	}

	// Set up the DCB for the comm parameters
	dcb.ByteSize = static_cast<BYTE>( wordlen );
	switch(parity) {
	case 'N':
		dcb.Parity = NOPARITY;
		break;
	case 'E':
		dcb.Parity = EVENPARITY;
		break;
	case 'O':
		dcb.Parity = ODDPARITY;
		break;
	default:
		WIOS_TRACE1("Invalid parity value: %d\n", parity);
		return false;
	}
	switch(stopbits) {
	case 1:
		dcb.StopBits = 0;
		break;
	case 2:
		dcb.StopBits = 2;
		break;
	default:
		WIOS_TRACE1("Invalid stop bits value: %d\n", stopbits);
		return false;
	}

	if ( !SetCommState( hComm, &dcb ) ) {
		WIOS_TRACE1("rc from SetCommState is %lu\n", GetLastError());
		return false;
	}
	return true;

}


unsigned int WIOSerial::GetHandle() const {
	return reinterpret_cast<unsigned int>( hComm );
}


unsigned int WIOSerial::open() {
	WIOS_TRACE("WIOSerial::open()\n");
	if (bOpen) {
		WIOS_TRACE("Port already open");
		return true;
	}


	char szComPortFileName[FILENAME_MAX];

	_snprintf( szComPortFileName, sizeof( szComPortFileName ), "\\\\.\\COM%d", m_ComPort );

	int nOpenTries	= 0;
	bool bOpened	= false;
	while ( !bOpened && ( nOpenTries < 6 ) ) {
		WIOS_TRACE1( "Attempt to open comport %s", szComPortFileName );
		// Try up to 6 times to open the com port if it failed
		// with ERROR_ACCESS_DENIED
		//
		// Under COM/IP and other fossil drivers, they take a few seconds
		// (up to 6 it seems to actually release the COM handle.  So we
		// will try 6 times sleeping  for 2 seconds each time to get the
		// com handle. (12 seconds total)  This  seems to work for BRE/FE/GWAR
		//
		// %%JZ Added = NULL to report if port isn't available.
		hComm = NULL;
		hComm = CreateFile(
		            szComPortFileName,
		            GENERIC_READ | GENERIC_WRITE,
		            0,
		            NULL,
		            OPEN_EXISTING,
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		            NULL);

		if ( ( hComm == INVALID_HANDLE_VALUE ) && ( ERROR_ACCESS_DENIED == GetLastError() ) ) {
			WIOS_TRACE1( "TRY #%d - from CreateFile() [ERROR_ACCESS_DENIED]\n", nOpenTries );
			nOpenTries++;
			Sleep( 2000 );
		} else {
			bOpened = true;
		}
	}
	if ( hComm == INVALID_HANDLE_VALUE ) {
		WIOS_TRACE2("rc from CreateFile() is %lu - '%s'\n", GetLastError(), GetLastErrorText());
		return false;
	}
	WIOS_TRACE1("Opened Comm Handle %d\n", reinterpret_cast<unsigned int>(hComm));


	// Set comm timeouts
	COMMTIMEOUTS timeouts;

	//timeouts.ReadIntervalTimeout = 0; // MAXDWORD;
	//timeouts.ReadTotalTimeoutMultiplier = 0; // MAXDWORD;
	//timeouts.ReadTotalTimeoutConstant = 0; // 5000;

	// Not sure what these should be... Playing around
	// rushfan 2003-03-31
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;// MAXDWORD;
	timeouts.ReadTotalTimeoutConstant = 1000;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;

	GetCommTimeouts(hComm, &oldtimeouts);

	if (!SetCommTimeouts(hComm, &timeouts)) {
		// Error setting time-outs.
		WIOS_TRACE("Error setting comm timeout\n");
	}


	this->dtr( true );

	// Make sure our signal event is not set to the "signaled" state
	m_hReadStopEvent = CreateEvent(NULL, true, false, NULL);

	StartThreads();
	bOpen = true;
	return true;

}

DWORD WIOSerial::GetDCBCodeForSpeed( unsigned long speed ) {
	switch( speed ) {
	case 110:
		return CBR_110;
	case 300:
		return CBR_300;
	case 600:
		return CBR_600;
	case 1200:
		return CBR_1200;
	case 2400:
		return CBR_2400;
	case 4800:
		return CBR_4800;
	case 9600:
		return CBR_9600;
	case 14400:
		return CBR_14400;
	case 19200:
		return CBR_19200;
	case 38400:
		return CBR_38400;
	case 56000:
		return CBR_56000;
	case 57600:
		return CBR_57600;
	case 115200:
		return CBR_115200;
	case 128000:
		return CBR_128000;
	case 256000:
		return CBR_256000;
	default:
		return 0;
	}
}

bool WIOSerial::SetBaudRate(unsigned long speed) {
	if (!bOpen) {
		return false;
	}

	// Get the current parameters
	if ( !GetCommState( hComm, &dcb ) ) {
		WIOS_TRACE2("rc from GetCommState is %lu - '%s'\n", GetLastError(), GetLastErrorText());
		return false;
	}

	// Now set the DCB up correctly...
	dcb.BaudRate = GetDCBCodeForSpeed( speed );
	if ( dcb.BaudRate == 0 ) {
		WIOS_TRACE1("Invalid baud rate: %lu\n", speed);
		return false;
	}

	if ( !SetCommState( hComm, &dcb ) ) {
		WIOS_TRACE1("rc from SetCommState is %lu\n", GetLastError());
		return false;
	}
	return true;
}

void WIOSerial::StopThreads() {
	// Try moving it before the wait...
	FlushFileBuffers(hComm);

	WIOS_TRACE1("Attempting to end thread ID (%ld)          \n", reinterpret_cast<unsigned int>(m_hReadThread));

	WWIV_Delay( 0 );
	if ( !PulseEvent( m_hReadStopEvent ) ) {
		WIOS_TRACE2("Error with PulseEvent %d - '%s'", GetLastError(), GetLastErrorText());
	}

	WWIV_Delay( 0 );

	// Stop read thread
	DWORD dwRes = WaitForSingleObject(m_hReadThread, 5000);
	switch ( dwRes ) {
	case WAIT_OBJECT_0:
		WIOS_TRACE("Reader Thread Ended\n");
		CloseHandle( m_hReadThread );
		m_hReadThread = NULL;
		// Thread Ended
		break;
	case WAIT_TIMEOUT:
		WIOS_TRACE("Unable to end thread - WAIT_TIMEOUT\n");
		::TerminateThread( m_hReadThread, 123 );
		break;
	}
}


void WIOSerial::StartThreads() {
	DWORD dwThreadID;

	// WIOS_TRACE("Creating Listener thread\n");
	m_hReadThread = (HANDLE) _beginthreadex(NULL, 0, &WIOSerial::InboundSerialProc, static_cast< void * >( this ), 0 , (unsigned int *)&dwThreadID);
	WIOS_TRACE2("Created Listener thread.  Handle = %d, ID = %d\n", reinterpret_cast<unsigned int>(m_hReadThread), dwThreadID);
	WWIV_Delay( 100 );
	WWIV_Delay( 0 );
	WWIV_Delay( 0 );
	WWIV_Delay( 0 );
}


void WIOSerial::close( bool bIsTemporary ) {
	UNREFERENCED_PARAMETER( bIsTemporary );
	if (!bOpen) {
		WIOS_TRACE("WIOSerial::close called when port not open\n");
		return;
	}

	// Stop the send/receive threads
	StopThreads();

	CloseHandle( m_hReadStopEvent );
	bOpen = false;

	SetCommTimeouts(hComm, &oldtimeouts);

	if (CloseHandle(hComm) != 0) {
		WIOS_TRACE2("ERROR: CloseHandle(hComm) [%lu - '%s']\n", GetLastError(), GetLastErrorText());
	}
	WIOS_TRACE("WIOSerial::close - Com port closed.\n");
}


unsigned int WIOSerial::putW(unsigned char ch) {
	if (!bOpen) {
		return 0;
	}

	char buf[2]; //This was 1, which is 1 byte too small <sigh>
	buf[0] = static_cast<char>( ch );
	buf[1] = '\0';
	return write( buf, 1 );
}


unsigned char WIOSerial::getW() {
	if (!bOpen) {
		return 0;
	}
	char ch = 0;
	WaitForSingleObject( m_hInBufferMutex, INFINITE );
	if ( !m_inputQueue.empty() ) {
		ch = m_inputQueue.front();
		m_inputQueue.pop();
	}
	ReleaseMutex( m_hInBufferMutex );
	return static_cast<unsigned char>( ch );
}


bool WIOSerial::dtr(bool raise) {
	// Get the current parameters
	if ( !GetCommState( hComm, &dcb ) ) {
		WIOS_TRACE1("rc from GetCommState is %lu\n", GetLastError());
		return false;
	}

	dcb.fDtrControl = ( raise ) ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;

	// Set the change in place
	if ( !SetCommState( hComm, &dcb ) ) {
		WIOS_TRACE1("rc from SetCommState is %lu\n", GetLastError());
		return false;
	}

	return true;

}


void WIOSerial::flushOut() {
}


void WIOSerial::purgeOut() {
}


void WIOSerial::purgeIn() {
}


unsigned int WIOSerial::put(unsigned char ch) {
	return putW( ch );
}


char WIOSerial::peek() {
	if (bOpen) {
		if ( !m_inputQueue.empty() ) {
			return m_inputQueue.front();
		}
	}
	return 0;
}


unsigned int WIOSerial::read( char *buffer, unsigned int count ) {
	UNREFERENCED_PARAMETER( count );
	int nRet = 0;
	char * pszTemp = buffer;

	while ( !m_inputQueue.empty() ) {
		char ch = m_inputQueue.front();
		m_inputQueue.pop();
		*pszTemp++ = ch;
		nRet++;
	}
	*pszTemp++ = '\0';

	return nRet;
}


unsigned int WIOSerial::write(const char *buffer, unsigned int count, bool bNoTranslation ) {
	UNREFERENCED_PARAMETER( bNoTranslation );
	return writeImpl( buffer, count );
}



/** Actually performs the write operation */

unsigned int WIOSerial::writeImpl(const char *buffer, unsigned int count) {
	if (!bOpen) {
		WIOS_TRACE("WIOSerial::write() - not open\n");
		return 0;
	}
	DWORD actualWritten;
	OVERLAPPED osWrite = {0};
	bool fRes;


	osWrite.hEvent = CreateEvent(NULL, true, false, NULL);
	if (osWrite.hEvent == NULL) {
		// Error creating overlapped event handle.
		WIOS_TRACE("Unable to create Overlapped Event.  Exiting WIOSerial::write()\n");
		return 0;
	}
	if ( WriteFile( hComm, buffer, (DWORD)count, &actualWritten, &osWrite ) == FALSE ) {
		if (GetLastError() != ERROR_IO_PENDING) {
			// WriteFile failed, but it isn't delayed. Report error.
			fRes = false;
		} else {
			// Write is pending.
			DWORD dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
			switch(dwRes) {
			// Overlapped event has been signaled.
			case WAIT_OBJECT_0:
				DWORD dwWritten;
				if (!GetOverlappedResult(hComm, &osWrite, &dwWritten, false)) {
					fRes = false;
				} else {
					if (dwWritten != count) {
						fRes = false;
					} else {
						// Write operation completed successfully.
						fRes = true;
						actualWritten = dwWritten;
					}
				}
				break;

			default:
				// An error has occurred in WaitForSingleObject.
				fRes = false;
				break;
			}
		}
		if (!fRes) {
			WIOS_TRACE2("rc from WriteFile is %lu - '%s'\n", GetLastError(), (LPCTSTR)GetLastErrorText());
			actualWritten = 0;
		}
	} else if (actualWritten != count) {
		WIOS_TRACE2("only wrote %d bytes instead of %d\n", actualWritten, count);
	}

	CloseHandle(osWrite.hEvent);

	return static_cast<size_t>( actualWritten );
}


bool WIOSerial::carrier() {
	DWORD dwModemStat;
	BOOL bRes = GetCommModemStatus(hComm, &dwModemStat);
	if (!bRes) {
		// An error occurred, we cannot tell
		WIOS_TRACE("Error getting carrier");
		WIOS_TRACE2(" %d - %s", GetLastError(), GetLastErrorText());
		return false;
	}
	if (dwModemStat & MS_RLSD_ON) {
		return true;
	}
	return false;
}


bool WIOSerial::incoming() {
	return ( m_inputQueue.size() > 0 ) ? true : false;
}


bool WIOSerial::HandleASuccessfulRead( LPCTSTR pszBuffer, DWORD dwNumRead, WIOSerial* pSerial ) {
	char * p = const_cast<char *>( pszBuffer );

	WaitForSingleObject( pSerial->m_hInBufferMutex, INFINITE );
	for ( DWORD i=0; i<dwNumRead; i++ ) {
		pSerial->m_inputQueue.push( *p++ );
#if defined( _DEBUG )
		if ( dwNumRead && p && *p) {
			char szBuffer[ 1024 ];
			strcpy( szBuffer, "HandleASuccessfulRead" );
			strncat( szBuffer, pszBuffer, std::min<int>( dwNumRead, 255 ) );
			strcat( szBuffer, "\n" );
			OutputDebugString( szBuffer );
		}
#endif // _DEBUG
	}
	ReleaseMutex( pSerial->m_hInBufferMutex );
	return true;
}


unsigned int __stdcall WIOSerial::InboundSerialProc(LPVOID pSerialVoid) {
	WIOSerial*	pSerial			= static_cast<WIOSerial*>( pSerialVoid );
	DWORD       dwRead          = 0;
	bool        fWaitingOnRead  = false;
	bool        bDone           = false;
	OVERLAPPED  osReader        = {0};
	HANDLE      hComm			= static_cast<HANDLE>( pSerial->hComm );
	int			loopNum			= 0;
	HANDLE      hArray[2];
	char        szBuf[READ_BUF_SIZE+1];

	WIOS_TRACE1( "InboundSerialProc (ID=%d) - started\n", GetCurrentThreadId() );
	// Create the overlapped event. Must be closed before exiting
	// to avoid a handle leak.
	osReader.hEvent = CreateEvent( NULL, true, false, NULL );

	if (osReader.hEvent == NULL) {
		// Error creating overlapped event; abort.
		_endthreadex( 0 );
		return false;
	}
	hArray[0] = osReader.hEvent;
	hArray[1] = pSerial->m_hReadStopEvent;

	while (!bDone) {
		if (!fWaitingOnRead) {
			// Issue read operation.
			if (  (loopNum++ % 1000 ) == 0 ) {
				WIOS_TRACE1("InboundSerialProc - before ReadFile (%d)\n", loopNum);
			}
			if (!ReadFile(hComm, &szBuf, READ_BUF_SIZE, &dwRead, &osReader)) {
				if ( GetLastError() != ERROR_IO_PENDING ) {
					// read not delayed?
					// Error in communications; report it.
					std::cout << "Error in call to ReadFile: " << GetLastError() << " - [" << GetLastErrorText() << "]";
					if ( 6 == GetLastError() ) {
						return false;
					}
				} else {
					fWaitingOnRead = true;
				}
			} else {
				// read completed immediately
				HandleASuccessfulRead( szBuf, dwRead, pSerial );
			}
		}


		DWORD dwRes;

		if (fWaitingOnRead) {
			dwRes = WaitForMultipleObjects( 2, hArray, false, INFINITE );
			switch( dwRes ) {
			// Event occurred.
			case WAIT_OBJECT_0: {
				if ( !GetOverlappedResult( hComm, &osReader, &dwRead, false ) ) {
					// Error in communications; report it.
					WIOS_TRACE("InboundSerialProc - error in get overlapped result\n");
				} else {
					// Read completed successfully.
					HandleASuccessfulRead( szBuf, dwRead, pSerial );
				}
				//  Reset flag so that another opertion can be issued.
				fWaitingOnRead = false;
			}
			break;
			case WAIT_OBJECT_0+1: {
				// Signaled Exit
				WIOS_TRACE( "InboundSerialProc - got stop signal\n" );
				bDone = true;
			}
			break;
			default:
				break;
			}
		}

	}
	CloseHandle( osReader.hEvent );

	// RC_END_TREAD_ATTEMPT_FIX - try to close all outstanding IO so the thread can terminate
	// quickly and cleanly.
	if ( !PurgeComm( hComm, PURGE_TXABORT | PURGE_RXABORT ) ) {
		WIOS_TRACE1( "Unable to PurgeComm %s\n", GetLastErrorText() );
	}
	WIOS_TRACE( "InboundSerialProc - end of thread\n" );

	// call _endtheadex to release any resources used by the C Runtime Lib
	_endthreadex( 0 );
	return true;
}


WIOSerial::WIOSerial( unsigned int nHandle ) : WComm() {
	WIOS_TRACE("WIOSerial::WIOSerial()\n");
	bOpen = false;
	hComm = reinterpret_cast<HANDLE>( nHandle );

	FillMemory(&dcb, sizeof(dcb), 0);
	if (!GetCommState(hComm, &dcb)) {
		// get current DCB
		WIOS_TRACE("DEBUG: WIOSerial::WIOSerial() - Unable to create DCB\n");
	}
	WIOS_TRACE( "WIOSerial::startup()\n" );
	m_hInBufferMutex = ::CreateMutex( NULL, false, "WWIV Input Buffer" );

}


WIOSerial::~WIOSerial() {
	WIOS_TRACE("WIOSerial::~WIOSerial()\n");

	if ( NULL != m_hInBufferMutex ) {
		CloseHandle( m_hInBufferMutex );
		m_hInBufferMutex = NULL;
	}
}
