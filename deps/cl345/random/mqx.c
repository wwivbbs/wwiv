/****************************************************************************
*																			*
*							MQX Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide randomness via an FPGA hardware entropy
   source such as a standard ring oscillator generator.  In its current
   form it does not provide any usable entropy and should not be used as an
   entropy source */

/* General includes */

#ifdef __IAR_SYSTEMS_ICC__ 
  #include <time.h>			/* IAR and MQX time definitions conflict */
#endif /* IAR compiler */
#include <mqx.h>
#undef FALSE				/* MQX has its own broken definition */
#include "crypt.h"
#include "random/random.h"

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

/* OS-specific includes */

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Add the number of elapsed hardware ticks and the number of 
	   microseconds and nanoseconds since the last interrupt */
	addRandomLong( randomState, _time_get_hwticks() );
	addRandomLong( randomState, _time_get_microseconds() );
	addRandomLong( randomState, _time_get_nanoseconds() );
	endRandomData( randomState, 10 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];

	fastPoll();

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Add some sort of supposedly unique number, possibly a CPUID, the
	   task ID of the system task, available scheduling priorities, the
	   current task's creator, the task error code, ID, and creation
	   parameter */
	addRandomLong( randomState, _mqx_get_counter() );
	addRandomLong( randomState, _mqx_get_system_task_id() );
	addRandomLong( randomState, _sched_get_max_priority( 0 ) );
	addRandomLong( randomState, _sched_get_min_priority( 0 ) );
	addRandomLong( randomState, _task_get_creator() );
	addRandomLong( randomState, _task_get_error() );
	addRandomLong( randomState, _task_get_id() );
	addRandomLong( randomState, _task_get_parameter() );
	endRandomData( randomState, 5 );

	fastPoll();
	}
