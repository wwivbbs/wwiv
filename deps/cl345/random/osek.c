/****************************************************************************
*																			*
*						  OSEK Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1996-2005					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide randomness via an external source.  In its
   current form it does not provide any usable entropy and should not be
   used as an entropy source */

/* General includes */

#include <os.h>
#include "crypt.h"
#include "random/random.h"

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];

	/* Get the core that we're currently running on */
	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	addRandomValue( randomState, GetCoreID() );
	endRandomData( randomState, 1 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	static BOOLEAN addedStaticData = FALSE;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get static system information */
	if( !addedStaticData )
		{
		/* The application ID is fixed at compile time, the current 
		   application ID is almost always the same thing */ 
		addRandomValue( randomState, GetApplicationID() );
		addRandomValue( randomState, GetCurrentApplicationID() );
		addRandomValue( randomState, GetNumberOfActivatedCores() );

		addedStaticData = TRUE;
		}

	endRandomData( randomState, 1 );
	}
