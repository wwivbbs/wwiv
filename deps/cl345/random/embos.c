/****************************************************************************
*																			*
*						  embOS Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1996-2015					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide randomness via an external source.  In its
   current form it does not provide any usable entropy and should not be
   used as an entropy source */

/* General includes */

#include <RTOS.h>
#include "crypt.h"
#include "random/random.h"
#ifdef USE_TCP
  #include <IP.h>
#endif /* USE_TCP */

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Add the current time in microseconds */
	addRandomLong( randomState, OS_GetTime_us() );

	/* Add the task control block containing the current tasks's state 
	   information, and a mask of signalled task events */
	addRandomData( randomState, OS_GetTaskID(), sizeof( OS_TASK ) );
	addRandomLong( randomState, OS_GetEventsOccurred( NULL ) );

	endRandomData( randomState, 20 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	static BOOLEAN addedStaticData = FALSE;
#ifdef USE_TCP
	int interface;
#endif /* USE_TCP */

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get static system information */
	if( !addedStaticData )
		{
		const char *string;

		string = OS_GetCPU();
		addRandomData( randomState, string, strlen( string ) );
		string = OS_GetLibMode();
	 	addRandomData( randomState, string, strlen( string ) );
		string = OS_GetModel();
		addRandomData( randomState, string, strlen( string ) );
		string = OS_GetLibName();
		addRandomData( randomState, string, strlen( string ) );
		addRandomLong( randomState, OS_GetVersion() );
#ifdef USE_TCP
		addRandomLong( randomState, IP_GetVersion() );
#endif /* USE_TCP */

		addedStaticData = TRUE;
		}

	/* Get a bit mask of active devices */
	addRandomLong( randomState, OS_POWER_GetMask() );

	/* Get the task, system, and interrupt stack address, total size, and 
	   unused space */
 	addRandomData( randomState, OS_GetStackBase( NULL ), 
				   sizeof( void * ) );
	addRandomLong( randomState, OS_GetStackSize( NULL ) );
	addRandomLong( randomState, OS_GetStackSpace( NULL ) );
 	addRandomData( randomState, OS_GetSysStackBase(), 
				   sizeof( void * ) );
	addRandomLong( randomState, OS_GetSysStackSize() );
	addRandomLong( randomState, OS_GetSysStackSpace() );
 	addRandomData( randomState, OS_GetIntStackBase(), 
				   sizeof( void * ) );
	addRandomLong( randomState, OS_GetIntStackSize() );
	addRandomLong( randomState, OS_GetIntStackSpace() );

	/* Add networking-related data */
#ifdef USE_TCP
	interface = IP_GetPrimaryIFace();
	addRandomLong( randomState, IP_GetCurrentLinkSpeed() );
	addRandomLong( randomState, IP_GetGWAddr( ( BYTE ) interface ) );
	addRandomLong( randomState, IP_GetIPAddr( ( BYTE ) interface ) );
	addRandomLong( randomState, IP_DNS_GetServer() );
	addRandomLong( randomState, IP_TCP_GetMTU( ( BYTE ) interface ) );
#endif /* USE_TCP */

	endRandomData( randomState, 20 );
	}
