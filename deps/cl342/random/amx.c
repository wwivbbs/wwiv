/****************************************************************************
*																			*
*							AMX Randomness-Gathering Code					*
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

#include <cjzzz.h>
#include "crypt.h"

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	addRandomLong( randomState, cjtmtick() );
	endRandomData( randomState, 1 );
	}

void slowPoll( void )
	{
	return;
	}
