/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2005 by WWIV Software Services             */
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
#include "wios.h"


//#define TRACE_DEBUG
//#define TRACE_SYSOPLOG

#ifdef TRACE_DEBUG

#define WIOS_TRACE(x) printf((x))
#define WIOS_TRACE1(x,y) printf((x),(y))
#define WIOS_TRACE2(x,y,z) printf((x),(y),(z))
#define WIOS_TRACE3(x,y,z, z2) printf((x),(y),(z), (z2))

#elif defined TRACE_SYSOPLOG

static char szLogStr[255];

#define WIOS_TRACE(x)			{ sysoplog((x)); }
#define WIOS_TRACE1(x,y)		{ sprintf(szLogStr, (x),(y)); sysoplog(szLogStr); }
#define WIOS_TRACE2(x,y,z)		{ sprintf(szLogStr, (x),(y),(z)); sysoplog(szLogStr); }
#define WIOS_TRACE3(x,y,z, z2)	{ sprintf(szLogStr, (x),(y),(z), (z2)); sysoplog(szLogStr); }

#else

#define WIOS_TRACE(x) 
#define WIOS_TRACE1(x,y) 
#define WIOS_TRACE2(x,y,z) 
#define WIOS_TRACE3(x,y,z, z2) 
#endif // _DEBUG


//
// Static variables
//

cbuf_t WIOSerial::inBuffer;
cbuf_t WIOSerial::outBuffer;
HANDLE WIOSerial::hInBufferMutex;
HANDLE WIOSerial::hOutBufferMutex;
HANDLE WIOSerial::hReadStopEvent;
HANDLE WIOSerial::hWriteStopEvent;


COMMTIMEOUTS oldtimeouts;


const int READ_BUF_SIZE = 1024;

unsigned int __stdcall InboundSerialProc(LPVOID hCommHandle);


const char* GetLastErrorText()
{
    static char szErrorText[8192];

    LPVOID lpMsgBuf;
    FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR) &lpMsgBuf,
        0,
        NULL 
        );
    strcpy(szErrorText, (LPCTSTR)lpMsgBuf);
    LocalFree( lpMsgBuf );
    return ((const char *) &szErrorText);
}


bool WIOSerial::setup(char parity, int wordlen, int stopbits, unsigned long baud)
{
    WIOS_TRACE("WIOSerial::setup()\n");
    if (!bOpen)
    {
        return false;
    }
    this->SetBaudRate(baud);

	// Get the current parameters
    	bool rc = GetCommState(hComm, &dcb);
    if (rc != true)
    {
        WIOS_TRACE2("rc from GetCommState is %lu - '%s' \n", GetLastError(), GetLastErrorText());
		return(false);
    }
	
	// Set up the DCB for the comm parameters
	dcb.ByteSize = (BYTE)wordlen;
	switch(parity)
	{
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
		    return(false);
			break;
	}
	switch(stopbits)
	{
		case 1:
			dcb.StopBits = 0;
			break;
		case 2:
			dcb.StopBits = 2;
			break;
		default:
			WIOS_TRACE1("Invalid stop bits value: %d\n", stopbits);
		    return(false);
			break;
	}

	rc = SetCommState(hComm, &dcb);
    if (rc != true)
    {
        WIOS_TRACE1("rc from SetCommState is %lu\n", GetLastError());
        return(false);
    }
    return(true);

}

unsigned int WIOSerial::open()
{
    WIOS_TRACE("WIOSerial::open()\n");
    if (bOpen)
    {
		WIOS_TRACE("Port already open");
        return true;
    }

    char    fileName[FILENAME_MAX];
    
    sprintf(fileName, "\\\\.\\COM%d", m_ComPort);

	int nOpenTries	= 0;
	bool bOpened	= false;
	while ((!bOpened) && (nOpenTries < 6))
	{
		WIOS_TRACE1("Attempt to open comport %s", fileName);
		// Try up to 6 times to open the com port if it failed 
		// with ERROR_ACCESS_DENIED
		//
		// Under COM/IP and other fossil drivers, they take a few seconds 
		// (up to 6 it seems to actually release the COM handle.  So we 
		// will try 6 times sleeping  for 2 seconds each time to get the 
		// com handle. (12 seconds total)  This  seems to work for BRE/FE/GWAR
		//
		hComm = CreateFile(
					fileName,
					GENERIC_READ | GENERIC_WRITE,
					0,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 
					NULL);

		if ((hComm == INVALID_HANDLE_VALUE) && (ERROR_ACCESS_DENIED == GetLastError()))
		{
			WIOS_TRACE1("TRY #%d - from CreateFile() [ERROR_ACCESS_DENIED]\n", nOpenTries);
			nOpenTries++;
			Sleep(2000);
		}
		else
		{
			bOpened = true;
		}
	}
	if (hComm == INVALID_HANDLE_VALUE)
	{
		WIOS_TRACE2("rc from CreateFile() is %lu - '%s'\n", GetLastError(), GetLastErrorText());
		return (false);
	}
    WIOS_TRACE1("Opened Comm Handle %d\n", hComm);


	// Set comm timeouts
	COMMTIMEOUTS timeouts;

    timeouts.ReadIntervalTimeout = 0; // MAXDWORD; 
    timeouts.ReadTotalTimeoutMultiplier = 0; // MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0; // 5000;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    GetCommTimeouts(hComm, &oldtimeouts);

    if (!SetCommTimeouts(hComm, &timeouts))
    {
        // Error setting time-outs.
        WIOS_TRACE("Error setting comm timeout\n");
    }


    this->dtr(true);
    sysinfo.hCommHandle = hComm;

    // Make sure our signal event is not set to the "signaled" state
    WIOSerial::hReadStopEvent = CreateEvent(NULL, true, false, NULL);

	StartThreads();    
    bOpen = true;
    return(true);

}


bool WIOSerial::SetBaudRate(unsigned long speed)
{
    if (!bOpen)
    {
        return false;
    }
    // Get the current parameters
	bool rc = GetCommState(hComm, &dcb);
    if (rc != true)
    {
        WIOS_TRACE2("rc from GetCommState is %lu - '%s'\n", GetLastError(), GetLastErrorText());
		return(false);
    }

	// Now set the DCB up correctly...
	switch(speed)
	{
		case 110:
			dcb.BaudRate = CBR_110;
			break;
		case 300:
			dcb.BaudRate = CBR_300;
			break;
		case 600:
			dcb.BaudRate = CBR_600;
			break;
		case 1200:
			dcb.BaudRate = CBR_1200;
			break;
		case 2400:
			dcb.BaudRate = CBR_2400;
			break;
		case 4800:
			dcb.BaudRate = CBR_4800;
			break;
		case 9600:
			dcb.BaudRate = CBR_9600;
			break;
		case 14400:
			dcb.BaudRate = CBR_14400;
			break;
		case 19200:
			dcb.BaudRate = CBR_19200;
			break;
		case 38400:
			dcb.BaudRate = CBR_38400;
			break;
		case 56000:
			dcb.BaudRate = CBR_56000;
			break;
		case 57600:
			dcb.BaudRate = CBR_57600;
			break;
		case 115200:
			dcb.BaudRate = CBR_115200;
			break;
		case 128000:
			dcb.BaudRate = CBR_128000;
			break;
		case 256000:
			dcb.BaudRate = CBR_256000;
			break;
		default:
			WIOS_TRACE1("Invalid baud rate: %lu\n", speed);
			return(false);
	}

	rc = SetCommState(hComm, &dcb);
    if (rc != true)
    {
        WIOS_TRACE1("rc from SetCommState is %lu\n", GetLastError());
        return(false);
    }
    return(true);
}

void WIOSerial::StopThreads()
{
    // Try moving it before the wait...
	FlushFileBuffers(hComm);

    WIOS_TRACE1("Attempting to end thread ID (%ld)          \n", hReadThread);
    
    WWIV_Delay(0);
    if (!PulseEvent(WIOSerial::hReadStopEvent))
    {
        WIOS_TRACE2("Error with PulseEvent %d - '%s'", GetLastError(), GetLastErrorText());
    }
    
    WWIV_Delay(0);

	// Stop read thread
    DWORD dwRes = WaitForSingleObject(hReadThread, 5000);
    switch (dwRes)
    {
    case WAIT_OBJECT_0:
        WIOS_TRACE("Reader Thread Ended\n");
        CloseHandle(hReadThread);
        hReadThread = NULL;
        // Thread Ended
        break;
    case WAIT_TIMEOUT:
        WIOS_TRACE("Unable to end thread - WAIT_TIMEOUT\n");
        ::TerminateThread(hReadThread, 123);
        break;
    }
}


void WIOSerial::StartThreads()
{
    DWORD dwThreadID;
   
    // WIOS_TRACE("Creating Listener thread\n");
    hReadThread = (HANDLE) _beginthreadex(NULL, 0, &InboundSerialProc, sysinfo.hCommHandle, 0 , (unsigned int *)&dwThreadID);
    WIOS_TRACE2("Created Listener thread.  Handle = %d, ID = %d\n", hReadThread, dwThreadID);
    WWIV_Delay(100);
	WWIV_Delay(0);
}


void WIOSerial::close()
{
    if (!bOpen)
    {
		WIOS_TRACE("WIOSerial::close called when port not open\n");
        return;
    }

	// Stop the send/receive threads
	StopThreads();

    CloseHandle(WIOSerial::hReadStopEvent);
    bOpen = false;

    SetCommTimeouts(hComm, &oldtimeouts);

    if (CloseHandle(hComm) != 0)
    {
        WIOS_TRACE2("ERROR: CloseHandle(hComm) [%lu - '%s']\n", GetLastError(), GetLastErrorText());
    }
    WIOS_TRACE("WIOSerial::close - Com port closed.\n");
}


unsigned int WIOSerial::putW(unsigned char ch)
{
    if (!bOpen)
    {
        return(0);
    }
    
    char buf[1];
    buf[0] = (char)ch;
    buf[1] = '\0';
    return write(buf, 1);
}


unsigned char WIOSerial::getW()
{
    if (!bOpen)
    {
        return(0);
    }
	char ch = 0;
	WaitForSingleObject(WIOSerial::hInBufferMutex, INFINITE);
	if (inBuffer.data_avail() == C_TRUE)
	{
		inBuffer.fetch(&ch);
	}
	ReleaseMutex(WIOSerial::hInBufferMutex);
	return static_cast<unsigned char> (ch);
}


unsigned char WIOSerial::dtr(bool raise)
{
	
	// Get the current parameters
    bool rc = GetCommState(hComm, &dcb);
    if (rc != true)
    {
        WIOS_TRACE1("rc from GetCommState is %lu\n", GetLastError());
		return(false);
    }
	
	if (raise)
	{
    	// Set up the DCB to Raise DTR
	    dcb.fDtrControl = DTR_CONTROL_ENABLE;
	}
    else
    {
	    // Set up the DCB to Lower DTR
	    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    }

	// Set the change in place
	rc = SetCommState(hComm, &dcb);
    if (rc != true)
    {
        WIOS_TRACE1("rc from SetCommState is %lu\n", GetLastError());
    }

    return(true);

}


void WIOSerial::flushOut()
{
}


void WIOSerial::purgeOut()
{
}


void WIOSerial::purgeIn()
{
}


unsigned int WIOSerial::put(unsigned char ch)
{
	return putW( ch );
}


char WIOSerial::peek()
{
    if (!bOpen)
    {
        return(0);
    }
	if (inBuffer.data_avail())
	{
		char ch;
		inBuffer.peek(&ch);
		return ch;
	}
	return(0);
}

unsigned int WIOSerial::read(char *buffer, unsigned int count)
{
    //WIOS_TRACE("WIOSerial::read()\n");
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


unsigned int WIOSerial::write(char *buffer, unsigned int count)
{
	return(writeImpl(buffer, count));
}



/** Actually performs the write operation */

unsigned int WIOSerial::writeImpl(char *buffer, unsigned int count)
{
    if (!bOpen)
    {
        WIOS_TRACE("WIOSerial::write() - not open\n");
        return(0);
    }
    DWORD actualWritten;
    OVERLAPPED osWrite = {0};
    bool fRes;   
    

    osWrite.hEvent = CreateEvent(NULL, true, false, NULL);
    if (osWrite.hEvent == NULL)
    {
      // Error creating overlapped event handle.
        WIOS_TRACE("Unable to create Overlapped Event.  Exiting WIOSerial::write()\n");
        return 0;
    }
    int rc = WriteFile(hComm, buffer, (DWORD)count, &actualWritten, &osWrite);
    if (rc != true)
    {
        if (GetLastError() != ERROR_IO_PENDING) 
        { 
            // WriteFile failed, but it isn't delayed. Report error.
            fRes = false;
        }
        else
        {
            // Write is pending.
            DWORD dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
            switch(dwRes)
            {
            // Overlapped event has been signaled. 
            case WAIT_OBJECT_0:
                DWORD dwWritten;
                if (!GetOverlappedResult(hComm, &osWrite, &dwWritten, false))
                {
                    fRes = false;
                }
                else 
                {
                    if (dwWritten != count) 
                    {
                        fRes = false;
                    }
                    else
                    {
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
        if (!fRes)
        {
            WIOS_TRACE2("rc from WriteFile is %lu - '%s'\n", GetLastError(), (LPCTSTR)GetLastErrorText());
            actualWritten = 0;
        }
    }
    else if (actualWritten != count)
    {
        WIOS_TRACE2("only wrote %d bytes instead of %d\n", actualWritten, count);
    }

    CloseHandle(osWrite.hEvent);

    return ((size_t)actualWritten);
}


bool WIOSerial::carrier()
{
    DWORD dwModemStat;
    bool bRes = GetCommModemStatus(hComm, &dwModemStat);
    if (!bRes)
    {
        // An error occurred, we cannot tell
        WIOS_TRACE("Error getting carrier");
        WIOS_TRACE2(" %d - %s", GetLastError(), GetLastErrorText());
        return(false);
    }
    if (dwModemStat & MS_RLSD_ON)
    {
        return(true);
    }
	return(false);
}


bool WIOSerial::incoming()
{
	if( peek() )
	{
		return( true );
	}
	return ( false );
}


bool WIOSerial::startup()
{
    WIOS_TRACE("WIOSerial::startup()\n");
   	inBuffer.init(MAX_WWIV_INPUT_BUFFER_SIZE);
    WIOSerial::hInBufferMutex = ::CreateMutex(NULL, false, "WWIV Input Buffer");

	return true;
}

bool WIOSerial::shutdown()
{
    WIOS_TRACE("WIOSerial::shutdown()\n");
    if (NULL != WIOSerial::hInBufferMutex)
    {
        CloseHandle(WIOSerial::hInBufferMutex);
        WIOSerial::hInBufferMutex = NULL;
    }
    return true;
}



#define READ_TIMEOUT      500      // milliseconds


bool WIOSerial::HandleASuccessfulRead(LPCTSTR pszBuffer, DWORD dwNumRead)
{
    char * p = const_cast<char *>(pszBuffer);

    WaitForSingleObject(WIOSerial::hInBufferMutex, INFINITE);
    for (DWORD i=0; i<dwNumRead; i++)
    {
        WIOSerial::inBuffer.add(*p++);
    }
    ReleaseMutex(WIOSerial::hInBufferMutex);
    return(true);
}


unsigned int __stdcall WIOSerial::InboundSerialProc(LPVOID hCommHandle)
{
    DWORD       dwRead          = 0;
    bool        fWaitingOnRead  = false;
    bool        bDone           = false;
    OVERLAPPED  osReader        = {0};
    HANDLE      hArray[2];
    HANDLE      hComm = (HANDLE) hCommHandle;
    char        szBuf[READ_BUF_SIZE+1];
	int			loopNum			= 0;

    WIOS_TRACE1("InboundSerialProc (ID=%d) - started\n", GetCurrentThreadId());
    // Create the overlapped event. Must be closed before exiting
    // to avoid a handle leak.
    osReader.hEvent = CreateEvent(NULL, true, false, NULL);

    if (osReader.hEvent == NULL)
    {
        // Error creating overlapped event; abort.
        _endthreadex(0);
        return false;
    }
    hArray[0] = osReader.hEvent;
    hArray[1] = WIOSerial::hReadStopEvent;

    while (!bDone)
    {
        if (!fWaitingOnRead) 
        {
            // Issue read operation.
            //WIOS_TRACE("InboundSerialProc - before ReadFile\n");
			if ((loopNum++ % 1000)==0)
			{
				WIOS_TRACE1("InboundSerialProc - before ReadFile (%d)\n", loopNum);
			}
            if (!ReadFile(hComm, &szBuf, 1, &dwRead, &osReader)) 
            {
                if (GetLastError() != ERROR_IO_PENDING)     
                {
                    // read not delayed?
                    // Error in communications; report it.
					printf("Error in call to ReadFile: %d - [%s]", GetLastError(), GetLastErrorText());
					if (6 == GetLastError())
					{
						return(false);
					}
                }
                else
                {
                    fWaitingOnRead = true;
                }
            }
            else 
            {    
                // read completed immediately
                HandleASuccessfulRead(szBuf, dwRead);
            }
        }


        DWORD dwRes;
    
        if (fWaitingOnRead) 
        {
            // WIOS_TRACE("InboundSerialProc - before Wait\n");
            dwRes = WaitForMultipleObjects(2, hArray, false, INFINITE);
            // WIOS_TRACE("InboundSerialProc - after Wait\n");
            switch(dwRes)
            {
            // Event occurred.
            case WAIT_OBJECT_0:         
                if (!GetOverlappedResult(hComm, &osReader, &dwRead, false))
                {
                    // Error in communications; report it.
                    WIOS_TRACE("InboundSerialProc - error in get overlapped result\n");
                }
                else
                {
                    // Read completed successfully.
                    HandleASuccessfulRead(szBuf, dwRead);
                }
                //  Reset flag so that another opertion can be issued.
                fWaitingOnRead = false;
                break;
            case WAIT_OBJECT_0+1:
                // Signaled Exit
                WIOS_TRACE("InboundSerialProc - got stop signal\n");
                bDone = true;
                break;
            default:
                break;
            }
        }

    }
    CloseHandle(osReader.hEvent);

	// RC_END_TREAD_ATTEMPT_FIX - try to close all outstanding IO so the thread can terminate
	// quickly and cleanly.
	if (!PurgeComm(hComm, PURGE_TXABORT | PURGE_RXABORT))
	{
		WIOS_TRACE1("Unable to PurgeComm %s\n", GetLastErrorText());
	}
	WIOS_TRACE("InboundSerialProc - end of thread\n");

	// call _endtheadex to release any resources used by the C Runtime Lib
    _endthreadex(0);
    return (true);
}


WIOSerial::WIOSerial() : WComm()
{
    WIOS_TRACE("WIOSerial::WIOSerial()\n");
    bOpen = false;

    FillMemory(&dcb, sizeof(dcb), 0);
    if (!GetCommState(hComm, &dcb))     
    {
        // get current DCB
        WIOS_TRACE("DEBUG: WIOSerial::WIOSerial() - Unable to create DCB\n");
    }
}


WIOSerial::~WIOSerial()
{
    // Let our parent take care if itself 1st.
    WIOS_TRACE("WIOSerial::~WIOSerial()\n");

    if (NULL != WIOSerial::hInBufferMutex)
    {
        CloseHandle(WIOSerial::hInBufferMutex);
        WIOSerial::hInBufferMutex = NULL;
    }
}
