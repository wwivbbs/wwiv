/****************************************************************************
*																			*
*						VxWorks Randomness-Gathering Code					*
*						Copyright Peter Gutmann 1996-2011					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   VxWorks provides very few entropy sources, the code below provides very 
   little usable entropy and should not be relied upon as the sole entropy 
   source for the system but should be augmented with randomness from 
   external hardware sources */

/* General includes */

#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */

#include <vxWorks.h>
#include <intLib.h>
#include <sysLib.h>
#include <taskLib.h>
#include <tickLib.h>
#include <time.h>

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	1024

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	REG_SET registerSet;
	struct timespec timeSpec;
	ULONG tickCount;
	int value, status;

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	if( cryptStatusError( status ) )
		retIntError_Void();

	/* Add various clock/timer values.  These are both very fast clocks/
	   counters, however the difference over subsequent calls is only
	   2-3 bits in the LSB */
	tickCount = tickGet();
	addRandomLong( randomState, tickCount );
	status = clock_gettime( CLOCK_REALTIME, &timeSpec );
	if( status == 0 )
		addRandomData( randomState, &timeSpec, sizeof( struct timespec ) );

	/* Add the interrupt nesting depth (usually 0) */
	value = intCount();
	addRandomLong( randomState, value );

	/* Add the current task's registers.  The documentation states that 
	   "self-examination is not advisable as results are unpredictable",
	   which is either good (the registers are random garbage values) or
	   bad (the registers have fixed values).  Usually they seem to be 
	   pretty fixed, at least when called repeatedly in rapid succession */
	status = taskRegsGet( taskIdSelf(), &registerSet );
	if( status == OK )
		addRandomData( randomState, &registerSet, sizeof( REG_SET ) );

	endRandomData( randomState, 5 );
	}

void slowPoll( void )
	{
	static BOOLEAN addedFixedItems = FALSE;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	int taskID, value, status;

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	if( cryptStatusError( status ) )
		retIntError_Void();

	/* The following are fixed for the lifetime of the process (and in fact 
	   for the BSP as a whole) so we only add them once */
	if( !addedFixedItems )
		{
		const char *string;
		int value;

		/* Add the model name of the CPU board and the BSP version and 
		   revision number */
		string = sysModel();
		if( string != NULL )
			addRandomData( randomState, string, strlen( string ) );
		string = sysBspRev();
		if( string != NULL )
			addRandomData( randomState, string, strlen( string ) );
		value = sysProcNumGet();	/* Usually 0 */
		addRandomLong( randomState, value );
		}

	/* Add the current task ID and task options.  The task options are 
	   relatively fixed but the task ID seems quite random and over the full 
	   32-bit range */
	taskID = taskIdSelf();
	addRandomLong( randomState, taskID );
	status = taskOptionsGet( taskID, &value );
	if( status == OK )
		addRandomLong( randomState, value );

	endRandomData( randomState, 3 );

	fastPoll();
	}
