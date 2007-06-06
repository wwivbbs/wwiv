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

#include "wwiv.h"

bool CreateSyncTempFile( std::string &outFileName, const std::string commandLine );
void CreateSyncFosCommandLine( std::string &outCommandLine, const std::string tempFilePath, int nSyncMode );
bool DoSyncFosLoopNT( HANDLE hProcess, HANDLE hSyncHangupEvent, HANDLE hSyncReadSlot, int nSyncMode  );
bool DoSyncFosLoop9X( HANDLE hProcess, HMODULE hKernel32, HANDLE hSyncStartEvent, HANDLE hSbbsExecVxd, HANDLE hSyncHangupEvent, int nSyncMode  );
bool DoSyncFosLoop9XImpl( HANDLE hProcess, HANDLE hSyncStartEvent, HANDLE hSbbsExecVxd, HANDLE hSyncHangupEvent, int nSyncMode  );

void GetSyncFosTempFilePath( std::string &outFileName );
bool VerifyDosXtrnExists();
const std::string GetDosXtrnPath();
const std::string GetSyncFosOSMode();
bool DeleteSyncTempFile();
bool IsWindowsNT();
bool ExpandWWIVHeartCodes( char *pszBuffer );


static FILE* hLogFile;


const int   CONST_SBBSFOS_FOSSIL_MODE           = 0;
const int   CONST_SBBSFOS_DOSIN_MODE            = 1;
const int   CONST_SBBSFOS_DOSOUT_MODE           = 2;
const int   CONST_SBBSFOS_LOOPS_BEFORE_YIELD    = 10;
const int   CONST_SBBSFOS_BUFFER_SIZE           = 5000;
const int   CONST_SBBSFOS_BUFFER_PADDING        = 1000; // padding in case of errors.

const DWORD SBBSEXEC_IOCTL_START		        = 0x8001;
const DWORD SBBSEXEC_IOCTL_COMPLETE		        = 0x8002;
const DWORD SBBSEXEC_IOCTL_READ			        = 0x8003;
const DWORD SBBSEXEC_IOCTL_WRITE		        = 0x8004;
const DWORD SBBSEXEC_IOCTL_DISCONNECT	        = 0x8005;
const DWORD SBBSEXEC_IOCTL_STOP			        = 0x8006;

const int CONST_NUM_LOOPS_BEFORE_EXIT_CHECK         = 500;
const int CONST_WIN9X_NUM_LOOPS_BEFORE_EXIT_CHECK   = 10;

struct sbbsexecrec
{
    DWORD	dwMode;
    HANDLE	hEvent;
};



typedef HANDLE (WINAPI *OPENVXDHANDLEFUNC)(HANDLE);


int ExecExternalProgram( const std::string commandLine, int flags )
{
    bool bUsingSync = false;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );
    std::string workingCommandLine;

    bool bShouldUseSync = false;
    int nSyncMode = 0;
    if ( GetSession()->using_modem && ( flags & EFLAG_FOSSIL ) )
    {
        bShouldUseSync = true;
    }
    else if ( flags & EFLAG_COMIO )
    {
        nSyncMode |= CONST_SBBSFOS_DOSIN_MODE;
        nSyncMode |= CONST_SBBSFOS_DOSOUT_MODE;
        bShouldUseSync = true;
    }

    if ( bShouldUseSync )
    {
        std::string syncFosTempFile;
        if ( !CreateSyncTempFile( syncFosTempFile, commandLine ) )
        {
            return -1;
        }
        CreateSyncFosCommandLine( workingCommandLine, syncFosTempFile, nSyncMode );
        bUsingSync = true;
        char szTempLogFileName[ MAX_PATH ];
        _snprintf( szTempLogFileName, sizeof( szTempLogFileName ), "%swwivsync.log", GetApplication()->GetHomeDir() );
        hLogFile = fopen( szTempLogFileName, "at" );
        fprintf( hLogFile, charstr( 78, '=' ) );
        fprintf( hLogFile, "\r\n\r\n" );
        fprintf( hLogFile, "Cmdline = [%s]\r\n\n", workingCommandLine.c_str() );
    }
    else
    {
        workingCommandLine = commandLine;
    }


    DWORD dwCreationFlags = 0;

    if ( !IsWindowsNT() )
    {
        // We only need the detached console on Win9x.
        dwCreationFlags = CREATE_NEW_CONSOLE;
    }

    char * pszTitle = new char[ 255 ];
    _snprintf( pszTitle, sizeof( pszTitle ), "%s in door on node %d",
             GetSession()->GetCurrentUser()->GetName(), GetApplication()->GetInstanceNumber() );
    si.lpTitle = pszTitle;

	if ( ok_modem_stuff && !bUsingSync )
    {
		GetSession()->remoteIO()->close( true );
	}

    HMODULE hKernel32 = NULL;
    HANDLE hSyncStartEvent = INVALID_HANDLE_VALUE;
    HANDLE hSbbsExecVxd = INVALID_HANDLE_VALUE;
    HANDLE hSyncHangupEvent = INVALID_HANDLE_VALUE;

    HANDLE hSyncReadSlot    = INVALID_HANDLE_VALUE;     // Mailslot for reading


    if ( bUsingSync && !IsWindowsNT() )
    {
        hKernel32 = LoadLibrary("KERNEL32");
        if ( hKernel32 == NULL )
        {
            // TODO Show error
            return false;
        }

        fprintf( hLogFile, " Starting DoSyncFosLoop9XImpl\r\n" );

        OPENVXDHANDLEFUNC OpenVxDHandle = ( OPENVXDHANDLEFUNC ) GetProcAddress(hKernel32, "OpenVxDHandle");
        if ( OpenVxDHandle == NULL )
        {
            fprintf( hLogFile, " ERROR 1\r\n" );
            // TODO Show error
            return false;
        }

        hSyncHangupEvent = INVALID_HANDLE_VALUE;     // Event to hangup program

        char szSbbsExecVxdName[ MAX_PATH ];
        _snprintf( szSbbsExecVxdName, sizeof( szSbbsExecVxdName ), "\\\\.\\%ssbbsexec.vxd", GetApplication()->GetHomeDir() );
        fprintf( hLogFile, "Opening VXD: [%s]\r\n", szSbbsExecVxdName );
        hSbbsExecVxd = CreateFile( szSbbsExecVxdName, 0, 0, 0, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, 0 );
        if ( hSbbsExecVxd == INVALID_HANDLE_VALUE )
        {
            fprintf( hLogFile, " ERROR 2\r\n" );
            // TODO Show error
            return false;
        }

        hSyncStartEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
        if ( hSyncStartEvent == INVALID_HANDLE_VALUE )
        {
            sysoplogf( "!!! Unable to create Start Event for SyncFoss External program [%ld]", GetLastError() );
            fprintf( hLogFile, "!!! Unable to create Start Event for SyncFoss External program [%ld]", GetLastError() );
            return false;
        }

        sbbsexecrec start;
        if ( OpenVxDHandle != NULL )
        {
            start.hEvent = OpenVxDHandle( hSyncStartEvent );
        }
        else
        {
            start.hEvent = hSyncStartEvent;
        }
        start.dwMode = (DWORD) nSyncMode;

        DWORD dwRecvCount = 0;

        if ( !DeviceIoControl
                (
                hSbbsExecVxd,
                SBBSEXEC_IOCTL_START,
			    &start,                 // pointer to buffer to supply input data
			    sizeof( start ),        // size of input buffer
			    NULL,                   // pointer to buffer to receive output data
			    0,                      // size of output buffer
			    &dwRecvCount,           // pointer to variable to receive output byte count
			    NULL                    // Overlapped I/O
			    )
            )
        {
            // TODO Show error
            CloseHandle( hSbbsExecVxd );
            fprintf( hLogFile, " ERROR 4\r\n" );
            return false;
        }
    }
    else if ( bUsingSync && IsWindowsNT() )
    {
        // Create Hangup Event.
        char szHangupEventName[ MAX_PATH + 1 ];
        _snprintf( szHangupEventName, sizeof( szHangupEventName ), "sbbsexec_hungup%d", GetApplication()->GetInstanceNumber() );
        hSyncHangupEvent = CreateEvent( NULL, TRUE, FALSE, szHangupEventName );
        if ( hSyncHangupEvent == INVALID_HANDLE_VALUE )
        {
            fprintf( hLogFile, "!!! Unable to create Hangup Event for SyncFoss External program [%ld]", GetLastError() );
            sysoplogf( "!!! Unable to create Hangup Event for SyncFoss External program [%ld]", GetLastError() );
            return false;
        }

        // Create Read Mail Slot
        char szReadSlotName[ MAX_PATH + 1];
        _snprintf( szReadSlotName, sizeof( szReadSlotName ), "\\\\.\\mailslot\\sbbsexec\\rd%d", GetApplication()->GetInstanceNumber() );
        hSyncReadSlot = CreateMailslot( szReadSlotName, CONST_SBBSFOS_BUFFER_SIZE, 0, NULL );
        if ( hSyncReadSlot == INVALID_HANDLE_VALUE )
        {
            fprintf( hLogFile, "!!! Unable to create mail slot for reading for SyncFoss External program [%ld]", GetLastError() );
            sysoplogf( "!!! Unable to create mail slot for reading for SyncFoss External program [%ld]", GetLastError() );
            CloseHandle( hSyncHangupEvent );
            hSyncHangupEvent = INVALID_HANDLE_VALUE;
            return false;
        }
    }


    char szCurDir[ MAX_PATH ];
    WWIV_GetDir( szCurDir, true );

    ::Sleep( 250 );
    
    char szTempWorkingCommandline[MAX_PATH + 1];
    strncpy( szTempWorkingCommandline, workingCommandLine.c_str(), MAX_PATH );
    BOOL bRetCP = CreateProcess(
        NULL,
        szTempWorkingCommandline,
        NULL,
        NULL,
        TRUE,
        dwCreationFlags,
        NULL, // GetApplication()->xenviron not using NULL causes things to not work.
        szCurDir,
        &si,
        &pi );

    if ( !bRetCP )
    {
        delete[] pszTitle;
        sysoplogf( "!!! CreateProcess failed for command: [%s] with Error Code %ld", workingCommandLine.c_str(), GetLastError() );
        if ( bUsingSync )
        {
            fprintf( hLogFile, "!!! CreateProcess failed for command: [%s] with Error Code %ld", workingCommandLine.c_str(), GetLastError() );
        }
        return -1;
    }

	// If we are on Windows NT and GetSession()->IsExecUseWaitForInputIdle() is true use this.
    if ( IsWindowsNT() && GetSession()->IsExecUseWaitForInputIdle() )
    {
		int dwWaitRet = ::WaitForInputIdle( pi.hProcess, GetSession()->GetExecWaitForInputTimeout() );
        if ( dwWaitRet != 0 )
        {
            if ( bUsingSync )
            {
                fprintf( hLogFile, "!!! WaitForInputIdle Failed with code %ld", dwWaitRet );
            }
            sysoplogf( "!!! WaitForInputIdle Failed with code %ld", dwWaitRet );
        }
    }
    else
    {
		::Sleep( GetSession()->GetExecWaitForInputTimeout() );
        ::Sleep( 0 );
        ::Sleep( 0 );
        ::Sleep( 0 );
        ::Sleep( 0 );
    }
    ::Sleep( 0 );

    CloseHandle( pi.hThread );


    if ( bUsingSync )
    {
        bool bSavedBinaryMode = GetSession()->remoteIO()->GetBinaryMode();
        GetSession()->remoteIO()->SetBinaryMode( true );
        bool bSyncLoopStatus = false;
        if ( IsWindowsNT() )
        {
            bSyncLoopStatus = DoSyncFosLoopNT( pi.hProcess, hSyncHangupEvent, hSyncReadSlot, nSyncMode );
            fprintf( hLogFile,  "DoSyncFosLoopNT: Returning %s\r\n", ( bSyncLoopStatus ) ? "TRUE" : "FALSE" );
        }
        else
        {
            bSyncLoopStatus = DoSyncFosLoop9X( pi.hProcess, hKernel32, hSyncStartEvent, hSbbsExecVxd, hSyncHangupEvent, nSyncMode );
            fprintf( hLogFile,  "DoSyncFosLoop9X: Returning %s\r\n", ( bSyncLoopStatus ) ? "TRUE" : "FALSE" );
        }
        fprintf( hLogFile, charstr( 78, '=' ) );
        fprintf( hLogFile, "\r\n\r\n\r\n" );
        fclose( hLogFile );

        if ( !bSyncLoopStatus )
        {
            // TODO Log error
        }
        else
        {
            DWORD dwExitCode = 0;
            GetExitCodeProcess( pi.hProcess, &dwExitCode );
            if ( dwExitCode == STILL_ACTIVE )
            {
                TerminateProcess( pi.hProcess, 0 );
            }
        }
        GetSession()->remoteIO()->SetBinaryMode( bSavedBinaryMode );
    }
    else
    {
        // Wait until child process exits.
        WaitForSingleObject( pi.hProcess, INFINITE );
    }

    DWORD dwExitCode = 0;
    GetExitCodeProcess( pi.hProcess, &dwExitCode );

    // Close process and thread handles.
    CloseHandle( pi.hProcess );

    delete[] pszTitle;

	// reengage comm stuff
	if ( ok_modem_stuff && !bUsingSync )
    {
		GetSession()->remoteIO()->open();
		GetSession()->remoteIO()->dtr( true );
    }

    return static_cast< int >( dwExitCode );
}


//
// Helper functions
//

bool CreateSyncTempFile( std::string &outFileName, const std::string commandLine )
{
    GetSyncFosTempFilePath( outFileName );
    DeleteSyncTempFile();

    WTextFile file( outFileName, "wt" );
    if ( !file.IsOpen() )
    {
        return false;
    }
    file.Write( commandLine.c_str() );
    if ( !file.Close() )
    {
        return false;
    }

    return true;
}

void GetSyncFosTempFilePath( std::string &outFileName )
{
    outFileName = syscfgovr.tempdir;
    outFileName += "WWIVSYNC.ENV";
}


void CreateSyncFosCommandLine( std::string &outCommandLine, const std::string tempFilePath, int nSyncMode )
{
    std::stringstream sstream;
    sstream << GetDosXtrnPath() << " " << tempFilePath << " " << GetSyncFosOSMode() << " ";
    sstream << GetApplication()->GetInstanceNumber() << " " << nSyncMode << " " << CONST_SBBSFOS_LOOPS_BEFORE_YIELD;
    outCommandLine = sstream.str();
}

bool VerifyDosXtrnExists()
{
    return WFile::Exists( GetDosXtrnPath() );
}


const std::string GetDosXtrnPath()
{
    std::stringstream sstream;
    sstream << GetApplication()->GetHomeDir() << "DOSXTRN.EXE";
    return std::string( sstream.str() );
}


const std::string GetSyncFosOSMode()
{
    return IsWindowsNT() ? std::string("NT") : std::string("95");
}


bool IsWindowsNT()
{
    DWORD dwVersion = GetVersion();
    // Windows NT/2000/XP is < 0x80000000
    return ( dwVersion < 0x80000000 ) ? true : false;
}


// returns true if the file is deleted.
bool DeleteSyncTempFile()
{
    std::string tempFileName;
    GetSyncFosTempFilePath( tempFileName );
    if ( WFile::Exists( tempFileName ) )
    {
        WFile::Remove(tempFileName );
        return true;
    }
    return false;
}

#define SYNC_LOOP_CLEANUP() {   \
    if ( hSyncHangupEvent != INVALID_HANDLE_VALUE ) CloseHandle( hSyncHangupEvent ); hSyncHangupEvent = INVALID_HANDLE_VALUE; \
    if ( hSyncReadSlot    != INVALID_HANDLE_VALUE ) CloseHandle( hSyncReadSlot );    hSyncReadSlot    = INVALID_HANDLE_VALUE; \
    if ( hSyncWriteSlot   != INVALID_HANDLE_VALUE ) CloseHandle( hSyncWriteSlot );   hSyncWriteSlot   = INVALID_HANDLE_VALUE; \
}


bool DoSyncFosLoopNT( HANDLE hProcess, HANDLE hSyncHangupEvent, HANDLE hSyncReadSlot, int nSyncMode  )
{
    fprintf( hLogFile, "Starting DoSyncFosLoopNT()\r\n" );
    HANDLE hSyncWriteSlot   = INVALID_HANDLE_VALUE;     // Mailslot for writing

    char szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + CONST_SBBSFOS_BUFFER_PADDING ];
    ::ZeroMemory( szReadBuffer, CONST_SBBSFOS_BUFFER_SIZE + CONST_SBBSFOS_BUFFER_PADDING );
    szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 1 ] = '\xFE';

    int nCounter = 0;
    for( ;; )
    {
        nCounter++;
        if ( GetSession()->using_modem && ( !GetSession()->remoteIO()->carrier() ) )
        {
            SetEvent( hSyncHangupEvent );
            fprintf( hLogFile, "Setting Hangup Event and Sleeping\r\n" );
            ::Sleep( 1000 );
        }

        if ( nCounter > CONST_NUM_LOOPS_BEFORE_EXIT_CHECK )
        {
            // Only check for exit when we've been idle
            DWORD dwExitCode = 0;
            if ( GetExitCodeProcess( hProcess, &dwExitCode ) )
            {
                if ( dwExitCode != STILL_ACTIVE )
                {
                    // process is done and so are we.
                    fprintf( hLogFile, "Process finished, exiting\r\n" );
                    SYNC_LOOP_CLEANUP();
                    return true;
                }
            }
        }

		if ( GetSession()->remoteIO()->incoming() )
        {
            nCounter = 0;
            // SYNCFOS_DEBUG_PUTS( "Char available to send to the door" );
            int nNumReadFromComm = GetSession()->remoteIO()->read( szReadBuffer, CONST_SBBSFOS_BUFFER_SIZE );
            fprintf( hLogFile, "Read [%d] from comm\r\n", nNumReadFromComm );
#if 1
			int nLp = 0;
            for( nLp = 0; nLp < nNumReadFromComm; nLp++ )
            {
                fprintf( hLogFile, "[%u]", static_cast<unsigned char>( szReadBuffer[ nLp ] ), ( unsigned char ) szReadBuffer[ nLp ] );
            }
            fprintf( hLogFile, "   " );
            for( nLp = 0; nLp < nNumReadFromComm; nLp++ )
            {
                fprintf( hLogFile, "[%c]", static_cast<unsigned char>( szReadBuffer[ nLp ] ) );
            }
            fprintf( hLogFile, "\r\n" );
#endif // 0

            if ( hSyncWriteSlot == INVALID_HANDLE_VALUE )
            {
                // Create Write handle.
                char szWriteSlotName[ MAX_PATH ];
                ::Sleep(500);
                _snprintf( szWriteSlotName, sizeof( szWriteSlotName ), "\\\\.\\mailslot\\sbbsexec\\wr%d", GetApplication()->GetInstanceNumber() );
                fprintf( hLogFile, "Creating Mail Slot [%s]\r\n", szWriteSlotName );

                hSyncWriteSlot = CreateFile( szWriteSlotName,
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ,
                                            NULL,
                                            OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL,
                                            (HANDLE) NULL );
                if ( hSyncWriteSlot == INVALID_HANDLE_VALUE )
                {
                    sysoplogf( "!!! Unable to create mail slot for writing for SyncFoss External program [%ld]", GetLastError() );
                    fprintf( hLogFile, "!!! Unable to create mail slot for writing for SyncFoss External program [%ld]", GetLastError() );
                    SYNC_LOOP_CLEANUP();
                    return false;
                }
                fprintf( hLogFile, "Created MailSlot\r\n" );

            }

            DWORD dwNumWrittenToSlot = 0;
            WriteFile( hSyncWriteSlot, szReadBuffer, nNumReadFromComm, &dwNumWrittenToSlot, NULL );
            fprintf( hLogFile, "Wrote [%ld] to MailSlot\r\n", dwNumWrittenToSlot );
        }

        int nBufferPtr      = 0;    // BufPtr
        DWORD dwNextSize    = 0;    // InUsed
        DWORD dwNumMessages = 0;    // TmpLong
        DWORD dwReadTimeOut = 0;    // -------

        if ( GetMailslotInfo( hSyncReadSlot, NULL, &dwNextSize, &dwNumMessages, &dwReadTimeOut ) )
        {
            if ( dwNumMessages > 0 )
            {
				fprintf( hLogFile, "[%ld/%ld] slot messages\r\n", dwNumMessages, std::min<DWORD>( dwNumMessages, CONST_SBBSFOS_BUFFER_SIZE - 1000 ) );
                dwNumMessages = std::min<DWORD>( dwNumMessages, CONST_SBBSFOS_BUFFER_SIZE - 1000 );
                dwNextSize = 0;

                for( unsigned int nSlotCounter = 0; nSlotCounter < dwNumMessages; nSlotCounter++ )
                {
                    if ( ReadFile( hSyncReadSlot,
                        &szReadBuffer[ nBufferPtr ],
                        CONST_SBBSFOS_BUFFER_SIZE - nBufferPtr,
                        &dwNextSize,
                        NULL ) )
                    {
                        if ( dwNextSize > 1 )
                        {
                            fprintf( hLogFile, "[%ld] bytes read   \r\n", dwNextSize );
                        }
                        if ( dwNextSize == MAILSLOT_NO_MESSAGE )
                        {
                            fprintf( hLogFile, "** MAILSLOT thinks it's empty!\r\n" );
                        }
                        else
                        {
                            nBufferPtr += dwNextSize;   // was just ++.. oops
                        }

                        if ( nBufferPtr >= CONST_SBBSFOS_BUFFER_SIZE - 10 )
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                ::Sleep( 0 );
            }

            if ( nBufferPtr > 0 )
            {
                nCounter = 0;
                if ( nSyncMode & CONST_SBBSFOS_DOSOUT_MODE )
                {
                    // For some reason this doesn't write twice locally, so it works pretty well.
                    szReadBuffer[ nBufferPtr ] = '\0';
                    fprintf( hLogFile, "{%s}\r\n", szReadBuffer );
                    GetSession()->bout << szReadBuffer;

                    //ExpandWWIVHeartCodes( szReadBuffer );
                    //int nNumWritten = GetSession()->remoteIO()->write( szReadBuffer, strlen( szReadBuffer )  );
                    //fprintf( hLogFile, "Wrote [%d] bytes to comm.\r\n", nNumWritten );
                }
                else
                {
                    int nNumWritten = GetSession()->remoteIO()->write( szReadBuffer, nBufferPtr );
                    fprintf( hLogFile, "Wrote [%d] bytes to comm.\r\n", nNumWritten );
                }

            }
            if ( szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 1 ] != '\xFE' )
            {
                fprintf( hLogFile, "!!!!!! MEMORY CORRUPTION ON szReadBuffer !!!!!!!!\r\n\r\n\n\n");
            }
        }
        if ( nCounter > 0 )
        {
            ::Sleep( 0 );
        }
    }
}


bool DoSyncFosLoop9X(  HANDLE hProcess, HMODULE hKernel32, HANDLE hSyncStartEvent, HANDLE hSbbsExecVxd, HANDLE hSyncHangupEvent, int nSyncMode  )
{
    bool bRet = DoSyncFosLoop9XImpl( hProcess, hSyncStartEvent, hSbbsExecVxd, hSyncHangupEvent, nSyncMode );

    if ( hKernel32 != NULL )
    {
        FreeLibrary( hKernel32 );
        hKernel32 = NULL;
    }

    fprintf( hLogFile,  "DoSyncFosLoop9X: Returning %s\r\n", ( bRet ) ? "TRUE" : "FALSE" );
    fprintf( hLogFile, charstr( 78, '=' ) );
    fprintf( hLogFile, "\r\n\r\n\r\n" );
    fclose( hLogFile );

    return bRet;
}


bool DoSyncFosLoop9XImpl( HANDLE hProcess, HANDLE hSyncStartEvent, HANDLE hSbbsExecVxd, HANDLE hSyncHangupEvent, int nSyncMode  )
{
    DWORD hVM;
    DWORD rd;
    DWORD dwRecvCount = 0;

    time_t then = time(NULL);
    fprintf( hLogFile, "Starting Wait at [%ld]\r\n", then );
    DWORD dwWaitStatus = WaitForSingleObject( hSyncStartEvent, 15000 );
    if ( dwWaitStatus != WAIT_OBJECT_0 )
    {
        time_t now = time(NULL);
        fprintf( hLogFile, " ERROR 5 at [%ld] (elapsed = %ld)\r\n", now, ( now - then ) );
        if (!TerminateProcess( hProcess, 1 ) )
        {
            fprintf( hLogFile, " UNABLE TO TERM PROCESS AFTER ERROR 5\r\n" );
        }
        // TODO Show error
        return false;
    }

    CloseHandle( hSyncStartEvent );
    hSyncStartEvent = NULL;

    if( !DeviceIoControl(
            hSbbsExecVxd,					// handle to device of interest
            SBBSEXEC_IOCTL_COMPLETE,	// control code of operation to perform
            NULL,					// pointer to buffer to supply input data
            0,						// size of input buffer
            &hVM,					// pointer to buffer to receive output data
            sizeof(hVM),			// size of output buffer
            &rd,					// pointer to variable to receive output byte count
            NULL					// Overlapped I/O
        ))
    {
        TerminateProcess( hProcess, 1 );
        CloseHandle( hProcess );
        CloseHandle( hSbbsExecVxd );
        if ( hSyncStartEvent != INVALID_HANDLE_VALUE )
        {
            CloseHandle( hSyncStartEvent );
            hSyncStartEvent = INVALID_HANDLE_VALUE;
        }
        fprintf( hLogFile, " ERROR 6\r\n" );
        // TODO Show error
        return false;
    }

// BEGIN LOOP

    char szReadBuffer[ CONST_SBBSFOS_BUFFER_SIZE ];

    int nCounter = 0;
    for( ;; )
    {
        fprintf( hLogFile, " IN LOOP\r\n" );
        nCounter++;
        if ( GetSession()->using_modem && ( !GetSession()->remoteIO()->carrier() ) )
        {
            SetEvent( hSyncHangupEvent );
            nCounter += CONST_WIN9X_NUM_LOOPS_BEFORE_EXIT_CHECK;
            ::Sleep( 1000 );
        }

        DWORD dwExitCode = 0;
        if ( nCounter > CONST_WIN9X_NUM_LOOPS_BEFORE_EXIT_CHECK )
        {
            // Changed this to 10 loops since Eli said this was hanging on Win9x
            if ( GetExitCodeProcess( hProcess, &dwExitCode ) )
            {
                if ( dwExitCode != STILL_ACTIVE )
                {
                    // process is done and so are we.
                    //SYNC_LOOP_CLEANUP();
                    fprintf( hLogFile, " APP TERMINATED \r\n" );
                    return true;
                }
            }
        }

		if ( GetSession()->remoteIO()->incoming() )
        {
            nCounter = 0;
            // SYNCFOS_DEBUG_PUTS( "Char available to send to the door" );
            int nNumReadFromComm = GetSession()->remoteIO()->read( ( szReadBuffer + sizeof( hVM ) ), CONST_SBBSFOS_BUFFER_SIZE - sizeof( hVM ) );
            fprintf( hLogFile, "nNumReadFromComm = [%d]\r\n", nNumReadFromComm );
            for( int i = sizeof( hVM ); i < static_cast<int>( nNumReadFromComm + sizeof( hVM ) ); i++ )
            {
                fprintf( hLogFile, "COMM: [%d][%c]", szReadBuffer[i], szReadBuffer[i] );
            }
            nNumReadFromComm += sizeof( hVM );
            *( DWORD* ) szReadBuffer = hVM;
            if( !DeviceIoControl(
                    hSbbsExecVxd,           // handle to device of interest
                    SBBSEXEC_IOCTL_WRITE,   // control code of operation to perform
                    szReadBuffer,           // pointer to buffer to supply input data
                    nNumReadFromComm,       // size of input buffer
                    &rd,                    // pointer to buffer to receive output data
                    sizeof(rd),             // size of output buffer
                    &dwRecvCount,           // pointer to variable to receive output byte count
                    NULL                    // Overlapped I/O
             ) )
            {
                fprintf( hLogFile, " BREAK 1\r\n" );
                // TODO Show error
                break;
            }
            fprintf( hLogFile, "Read from COMM = [%d]\r\n", nNumReadFromComm - sizeof( hVM ) );
        }

        fprintf( hLogFile, "hVM = [%lud]\r\n", hVM );

        DWORD dwNumReadFromVXD = 0;
        DWORD dwOutBufferSize = sizeof( szReadBuffer );
        if( !DeviceIoControl(
                hSbbsExecVxd,			// handle to device of interest
                SBBSEXEC_IOCTL_READ,	// control code of operation to perform
                &hVM,					// pointer to buffer to supply input data
                sizeof( hVM ),			// size of input buffer
                szReadBuffer,			// pointer to buffer to receive output data
                dwOutBufferSize,		// size of output buffer
                &dwNumReadFromVXD,		// pointer to variable to receive output byte count
                NULL					// Overlapped I/O
        ))
        {
            fprintf( hLogFile, " BREAK 2\r\n" );
            // TODO Show error
            break;
        }

        fprintf( hLogFile, "dwNumReadFromVXD = [%ld]\r\n", dwNumReadFromVXD );
        if ( dwNumReadFromVXD > 0 )
        {
            // LogDebug( "Number of bytes to send to the user=%d ", nBufferPtr );
            nCounter = 0;
            if ( nSyncMode & CONST_SBBSFOS_DOSOUT_MODE )
            {
                ExpandWWIVHeartCodes( szReadBuffer );
            }
            GetSession()->remoteIO()->write( szReadBuffer, dwNumReadFromVXD );
        }

        if ( nCounter > 0 )
        {
            ::Sleep( 100 );
        }
    }



// END LOOP
    fprintf( hLogFile, " END LOOP \r\n" );

    if( !DeviceIoControl(
	        hSbbsExecVxd,			// handle to device of interest
	        SBBSEXEC_IOCTL_STOP,	// control code of operation to perform
	        &hVM,					// pointer to buffer to supply input data
	        sizeof(hVM),			// size of input buffer
	        NULL,					// pointer to buffer to receive output data
	        0,						// size of output buffer
	        &rd,					// pointer to variable to receive output byte count
	        NULL					// Overlapped I/O
	    ) )
    {
	    // TODO Show error
        TerminateProcess( hProcess, 1 );
        return false;
    }

    return true;
}


bool ExpandWWIVHeartCodes( char *pszBuffer )
{
    curatr = 0;
    char szTempBuffer[ CONST_SBBSFOS_BUFFER_SIZE + 100 ];
    char *pIn = pszBuffer;
    char *pOut = szTempBuffer;
    int n = 0;
    while( *pIn && n++ < ( CONST_SBBSFOS_BUFFER_SIZE ) )
    {
        if ( *pIn == '\x03' )
        {
            pIn++;
            if ( *pIn >= '0' && *pIn <= '9' )
            {
                char szTempColor[ 81 ];
                int nColor = *pIn - '0';
                makeansi( GetSession()->GetCurrentUser()->GetColor( nColor ), szTempColor, false );
                char *pColor = szTempColor;
                while ( *pColor )
                {
                    *pOut++ = *pColor++;
                }
                pIn++;
            }
            else
            {
                *pOut++ = '\x03';
            }
        }
        *pOut++ = *pIn++;
    }
    *pOut++ = '\0';
    strcpy( pszBuffer, szTempBuffer );
    return true;
}


