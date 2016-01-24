/****************************************************************************
*																			*
*						WinCE Randomness-Gathering Code						*
*						Copyright Peter Gutmann 1996-2005					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom number
   generator.  For usage conditions, see random.c */

/* General includes */

#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */

#include <tlhelp32.h>

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	4096

/* Handles to various randomness objects */

static HANDLE hToolHelp32;	/* Handle to Toolhelp.library */
static HANDLE hThread;		/* Background polling thread handle */
static DWORD threadID;		/* Background polling thread ID */

/****************************************************************************
*																			*
*									Fast Poll								*
*																			*
****************************************************************************/

/* Type definitions for function pointers to call CE native functions */

typedef BOOL ( *CEGENRANDOM )( DWORD dwLen, BYTE *pbBuffer );
typedef DWORD ( *GETSYSTEMPOWERSTATUS )( PSYSTEM_POWER_STATUS_EX2 pSystemPowerStatusEx2,
										 DWORD dwLen, BOOL fUpdate );

/* The shared Win32 fast poll routine */

void fastPoll( void )
	{
	static BOOLEAN addedFixedItems = FALSE, hasHardwareRNG = FALSE;
	static CEGENRANDOM pCeGenRandom = NULL;
	static GETSYSTEMPOWERSTATUS pGetSystemPowerStatusEx2 = NULL;
	FILETIME  creationTime, exitTime, kernelTime, userTime;
	LARGE_INTEGER performanceCount;
	SYSTEM_POWER_STATUS_EX2 powerStatus;
	MEMORYSTATUS memoryStatus;
	HANDLE handle;
	POINT point;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	int length;

	if( krnlIsExiting() )
		return;

	/* Initialize the native function pointers if necessary.  CeGetRandom()
	   is only available in relatively new versions of WinCE, so we have to
	   link it dynamically */
	if( pCeGenRandom == NULL )
		{
		HANDLE hCoreDLL;

		if( ( hCoreDLL = GetModuleHandle( TEXT( "Coredll.dll" ) ) ) != NULL )
			pCeGenRandom = ( CEGENRANDOM ) GetProcAddress( hCoreDLL, TEXT( "CeGenRandom" ) );
		}
	if( pGetSystemPowerStatusEx2 == NULL )
		{
		HANDLE hGetpower;

		if( ( hGetpower = GetModuleHandle( TEXT( "Getpower.dll" ) ) ) != NULL )
			pGetSystemPowerStatusEx2 = ( GETSYSTEMPOWERSTATUS ) \
							GetProcAddress( hGetpower, TEXT( "GetSystemPowerStatusEx2" ) );
		}

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get various basic pieces of system information: Handle of active
	   window, handle of window with mouse capture, handle of clipboard owner
	   handle of start of clpboard viewer list, pseudohandle of current
	   process, current process ID, pseudohandle of current thread, current
	   thread ID, handle of desktop window, handle  of window with keyboard
	   focus, whether system queue has any events, cursor position for last
	   message, 1 ms time for last message, handle of window with clipboard
	   open, handle of process heap, handle of procs window station, types of
	   events in input queue, and milliseconds since Windows was started */
	addRandomValue( randomState, GetActiveWindow() );
	addRandomValue( randomState, GetCapture() );
	addRandomValue( randomState, GetCaretBlinkTime() );
	addRandomValue( randomState, GetClipboardOwner() );
	addRandomValue( randomState, GetCurrentProcess() );
	addRandomValue( randomState, GetCurrentProcessId() );
	addRandomValue( randomState, GetCurrentThread() );
	addRandomValue( randomState, GetCurrentThreadId() );
	addRandomValue( randomState, GetDesktopWindow() );
	addRandomValue( randomState, GetDC( NULL ) );
	addRandomValue( randomState, GetDoubleClickTime() );
	addRandomValue( randomState, GetFocus() );
	addRandomValue( randomState, GetForegroundWindow() );
	addRandomValue( randomState, GetMessagePos() );
	addRandomValue( randomState, GetOpenClipboardWindow() );
	addRandomValue( randomState, GetProcessHeap() );
	addRandomValue( randomState, GetQueueStatus( QS_ALLINPUT ) );
	addRandomValue( randomState, GetTickCount() );
	if( krnlIsExiting() )
		return;

	/* Get multiword system information: Current caret position, current
	   mouse cursor position */
	GetCaretPos( &point );
	addRandomData( randomState, &point, sizeof( POINT ) );
	GetCursorPos( &point );
	addRandomData( randomState, &point, sizeof( POINT ) );

	/* Get percent of memory in use, bytes of physical memory, bytes of free
	   physical memory, bytes in paging file, free bytes in paging file, user
	   bytes of address space, and free user bytes */
	memoryStatus.dwLength = sizeof( MEMORYSTATUS );
	GlobalMemoryStatus( &memoryStatus );
	addRandomData( randomState, &memoryStatus, sizeof( MEMORYSTATUS ) );

	/* Get thread and process creation time, exit time, time in kernel mode,
	   and time in user mode in 100ns intervals */
	handle = GetCurrentThread();
	GetThreadTimes( handle, &creationTime, &exitTime, &kernelTime, &userTime );
	addRandomData( randomState, &creationTime, sizeof( FILETIME ) );
	addRandomData( randomState, &exitTime, sizeof( FILETIME ) );
	addRandomData( randomState, &kernelTime, sizeof( FILETIME ) );
	addRandomData( randomState, &userTime, sizeof( FILETIME ) );

	/* Get extended battery/power status information.  We set the fUpdate
	   flag to force a re-read of fresh data rather than a re-use of cached
	   information */
	if( pGetSystemPowerStatusEx2 != NULL && \
		( length = \
				pGetSystemPowerStatusEx2( &powerStatus,
										  sizeof( SYSTEM_POWER_STATUS_EX2 ),
										  TRUE ) ) > 0 )
		addRandomData( randomState, &powerStatus, length );

	/* Get random data provided by the OS.  Since this is expected to be
	   provided by the system vendor, it's quite likely to be the usual
	   process ID + time */
	if( pCeGenRandom != NULL )
		{
		BYTE randomBuffer[ 32 ];

		if( pCeGenRandom( 32, randomBuffer ) )
			addRandomData( randomState, randomBuffer, 32 );
		}

	/* The following are fixed for the lifetime of the process so we only
	   add them once */
	if( !addedFixedItems )
		{
		SYSTEM_INFO systemInfo;

		GetSystemInfo( &systemInfo );
		addRandomData( randomState, &systemInfo, sizeof( SYSTEM_INFO ) );
		addedFixedItems = TRUE;
		}

	/* The performance of QPC varies depending on the architecture it's
	   running on, and is completely platform-dependant.  If there's no
	   hardware performance counter available, it uses the 1ms system timer,
	   although usually there's some form of hardware timer available.
	   Since there may be no correlation, or only a weak correlation,
	   between the performance counter and the system clock, we get the
	   time from both sources */
	if( QueryPerformanceCounter( &performanceCount ) )
		addRandomData( randomState, &performanceCount,
					   sizeof( LARGE_INTEGER ) );
	addRandomValue( randomState, GetTickCount() );

	/* Flush any remaining data through.  Quality = int( 33 1/3 % ) */
	endRandomData( randomState, 34 );
	}

/****************************************************************************
*																			*
*									Slow Poll								*
*																			*
****************************************************************************/

/* Type definitions for function pointers to call Toolhelp32 functions */

typedef BOOL ( WINAPI *MODULEWALK )( HANDLE hSnapshot, LPMODULEENTRY32 lpme );
typedef BOOL ( WINAPI *THREADWALK )( HANDLE hSnapshot, LPTHREADENTRY32 lpte );
typedef BOOL ( WINAPI *PROCESSWALK )( HANDLE hSnapshot, LPPROCESSENTRY32 lppe );
typedef BOOL ( WINAPI *HEAPLISTWALK )( HANDLE hSnapshot, LPHEAPLIST32 lphl );
typedef BOOL ( WINAPI *HEAPFIRST )( HANDLE hSnapshot, LPHEAPENTRY32 lphe,
									DWORD th32ProcessID, DWORD th32HeapID );
typedef BOOL ( WINAPI *HEAPNEXT )( HANDLE hSnapshot, LPHEAPENTRY32 lphe );
typedef HANDLE ( WINAPI *CREATESNAPSHOT )( DWORD dwFlags, DWORD th32ProcessID );
typedef BOOL ( WINAPI *CLOSESNAPSHOT )( HANDLE hSnapshot );

/* Global function pointers. These are necessary because the functions need to
   be dynamically linked since only some WinCE builds contain them */

static CREATESNAPSHOT pCreateToolhelp32Snapshot = NULL;
static CLOSESNAPSHOT pCloseToolhelp32Snapshot = NULL;
static MODULEWALK pModule32First = NULL;
static MODULEWALK pModule32Next = NULL;
static PROCESSWALK pProcess32First = NULL;
static PROCESSWALK pProcess32Next = NULL;
static THREADWALK pThread32First = NULL;
static THREADWALK pThread32Next = NULL;
static HEAPLISTWALK pHeap32ListFirst = NULL;
static HEAPLISTWALK pHeap32ListNext = NULL;
static HEAPFIRST pHeap32First = NULL;
static HEAPNEXT pHeap32Next = NULL;

/* Since there are a significant number of ToolHelp data blocks, we use a
   larger-than-usual intermediate buffer to cut down on kernel traffic */

#define BIG_RANDOM_BUFSIZE	( RANDOM_BUFSIZE * 4 )

static void slowPollWinCE( void )
	{
	PROCESSENTRY32 pe32;
	THREADENTRY32 te32;
	MODULEENTRY32 me32;
	HEAPLIST32 hl32;
	HANDLE hSnapshot;
	RANDOM_STATE randomState;
	BYTE buffer[ BIG_RANDOM_BUFSIZE ];
	int iterationCount;

	/* Initialize the Toolhelp32 function pointers if necessary.  The
	   Toolhelp DLL isn't always present (some OEMs omit it) so we have to
	   link it dynamically */
	if( hToolHelp32 == NULL )
		{
		/* Obtain the module handle of the kernel to retrieve the addresses
		   of the ToolHelp32 functions */
		if( ( hToolHelp32 = LoadLibrary( TEXT( "Toolhelp.dll" ) ) ) == NULL )
			{
			/* There's no ToolHelp32 available, now we're in a bit of a
			   bind.  Try for at least a fast poll */
			fastPoll();
			return;
			}

		/* Now get pointers to the functions */
		pCreateToolhelp32Snapshot = ( CREATESNAPSHOT ) GetProcAddress( hToolHelp32, TEXT( "CreateToolhelp32Snapshot" ) );
		pCloseToolhelp32Snapshot = ( CLOSESNAPSHOT ) GetProcAddress( hToolHelp32, TEXT( "CloseToolhelp32Snapshot" ) );
		pModule32First = ( MODULEWALK ) GetProcAddress( hToolHelp32, TEXT( "Module32First" ) );
		pModule32Next = ( MODULEWALK ) GetProcAddress( hToolHelp32, TEXT( "Module32Next" ) );
		pProcess32First = ( PROCESSWALK ) GetProcAddress( hToolHelp32, TEXT( "Process32First" ) );
		pProcess32Next = ( PROCESSWALK ) GetProcAddress( hToolHelp32, TEXT( "Process32Next" ) );
		pThread32First = ( THREADWALK ) GetProcAddress( hToolHelp32, TEXT( "Thread32First" ) );
		pThread32Next = ( THREADWALK ) GetProcAddress( hToolHelp32, TEXT( "Thread32Next" ) );
		pHeap32ListFirst = ( HEAPLISTWALK ) GetProcAddress( hToolHelp32, TEXT( "Heap32ListFirst" ) );
		pHeap32ListNext = ( HEAPLISTWALK ) GetProcAddress( hToolHelp32, TEXT( "Heap32ListNext" ) );
		pHeap32First = ( HEAPFIRST ) GetProcAddress( hToolHelp32, TEXT( "Heap32First" ) );
		pHeap32Next = ( HEAPNEXT ) GetProcAddress( hToolHelp32, TEXT( "Heap32Next" ) );

		/* Make sure we got valid pointers for every Toolhelp32 function */
		if( pModule32First == NULL || pModule32Next == NULL || \
			pProcess32First == NULL || pProcess32Next == NULL || \
			pThread32First == NULL || pThread32Next == NULL || \
			pHeap32ListFirst == NULL || pHeap32ListNext == NULL || \
			pHeap32First == NULL || pHeap32Next == NULL || \
			pCreateToolhelp32Snapshot == NULL )
			{
			/* Mark the main function as unavailable in case for future
			   reference */
			pCreateToolhelp32Snapshot = NULL;
			return;
			}
		}
	if( krnlIsExiting() )
		return;

	initRandomData( randomState, buffer, BIG_RANDOM_BUFSIZE );

	/* Take snapshots what's currently in the system.  In theory we could
	   do a TH32CS_SNAPALL to get everything at once, but this can lead
	   to out-of-memory errors on some memory-limited systems, so we only
	   snapshot the individual resource that we're interested in.

	   First we walk through the local heap.  We have to be careful to not
	   spend excessive amounts of time on this if we're linked into a large
	   application with a great many heaps and/or heap blocks, since the
	   heap-traversal functions are rather slow.  Fortunately this is
	   quite rare under WinCE since it implies a large/long-running server
	   app, which we're unlikely to run into.

	   Ideally in order to prevent excessive delays we'd count the number
	   of heaps and ensure that no_heaps * no_heap_blocks doesn't exceed
	   some maximum value, however this requires two passes of (slow) heap
	   traversal rather than one, which doesn't help the situation much.
	   To provide at least some protection, we limit the total number of
	   heaps and heap entries traversed, although this leads to slightly
	   suboptimal performance if we have a small number of deep heaps
	   rather than the current large number of shallow heaps.

	   There is however a second consideration that needs to be taken into
	   account when doing this, which is that the heap-management functions
	   aren't completely thread-safe, so that under (very rare) conditions
	   of heavy allocation/deallocation this can cause problems when calling
	   HeapNext().  By limiting the amount of time that we spend in each
	   heap, we can reduce our exposure somewhat */
	hSnapshot = pCreateToolhelp32Snapshot( TH32CS_SNAPHEAPLIST, 0 );
	if( hSnapshot == INVALID_HANDLE_VALUE )
		{
		assert( DEBUG_WARN );	/* Make sure that we get some feedback */
		return;
		}
	hl32.dwSize = sizeof( HEAPLIST32 );
	if( pHeap32ListFirst( hSnapshot, &hl32 ) )
		{
		int listCount = 0;

		do
			{
			HEAPENTRY32 he32;

			/* First add the information from the basic Heaplist32
			   structure */
			if( krnlIsExiting() )
				{
				pCloseToolhelp32Snapshot( hSnapshot );
				return;
				}
			addRandomData( randomState, &hl32, sizeof( HEAPLIST32 ) );

			/* Now walk through the heap blocks getting information
			   on each of them */
			he32.dwSize = sizeof( HEAPENTRY32 );
			if( pHeap32First( hSnapshot, &he32, hl32.th32ProcessID, hl32.th32HeapID ) )
				{
				int entryCount = 0;

				do
					{
					if( krnlIsExiting() )
						{
						pCloseToolhelp32Snapshot( hSnapshot );
						return;
						}
					addRandomData( randomState, &he32,
								   sizeof( HEAPENTRY32 ) );
					}
				while( entryCount++ < 20 && pHeap32Next( hSnapshot, &he32 ) );
				}
			}
		while( listCount++ < 20 && pHeap32ListNext( hSnapshot, &hl32 ) );
		}
	pCloseToolhelp32Snapshot( hSnapshot );
	if( krnlIsExiting() )
		return;

	/* Now walk through all processes */
	hSnapshot = pCreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( hSnapshot == INVALID_HANDLE_VALUE )
		{
		endRandomData( randomState, 40 );
		return;
		}
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	iterationCount = 0;
	if( pProcess32First( hSnapshot, &pe32 ) )
		{
		do
			{
			if( krnlIsExiting() )
				{
				pCloseToolhelp32Snapshot( hSnapshot );
				return;
				}
			addRandomData( randomState, &pe32, sizeof( PROCESSENTRY32 ) );
			}
		while( pProcess32Next( hSnapshot, &pe32 ) && \
			   iterationCount++ < FAILSAFE_ITERATIONS_LARGE );
		}
	pCloseToolhelp32Snapshot( hSnapshot );
	if( krnlIsExiting() )
		return;

	/* Then walk through all threads */
	hSnapshot = pCreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if( hSnapshot == INVALID_HANDLE_VALUE )
		{
		endRandomData( randomState, 60 );
		return;
		}
	te32.dwSize = sizeof( THREADENTRY32 );
	iterationCount = 0;
	if( pThread32First( hSnapshot, &te32 ) )
		{
		do
			{
			if( krnlIsExiting() )
				{
				pCloseToolhelp32Snapshot( hSnapshot );
				return;
				}
			addRandomData( randomState, &te32, sizeof( THREADENTRY32 ) );
			}
		while( pThread32Next( hSnapshot, &te32 ) && \
			   iterationCount++ < FAILSAFE_ITERATIONS_LARGE  );
		}
	pCloseToolhelp32Snapshot( hSnapshot );
	if( krnlIsExiting() )
		return;

	/* Finally, walk through all modules associated with the process */
	hSnapshot = pCreateToolhelp32Snapshot( TH32CS_SNAPMODULE, 0 );
	if( hSnapshot == INVALID_HANDLE_VALUE )
		{
		endRandomData( randomState, 80 );
		return;
		}
	me32.dwSize = sizeof( MODULEENTRY32 );
	iterationCount = 0;
	if( pModule32First( hSnapshot, &me32 ) )
		{
		do
			{
			if( krnlIsExiting() )
				{
				pCloseToolhelp32Snapshot( hSnapshot );
				return;
				}
			addRandomData( randomState, &me32, sizeof( MODULEENTRY32 ) );
			}
		while( pModule32Next( hSnapshot, &me32 ) && \
			   iterationCount++ < FAILSAFE_ITERATIONS_LARGE  );
		}
	pCloseToolhelp32Snapshot( hSnapshot );
	if( krnlIsExiting() )
		return;

	/* Flush any remaining data through */
	endRandomData( randomState, 100 );
	}

/* Perform a thread-safe slow poll for Windows CE */

DWORD WINAPI threadSafeSlowPoll( void *dummy )
	{
	UNUSED_ARG( dummy );

	slowPollWinCE();
	ExitThread( 0 );
	return( 0 );
	}

/* Perform a generic slow poll.  This starts the OS-specific poll in a
   separate thread */

void slowPoll( void )
	{
	if( krnlIsExiting() )
		return;

	/* Start a threaded slow poll.  If a slow poll is already running, we
	   just return since there isn't much point in running two of them at the
	   same time */
	if( hThread )
		return;
	hThread = CreateThread( NULL, 0, threadSafeSlowPoll, NULL, 0, &threadID );
	assert( hThread );
	}

/* Wait for the randomness gathering to finish.  Anything that requires the
   gatherer process to have completed gathering entropy should call
   waitforRandomCompletion(), which will block until the background process
   completes */

int waitforRandomCompletion( const BOOLEAN force )
	{
	DWORD dwResult;
	const DWORD timeout = force ? 2000 : 300000L;
	int status;

	/* If there's no polling thread running, there's nothing to do.  Note
	   that this isn't entirely thread-safe because someone may start
	   another poll after we perform this check, but there's no way to
	   handle this without some form of interlock mechanism with the
	   randomness mutex and the WaitForSingleObject().  In any case all
	   that'll happen is that the caller won't get all of the currently-
	   polling entropy */
	if( hThread == NULL )
		return( CRYPT_OK );

	/* Wait for the polling thread to terminate.  If it's a forced shutdown
	   we only wait a short amount of time (2s) before we bail out, 
	   otherwise we hang around for as long as it takes (with a sanity-check
	   upper limit of 5 minutes) */
	dwResult = WaitForSingleObject( hThread, timeout );
	if( dwResult == WAIT_FAILED )
		{
		/* Since this is a cleanup function there's not much that we can do 
		   at this point, although we warn in debug mode */
		assert( DEBUG_WARN );
		return( CRYPT_OK );
		}
	assert( dwResult != WAIT_FAILED );	/* Warn in debug mode */

	/* Clean up */
	status = krnlEnterMutex( MUTEX_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	if( hThread != NULL )
		{
		CloseHandle( hThread );
		hThread = NULL;
		}
	krnlExitMutex( MUTEX_RANDOM );

	return( CRYPT_OK );
	}

/* Initialise and clean up any auxiliary randomness-related objects */

void initRandomPolling( void )
	{
	/* Reset the various object handles and status info */
	hToolHelp32 = hThread = NULL;
	}

void endRandomPolling( void )
	{
	assert( hThread == NULL );
	if( hToolHelp32 )
		{
		FreeLibrary( hToolHelp32 );
		hToolHelp32 = NULL;
		}
	}
