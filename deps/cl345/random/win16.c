/****************************************************************************
*																			*
*						Win16 Randomness-Gathering Code						*
*						Copyright Peter Gutmann 1996-2006					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#include "crypt.h"

/* OS-specific includes */

#include <stress.h>
#include <toolhelp.h>

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	1024

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	SYSHEAPINFO sysHeapInfo;
	MEMMANINFO memManInfo;
	TIMERINFO timerInfo;
	POINT point;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get various basic pieces of system information: Handle of the window
	   with mouse capture, handle of window with input focus, amount of
	   space in global heap, whether system queue has any events, cursor
	   position for last message, 55 ms time for last message, number of
	   active tasks, 55 ms time since Windows started, current mouse cursor
	   position, current caret position */
	addRandomValue( randomState, GetCapture() );
	addRandomValue( randomState, GetFocus() );
	addRandomValue( randomState, GetFreeSpace( 0 ) );
	addRandomValue( randomState, GetInputState() );
	addRandomValue( randomState, GetMessagePos() );
	addRandomValue( randomState, GetMessageTime() );
	addRandomValue( randomState, GetNumTasks() );
	addRandomValue( randomState, GetTickCount() );
	GetCursorPos( &point );
	addRandomData( randomState, &point, sizeof( POINT ) );
	GetCaretPos( &point );
	addRandomData( randomState, &point, sizeof( POINT ) );

	/* Get the largest free memory block, number of lockable pages, number of
	   unlocked pages, number of free and used pages, and number of swapped
	   pages */
	memManInfo.dwSize = sizeof( MEMMANINFO );
	MemManInfo( &memManInfo );
	addRandomData( randomState, &memManInfo, sizeof( MEMMANINFO ) );

	/* Get the execution times of the current task and VM to approximately
	   1ms resolution */
	timerInfo.dwSize = sizeof( TIMERINFO );
	TimerCount( &timerInfo );
	addRandomData( randomState, &timerInfo, sizeof( TIMERINFO ) );

	/* Get the percentage free and segment of the user and GDI heap */
	sysHeapInfo.dwSize = sizeof( SYSHEAPINFO );
	SystemHeapInfo( &sysHeapInfo );
	addRandomData( randomState, &sysHeapInfo, sizeof( SYSHEAPINFO ) );

	/* Flush any remaining data through */
	endRandomData( randomState, 25 );
	}

/* The slow poll can get *very* slow because of the overhead involved in
   obtaining the necessary information.  On a moderately loaded system there
   are often 500+ objects on the global heap and 50+ modules, so we limit
   the number checked to a reasonable level to make sure we don't spend
   forever polling.  We give the global heap walk the most leeway since this
   provides the best source of randomness */

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	MODULEENTRY moduleEntry;
	GLOBALENTRY globalEntry;
	TASKENTRY taskEntry;
	int count;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Walk the global heap getting information on each entry in it.  This
	   retrieves the objects linear address, size, handle, lock count, owner,
	   object type, and segment type */
	count = 0;
	globalEntry.dwSize = sizeof( GLOBALENTRY );
	if( GlobalFirst( &globalEntry, GLOBAL_ALL ) )
		do
			{
			addRandomData( randomState, &globalEntry, sizeof( GLOBALENTRY ) );
			count++;
			}
		while( count < 70 && GlobalNext( &globalEntry, GLOBAL_ALL ) );

	/* Walk the module list getting information on each entry in it.  This
	   retrieves the module name, handle, reference count, executable path,
	   and next module */
	count = 0;
	moduleEntry.dwSize = sizeof( MODULEENTRY );
	if( ModuleFirst( &moduleEntry ) )
		do
			{
			addRandomData( randomState, &moduleEntry, sizeof( MODULEENTRY ) );
			count++;
			}
		while( count < 20 && ModuleNext( &moduleEntry ) );

	/* Walk the task list getting information on each entry in it.  This
	   retrieves the task handle, parent task handle, instance handle, stack
	   segment and offset, stack size, number of pending events, task queue,
	   and the name of module executing the task.  We also call TaskGetCSIP()
	   for the code segment and offset of each task if it's safe to do so
	   (note that this call can cause odd things to happen in debuggers and
	   runtime code checkers because of the way TaskGetCSIP() is implemented) */
	count = 0;
	taskEntry.dwSize = sizeof( TASKENTRY );
	if( TaskFirst( &taskEntry ) )
		do
			{
			addRandomData( randomState, &taskEntry, sizeof( TASKENTRY ) );
			if( taskEntry.hTask != GetCurrentTask() )
				addRandomValue( randomState,
								TaskGetCSIP( taskEntry.hTask ) );
			count++;
			}
		while( count < 100 && TaskNext( &taskEntry ) );

	/* Flush any remaining data through */
	endRandomData( randomState, 100 );
	}
